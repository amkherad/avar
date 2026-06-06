#ifndef AVAR_DOWNLOAD_H
#define AVAR_DOWNLOAD_H
#include <stdbool.h>
#include <stdlib.h>

int transient_download(const char *url, const char *queue, const char *name, bool detached);

#endif
