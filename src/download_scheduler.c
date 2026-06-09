

static size_t next_chunk_index(const DownloadJob *job) {
    if (job->state == NULL || job->state->chunks_done == NULL) {
        return 0;
    }

    for (size_t i = 0; i < job->state->chunk_count; i++) {
        if (!job->state->chunks_done[i]) {
            return i;
        }
    }
    return job->state->chunk_count;
}


static void send_request(DownloadJob *job, struct mg_connection *c, const uint64_t range_start,
                         const uint64_t range_end) {
    const struct mg_str host = mg_url_host(job->current_url);
    const char *uri = mg_url_uri(job->current_url);

    LOG_DEBUG("Sending http request to %s", job->current_url);

    mg_printf(c,
              "GET %s HTTP/1.1\r\n"
              "Host: %.*s\r\n"
              "User-Agent: " APP_NAME "/" VERSION_STR "\r\n"
              "Accept: */*\r\n"
              "Accept-Encoding: identity\r\n"
              "Connection: close\r\n",
              uri, (int) host.len, host.buf);

    if (job->state != NULL && job->state->etag != NULL) {
        mg_printf(c, "If-Range: %s\r\n", job->state->etag);
    } else if (job->state != NULL && job->state->last_modified != NULL) {
        mg_printf(c, "If-Range: %s\r\n", job->state->last_modified);
    }

    if (job->step == DL_STEP_CHUNK) {
        mg_printf(c, "Range: bytes=%llu-%llu\r\n", (unsigned long long) range_start,
                  (unsigned long long) range_end);
    } else if (job->step == DL_STEP_STREAM && range_start > 0) {
        mg_printf(c, "Range: bytes=%llu-\r\n", (unsigned long long) range_start);
    }

    mg_printf(c, "\r\n");
}

static void send_request_for_step(DownloadJob *job, struct mg_connection *c) {
    if (c->data[0] != '\0') {
        return;
    }

    c->data[0] = 1;
    job->request_sent = true;

    if (job->step == DL_STEP_CHUNK) {
        const uint64_t start = (uint64_t) job->active_chunk * (uint64_t) job->state->chunk_size;
        uint64_t end = start + (uint64_t) job->state->chunk_size - 1;
        if (job->state->total_size > 0 && end >= job->state->total_size) {
            end = job->state->total_size - 1;
        }
        send_request(job, c, start, end);
    } else if (job->step == DL_STEP_STREAM) {
        send_request(job, c, job->stream_received, 0);
    } else {
        send_request(job, c, 0, 0);
    }
}


static bool write_at_offset(DownloadJob *job, const void *data, const size_t len,
                            const uint64_t offset) {
    if (job->fp == NULL) {
        if (!open_temp_file(job, "r+b") && !open_temp_file(job, "w+b")) {
            set_error(job, "Failed to open temp file: %s", job->temp_path);
            return false;
        }
    }

#if defined(_WIN32)
    if (_fseeki64(job->fp, (__int64) offset, SEEK_SET) != 0) {
#else
        if (fseeko(job->fp, (off_t) offset, SEEK_SET) != 0) {
#endif
        set_error(job, "Failed to seek in temp file");
        return false;
    }

    if (fwrite(data, 1, len, job->fp) != len) {
        set_error(job, "Failed to write temp file");
        return false;
    }

    fflush(job->fp);
    job->last_activity_ms = mg_millis();
    return true;
}

static bool append_stream(DownloadJob *job, const void *data, const size_t len) {
    if (len == 0) {
        return true;
    }

    if (job->fp == NULL) {
        const char *mode = job->stream_received > 0 ? "ab" : "wb";
        if (!open_temp_file(job, mode)) {
            set_error(job, "Failed to open temp file: %s", job->temp_path);
            return false;
        }
    }

    if (fwrite(data, 1, len, job->fp) != len) {
        set_error(job, "Failed to write temp file");
        return false;
    }

    fflush(job->fp);
    job->stream_received += len;
    job->last_activity_ms = mg_millis();
    return true;
}

static void begin_stream_body(DownloadJob *job, struct mg_connection *c,
                              struct mg_http_message *hm) {
    job->streaming = true;
    c->pfn = NULL;

    if (hm->body.len > 0) {
        if (!append_stream(job, hm->body.buf, hm->body.len)) {
            c->is_draining = 1;
            return;
        }
    }

    mg_iobuf_del(&c->recv, 0, hm->head.len + hm->body.len);
    print_progress(job);
    (void) dm_item_upsert(job, "downloading");
}

static bool store_header_value(char **dest, const struct mg_str value) {
    if (dest == NULL || value.buf == NULL || value.len == 0) {
        return true;
    }

    char *copy = malloc(value.len + 1);
    if (copy == NULL) {
        return false;
    }
    memcpy(copy, value.buf, value.len);
    copy[value.len] = '\0';
    free(*dest);
    *dest = copy;
    return true;
}

static bool parse_content_range_total(const struct mg_str value, uint64_t *total_out) {
    if (total_out == NULL || value.buf == NULL) {
        return false;
    }

    const char *slash = memchr(value.buf, '/', value.len);
    if (slash == NULL) {
        return false;
    }

    const char *total_start = slash + 1;
    const size_t total_len = value.len - (size_t) (total_start - value.buf);
    char buffer[32];
    if (total_len >= sizeof buffer) {
        return false;
    }

    memcpy(buffer, total_start, total_len);
    buffer[total_len] = '\0';

    if (strcmp(buffer, "*") == 0) {
        return false;
    }

    char *end = NULL;
    const unsigned long long total = strtoull(buffer, &end, 10);
    if (end == buffer) {
        return false;
    }

    *total_out = (uint64_t) total;
    return true;
}

static bool finalize_download(DownloadJob *job) {
    LOG_DEBUG("Finalizing download, jobId: %s", job->item_id);

    if (job->done) {
        return !job->failed;
    }

    close_file(job);

    if (job->state != NULL && job->state->total_size > 0) {
        const uint64_t done = job->step == DL_STEP_STREAM
                                  ? job->stream_received
                                  : download_state_bytes_done(job->state);
        if (done < job->state->total_size) {
            set_error(job, "Download incomplete (%llu / %llu bytes)",
                      (unsigned long long) done, (unsigned long long) job->state->total_size);
            return false;
        }
    }

    const char *last_sep = strrchr(job->dest_path, PATH_SEPARATOR);
    if (last_sep != NULL) {
        char *dir = strndup(job->dest_path, (size_t) (last_sep - job->dest_path));
        if (dir != NULL) {
            (void) make_dirs_in_path(dir);
            free(dir);
        }
    }

    if (move_file_atomic(job->temp_path, job->dest_path) != 0) {
        set_error(job, "Failed to move file to %s", job->dest_path);
        return false;
    }

    remove(job->state_path);
    (void) dm_item_upsert(job, "completed");
    print_progress(job);
    job->done = true;
    return true;
}

static void request_close(DownloadJob *job, struct mg_connection *c, const bool schedule_after_close) {
    job->pending_schedule = schedule_after_close;
    c->is_draining = 1;
}

static void flush_recv_stream(DownloadJob *job, struct mg_connection *c) {
    if (job == NULL || c == NULL || !job->streaming || c->recv.len == 0) {
        return;
    }

    if (!append_stream(job, c->recv.buf, c->recv.len)) {
        return;
    }

    c->recv.len = 0;
}

static bool try_parse_pending_response(DownloadJob *job, struct mg_connection *c) {
    if (job->headers_received || c == NULL || c->recv.len == 0) {
        return false;
    }

    struct mg_http_message hm;
    const int n = mg_http_parse((char *) c->recv.buf, c->recv.len, &hm);
    if (n <= 0) {
        return false;
    }

    job->headers_received = true;
    handle_response_headers(job, c, &hm);
    return true;
}

static void on_connection_closed(DownloadJob *job, struct mg_connection *c) {
    if (job == NULL || job->done || job->failed) {
        return;
    }

    LOG_DEBUG("Connection closed, step=%d streaming=%d pending_schedule=%d headers_received=%d",
              (int) job->step, job->streaming, job->pending_schedule, job->headers_received);

    if (c != NULL && !job->headers_received) {
        (void) try_parse_pending_response(job, c);
    }

    flush_recv_stream(job, c);

    if (job->pending_schedule) {
        job->pending_schedule = false;
        schedule_next(job);
        return;
    }

    if (job->step == DL_STEP_STREAM && (job->streaming || job->stream_received > 0)) {
        if (job->stream_expected == 0 || job->stream_received >= job->stream_expected) {
            (void) finalize_download(job);
        } else {
            set_error(job, "Connection closed before download completed (%llu / %llu bytes)",
                      (unsigned long long) job->stream_received,
                      (unsigned long long) job->stream_expected);
            (void) dm_item_upsert(job, "failed");
        }
        return;
    }

    if (job->step == DL_STEP_CHUNK) {
        if (download_state_all_chunks_done(job->state)) {
            (void) finalize_download(job);
        } else {
            set_error(job, "Connection closed before all chunks were downloaded");
            (void) dm_item_upsert(job, "failed");
        }
        return;
    }

    if (!job->request_sent) {
        set_error(job, "Failed to send download request");
    } else {
        set_error(job, "Connection closed before the download started");
    }
    (void) dm_item_upsert(job, "failed");
}

static void schedule_next(DownloadJob *job) {
    LOG_DEBUG("Scheduling a job, jobId: %s", job->item_id);

    if (job->failed || job->done) {
        return;
    }

    if (job->step == DL_STEP_CHUNK) {
        const size_t chunk = next_chunk_index(job);
        if (chunk >= job->state->chunk_count) {
            (void) finalize_download(job);
            return;
        }
        job->active_chunk = chunk;
    }

    struct mg_connection *c = mg_http_connect(&job->mgr, job->current_url, dl_handler, job);
    if (c == NULL) {
        set_error(job, "Failed to connect to %s", job->current_url);
    }
}
