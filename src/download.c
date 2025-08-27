/*
 * TinyPkg - Download System Implementation
 * Simple HTTP/HTTPS download functionality using external tools
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../include/tinypkg.h"

// Global state
static int download_initialized = 0;

// System initialization
int download_init(void) {
    if (download_initialized) {
        return TINYPKG_SUCCESS;
    }
    
    log_debug("Initializing download system");
    
    // Check if wget or curl is available
    int has_wget = (system("which wget >/dev/null 2>&1") == 0);
    int has_curl = (system("which curl >/dev/null 2>&1") == 0);
    
    if (!has_wget && !has_curl) {
        log_error("Neither wget nor curl found. Please install one of them.");
        return TINYPKG_ERROR;
    }
    
    if (has_wget) {
        log_debug("Using wget for downloads");
    } else {
        log_debug("Using curl for downloads");
    }
    
    download_initialized = 1;
    return TINYPKG_SUCCESS;
}

void download_cleanup(void) {
    if (download_initialized) {
        log_debug("Cleaning up download system");
        download_initialized = 0;
    }
}

// Basic download function
int download_file(const char *url, const char *dest_path) {
    if (!url || !dest_path) {
        return TINYPKG_ERROR;
    }
    
    if (!download_initialized) {
        if (download_init() != TINYPKG_SUCCESS) {
            return TINYPKG_ERROR;
        }
    }
    
    log_info("Downloading: %s", url);
    log_debug("Destination: %s", dest_path);
    
    // Create destination directory if needed
    char *dest_dir = utils_get_dirname(dest_path);
    if (dest_dir) {
        utils_create_directory_recursive(dest_dir);
        TINYPKG_FREE(dest_dir);
    }
    
    // Try wget first, then curl
    char cmd[MAX_CMD];
    int result;
    
    // Try wget
    if (system("which wget >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), 
                 "wget --no-check-certificate --timeout=30 --tries=3 -O '%s' '%s'",
                 dest_path, url);
        
        log_debug("Download command: %s", cmd);
        result = utils_run_command(cmd, NULL);
        
        if (result == TINYPKG_SUCCESS) {
            log_info("Download completed successfully");
            return TINYPKG_SUCCESS;
        }
    }
    
    // Try curl as fallback
    if (system("which curl >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), 
                 "curl -k --connect-timeout 30 --max-time 300 --retry 3 -L -o '%s' '%s'",
                 dest_path, url);
        
        log_debug("Download command (fallback): %s", cmd);
        result = utils_run_command(cmd, NULL);
        
        if (result == TINYPKG_SUCCESS) {
            log_info("Download completed successfully");
            return TINYPKG_SUCCESS;
        }
    }
    
    log_error("Download failed: %s", url);
    
    // Remove partial file if it exists
    if (utils_file_exists(dest_path)) {
        unlink(dest_path);
    }
    
    return TINYPKG_ERROR_NETWORK;
}

// Download with progress callback
int download_file_with_progress(const char *url, const char *dest_path,
                               download_progress_callback_t callback, void *data) {
    // For this simple implementation, we'll just call the basic download
    // and simulate progress. In a full implementation, this would use libcurl
    // for real progress reporting.
    
    if (callback) {
        callback(data, 0, 0, 0, 0); // Start
    }
    
    int result = download_file(url, dest_path);
    
    if (callback) {
        if (result == TINYPKG_SUCCESS) {
            struct stat st;
            if (stat(dest_path, &st) == 0) {
                callback(data, st.st_size, st.st_size, 0, 0); // Complete
            }
        }
    }
    
    return result;
}

// Download context management
download_context_t *download_context_create(const char *url, const char *dest_path) {
    if (!url || !dest_path) {
        return NULL;
    }
    
    download_context_t *ctx = TINYPKG_CALLOC(1, sizeof(download_context_t));
    if (!ctx) {
        return NULL;
    }
    
    strncpy(ctx->url, url, sizeof(ctx->url) - 1);
    strncpy(ctx->dest_path, dest_path, sizeof(ctx->dest_path) - 1);
    ctx->status = DOWNLOAD_STATUS_INIT;
    ctx->start_time = time(NULL);
    
    return ctx;
}

void download_context_free(download_context_t *ctx) {
    if (ctx) {
        TINYPKG_FREE(ctx);
    }
}

int download_execute(download_context_t *ctx) {
    if (!ctx) {
        return TINYPKG_ERROR;
    }
    
    ctx->status = DOWNLOAD_STATUS_CONNECTING;
    
    int result = download_file_with_progress(ctx->url, ctx->dest_path,
                                           ctx->progress_callback, ctx->progress_data);
    
    if (result == TINYPKG_SUCCESS) {
        ctx->status = DOWNLOAD_STATUS_COMPLETE;
    } else {
        ctx->status = DOWNLOAD_STATUS_FAILED;
    }
    
    return result;
}

// Utility functions
const char *download_status_to_string(download_status_t status) {
    switch (status) {
        case DOWNLOAD_STATUS_INIT: return "Initializing";
        case DOWNLOAD_STATUS_CONNECTING: return "Connecting";
        case DOWNLOAD_STATUS_DOWNLOADING: return "Downloading";
        case DOWNLOAD_STATUS_COMPLETE: return "Complete";
        case DOWNLOAD_STATUS_FAILED: return "Failed";
        default: return "Unknown";
    }
}

int download_verify_url(const char *url) {
    if (!url || strlen(url) == 0) {
        return 0;
    }
    
    // Basic URL validation
    if (!utils_string_starts_with(url, "http://") && 
        !utils_string_starts_with(url, "https://") &&
        !utils_string_starts_with(url, "ftp://")) {
        return 0;
    }
    
    return 1;
}
