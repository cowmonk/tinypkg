/*
 * TinyPkg - Build System Header
 * Package building and installation functions
 */

#ifndef TINYPKG_BUILD_H
#define TINYPKG_BUILD_H

#include <sys/types.h>

// Build configuration
typedef struct build_config {
    int parallel_jobs;
    char build_flags[512];
    char install_prefix[256];
    int enable_optimizations;
    int debug_symbols;
    int keep_build_dir;
    int build_timeout;
} build_config_t;

// Build status
typedef enum {
    BUILD_STATUS_INIT = 0,
    BUILD_STATUS_DOWNLOADING = 1,
    BUILD_STATUS_EXTRACTING = 2,
    BUILD_STATUS_CONFIGURING = 3,
    BUILD_STATUS_BUILDING = 4,
    BUILD_STATUS_INSTALLING = 5,
    BUILD_STATUS_COMPLETE = 6,
    BUILD_STATUS_FAILED = -1
} build_status_t;

// Build context
typedef struct build_context {
    package_t *package;
    char build_dir[MAX_PATH];
    char source_dir[MAX_PATH];
    char install_dir[MAX_PATH];
    build_status_t status;
    time_t start_time;
    time_t end_time;
    pid_t build_pid;
} build_context_t;

// Function declarations

// Main build functions
int build_package(package_t *pkg);
int build_install_package(package_t *pkg);
int build_clean_package(const char *package_name);

// Build steps
int build_download_source(build_context_t *ctx);
int build_extract_source(build_context_t *ctx);
int build_configure_package(build_context_t *ctx);
int build_compile_package(build_context_t *ctx);
int build_install_files(build_context_t *ctx);

// Build context management
build_context_t *build_context_create(package_t *pkg);
void build_context_free(build_context_t *ctx);
int build_context_setup_directories(build_context_t *ctx);
int build_context_cleanup(build_context_t *ctx);

// Build system detection and handling
build_type_t build_detect_system(const char *source_dir);
int build_run_autotools(build_context_t *ctx);
int build_run_cmake(build_context_t *ctx);
int build_run_make(build_context_t *ctx);
int build_run_custom(build_context_t *ctx);

// Build utilities
const char *build_status_to_string(build_status_t status);
int build_get_progress(const char *package_name);
int build_is_running(const char *package_name);
int build_cancel(const char *package_name);

#endif /* TINYPKG_BUILD_H */
