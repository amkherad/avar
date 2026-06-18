#include "avar_test.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#include "mongoose_log.h"
#include "stream_hls.h"

AVAR_TEST(stream_hls_url_detection) {
    avar_mongoose_log_silence();
    AVAR_ASSERT(stream_url_is_hls("https://cdn.example.com/live/playlist.m3u8", NULL));
    AVAR_ASSERT(stream_url_is_hls("https://cdn.example.com/live/playlist.M3U8", NULL));
    AVAR_ASSERT(stream_url_is_hls("https://cdn.example.com/live/stream", "hls"));
    AVAR_ASSERT(stream_url_is_hls("https://cdn.example.com/live/stream", "HLS"));
    AVAR_ASSERT(!stream_url_is_hls("https://cdn.example.com/file.mp4", NULL));
    AVAR_ASSERT(!stream_url_is_hls(NULL, NULL));
}

AVAR_TEST(stream_hls_master_playlist_variant_selection) {
    const char *master =
            "#EXTM3U\n"
            "#EXT-X-STREAM-INF:BANDWIDTH=128000\n"
            "low/index.m3u8\n"
            "#EXT-X-STREAM-INF:BANDWIDTH=256000\n"
            "high/index.m3u8\n";

    AVAR_ASSERT(stream_hls_test_playlist_is_master(master));
    char *variant = stream_hls_test_pick_variant_url(master, "https://cdn.example.com/live/");
    AVAR_ASSERT_NOT_NULL(variant);
    AVAR_ASSERT(strstr(variant, "high/index.m3u8") != NULL);
    free(variant);
}

AVAR_TEST(stream_hls_media_playlist_parsing) {
    const char *media =
            "#EXTM3U\n"
            "#EXT-X-VERSION:3\n"
            "#EXT-X-TARGETDURATION:10\n"
            "#EXT-X-MEDIA-SEQUENCE:0\n"
            "#EXT-X-KEY:METHOD=AES-128,URI=\"keys/0.key\",IV=0x00000000000000000000000000000001\n"
            "seg-0.ts\n"
            "seg-1.ts\n";

    AVAR_ASSERT(!stream_hls_test_playlist_is_master(media));
    const size_t count =
            stream_hls_test_parse_media_segment_count(media, "https://cdn.example.com/vod/");
    AVAR_ASSERT_EQ(count, 2U);
}

AVAR_TEST(stream_hls_parse_hex_iv) {
    unsigned char iv[16] = {0};
    AVAR_ASSERT(stream_hls_test_parse_hex_iv("0x00000000000000000000000000000001", iv));
    AVAR_ASSERT_EQ(iv[15], 1);
    AVAR_ASSERT(!stream_hls_test_parse_hex_iv("bad", iv));
    AVAR_ASSERT(!stream_hls_test_parse_hex_iv(NULL, iv));
}

AVAR_TEST_MAIN(
        run_stream_hls_url_detection();
        run_stream_hls_master_playlist_variant_selection();
        run_stream_hls_media_playlist_parsing();
        run_stream_hls_parse_hex_iv();)
