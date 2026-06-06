#include <avar.h>
#include <download.h>

#include <stdio.h>

int transient_download(const char *url, const char *queue, const char *name, bool detached) {
    (void) name;
    (void) detached;
    printf("transient download: %s", url);
    if (queue != NULL) {
        printf(" (queue=%s)", queue);
    }
    puts("");
    return EXIT_SUCCESS;
}
