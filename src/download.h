/*
 * TinyPkg - Download System Header
 * HTTP/HTTPS download functionality
 */

#ifndef TINYPKG_DOWNLOAD_H
#define TINYPKG_DOWNLOAD_H

#include <sys/types.h>

// Download status
typedef enum {
    DOWNLOAD_STATUS_INIT = 0,
    DOWNLOAD_STATUS_CONNECTING = 1,
    DOWNLOAD_STATUS_DOWNLOADING = 2,
    DOWNLOAD_STATUS_COMPLETE = 3,
    DOWNLOAD_STATUS_FAILED = -1
} download_status_t;

// Download progress callback
typedef int (*download_progress_callback_t)(void *clientp, double dltotal, double dlnow, 
                                           double ultotal, double ulnow);

// Download context
typedef struct download_context {
    char url[MAX_URL];
    char dest_path[MAX_PATH];
    download_status_t status;
    size_t total_size;
    size_t downloaded_size;
    time_t start_time;
    double speed;
    download_progress_callback_t progress_callback;
    void *progress_data;
} download_context_t;

// Function declarations

// System initialization
int download_init(void);
void download_cleanup(void);

// Basic download functions
int download_file(const char *url, const char *dest_path);
int download_file_with_progress(const char *url, const char *dest_path,
                               download_progress_callback_t callback, void *data);

// Download context management
download_context_t *download_context_create(const char *url, const char *dest_path);
void download_context_free(download_context_t *ctx);
int download_execute(download_context_t *ctx);

// Utility functions
const char *download_status_to_string(download_status_t status);
int download_verify_url(const char *url);

#endif /* TINYPKG_DOWNLOAD_H */
