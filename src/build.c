/*
 * TinyPkg - Build System Implementation
 * Package building and installation functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include "../include/tinypkg.h"
#include "download.h"
#include "package.h"
#include "security.h"
#include "utils.h"


// Global build tracking
static build_context_t *active_builds[16] = {0};
static int active_build_count = 0;

// Helper functions
static int add_active_build(build_context_t *ctx) {
    if (active_build_count >= 16) return TINYPKG_ERROR;
    
    active_builds[active_build_count] = ctx;
    active_build_count++;
    return TINYPKG_SUCCESS;
}

static void remove_active_build(build_context_t *ctx) {
    for (int i = 0; i < active_build_count; i++) {
        if (active_builds[i] == ctx) {
            for (int j = i; j < active_build_count - 1; j++) {
                active_builds[j] = active_builds[j + 1];
            }
            active_builds[active_build_count - 1] = NULL;
            active_build_count--;
            break;
        }
    }
}

// Main build function
int build_package(package_t *pkg) {
    if (!pkg) return TINYPKG_ERROR;
    
    log_info("Building package: %s %s", pkg->name, pkg->version);
    
    build_context_t *ctx = build_context_create(pkg);
    if (!ctx) return TINYPKG_ERROR_MEMORY;
    
    int result = TINYPKG_SUCCESS;
    
    ctx->start_time = time(NULL);
    add_active_build(ctx);
    
    // Step 1: Download source
    ctx->status = BUILD_STATUS_DOWNLOADING;
    log_info("Downloading source for %s", pkg->name);
    result = build_download_source(ctx);
    if (result != TINYPKG_SUCCESS) {
        log_error("Failed to download source for %s", pkg->name);
        goto cleanup;
    }
    
    // Step 2: Extract source
    ctx->status = BUILD_STATUS_EXTRACTING;
    log_info("Extracting source for %s", pkg->name);
    result = build_extract_source(ctx);
    if (result != TINYPKG_SUCCESS) {
        log_error("Failed to extract source for %s", pkg->name);
        goto cleanup;
    }
    
    // Step 3: Configure
    ctx->status = BUILD_STATUS_CONFIGURING;
    log_info("Configuring %s", pkg->name);
    result = build_configure_package(ctx);
    if (result != TINYPKG_SUCCESS) {
        log_error("Failed to configure %s", pkg->name);
        goto cleanup;
    }
    
    // Step 4: Build
    ctx->status = BUILD_STATUS_BUILDING;
    log_info("Compiling %s", pkg->name);
    result = build_compile_package(ctx);
    if (result != TINYPKG_SUCCESS) {
        log_error("Failed to compile %s", pkg->name);
        goto cleanup;
    }
    
    ctx->status = BUILD_STATUS_COMPLETE;
    ctx->end_time = time(NULL);
    log_info("Successfully built %s in %ld seconds", 
             pkg->name, ctx->end_time - ctx->start_time);

cleanup:
    if (result != TINYPKG_SUCCESS) {
        ctx->status = BUILD_STATUS_FAILED;
        ctx->end_time = time(NULL);
    }
    
    remove_active_build(ctx);
    
    // Cleanup unless configured to keep build directory
    if (!global_config->keep_build_dir || result != TINYPKG_SUCCESS) {
        build_context_cleanup(ctx);
    }
    
    build_context_free(ctx);
    return result;
}

int build_install_package(package_t *pkg) {
    if (!pkg) return TINYPKG_ERROR;
    
    log_info("Installing package: %s %s", pkg->name, pkg->version);
    
    build_context_t *ctx = build_context_create(pkg);
    if (!ctx) return TINYPKG_ERROR_MEMORY;
    
    // For installation, we assume the package was already built
    // and we're installing from the build directory
    
    ctx->status = BUILD_STATUS_INSTALLING;
    int result = build_install_files(ctx);
    
    if (result == TINYPKG_SUCCESS) {
        log_info("Successfully installed %s", pkg->name);
    } else {
        log_error("Failed to install %s", pkg->name);
    }
    
    build_context_free(ctx);
    return result;
}

// Build context management
build_context_t *build_context_create(package_t *pkg) {
    if (!pkg) return NULL;
    
    build_context_t *ctx = TINYPKG_CALLOC(1, sizeof(build_context_t));
    if (!ctx) return NULL;
    
    ctx->package = pkg;
    ctx->status = BUILD_STATUS_INIT;
    ctx->build_pid = 0;
    
    // Set up directories
    snprintf(ctx->build_dir, sizeof(ctx->build_dir), 
             "%s/builds/%s-%s", CACHE_DIR, pkg->name, pkg->version);
    
    snprintf(ctx->source_dir, sizeof(ctx->source_dir), 
             "%s/source", ctx->build_dir);
    
    snprintf(ctx->install_dir, sizeof(ctx->install_dir), 
             "%s/install", ctx->build_dir);
    
    if (build_context_setup_directories(ctx) != TINYPKG_SUCCESS) {
        build_context_free(ctx);
        return NULL;
    }
    
    return ctx;
}

void build_context_free(build_context_t *ctx) {
    if (!ctx) return;
    TINYPKG_FREE(ctx);
}

int build_context_setup_directories(build_context_t *ctx) {
    if (!ctx) return TINYPKG_ERROR;
    
    int result = 0;
    result |= utils_create_directory_recursive(ctx->build_dir);
    result |= utils_create_directory_recursive(ctx->source_dir);
    result |= utils_create_directory_recursive(ctx->install_dir);
    
    return (result == 0) ? TINYPKG_SUCCESS : TINYPKG_ERROR;
}

int build_context_cleanup(build_context_t *ctx) {
    if (!ctx) return TINYPKG_ERROR;
    
    log_debug("Cleaning up build directory: %s", ctx->build_dir);
    return utils_remove_directory_recursive(ctx->build_dir);
}

// Build steps implementation
int build_download_source(build_context_t *ctx) {
    if (!ctx || !ctx->package) return TINYPKG_ERROR;
    
    package_t *pkg = ctx->package;
    char download_path[MAX_PATH];
    
    // Determine download filename
    char *basename = utils_get_basename(pkg->source_url);
    if (!basename) {
        log_error("Failed to determine filename from URL: %s", pkg->source_url);
        return TINYPKG_ERROR;
    }
    
    snprintf(download_path, sizeof(download_path), 
             "%s/sources/%s", CACHE_DIR, basename);
    
    // Check if already downloaded
    if (utils_file_exists(download_path)) {
        log_info("Source already downloaded: %s", basename);
        TINYPKG_FREE(basename);
        return TINYPKG_SUCCESS;
    }
    
    // Create sources directory
    char sources_dir[MAX_PATH];
    snprintf(sources_dir, sizeof(sources_dir), "%s/sources", CACHE_DIR);
    utils_create_directory_recursive(sources_dir);
    
    // Download using wget or curl
    char cmd[MAX_CMD];
    snprintf(cmd, sizeof(cmd), 
             "wget -O '%s' '%s' || curl -o '%s' -L '%s'",
             download_path, pkg->source_url, download_path, pkg->source_url);
    
    log_debug("Download command: %s", cmd);
    int result = utils_run_command(cmd, NULL);
    
    TINYPKG_FREE(basename);
    return result;
}

int build_extract_source(build_context_t *ctx) {
    if (!ctx || !ctx->package) return TINYPKG_ERROR;
    
    package_t *pkg = ctx->package;
    char *basename = utils_get_basename(pkg->source_url);
    if (!basename) return TINYPKG_ERROR;
    
    char archive_path[MAX_PATH];
    snprintf(archive_path, sizeof(archive_path), 
             "%s/sources/%s", CACHE_DIR, basename);
    
    if (!utils_file_exists(archive_path)) {
        log_error("Source archive not found: %s", archive_path);
        TINYPKG_FREE(basename);
        return TINYPKG_ERROR;
    }
    
    // Determine extraction command based on file extension
    char cmd[MAX_CMD];
    if (utils_string_ends_with(basename, ".tar.gz") || 
        utils_string_ends_with(basename, ".tgz")) {
        snprintf(cmd, sizeof(cmd), 
                 "tar -xzf '%s' -C '%s' --strip-components=1",
                 archive_path, ctx->source_dir);
    } else if (utils_string_ends_with(basename, ".tar.bz2") ||
               utils_string_ends_with(basename, ".tbz2")) {
        snprintf(cmd, sizeof(cmd), 
                 "tar -xjf '%s' -C '%s' --strip-components=1",
                 archive_path, ctx->source_dir);
    } else if (utils_string_ends_with(basename, ".tar.xz")) {
        snprintf(cmd, sizeof(cmd), 
                 "tar -xJf '%s' -C '%s' --strip-components=1",
                 archive_path, ctx->source_dir);
    } else if (utils_string_ends_with(basename, ".zip")) {
        snprintf(cmd, sizeof(cmd), 
                 "unzip -q '%s' -d '%s'", archive_path, ctx->source_dir);
    } else {
        log_error("Unsupported archive format: %s", basename);
        TINYPKG_FREE(basename);
        return TINYPKG_ERROR;
    }
    
    log_debug("Extract command: %s", cmd);
    int result = utils_run_command(cmd, NULL);
    
    TINYPKG_FREE(basename);
    return result;
}

int build_configure_package(build_context_t *ctx) {
    if (!ctx || !ctx->package) return TINYPKG_ERROR;
    
    package_t *pkg = ctx->package;
    
    // Auto-detect build system if not specified
    if (pkg->build_system == BUILD_TYPE_AUTOTOOLS && 
        strlen(pkg->build_cmd) == 0) {
        pkg->build_system = build_detect_system(ctx->source_dir);
    }
    
    // Run appropriate configure step
    switch (pkg->build_system) {
        case BUILD_TYPE_AUTOTOOLS:
            return build_run_autotools(ctx);
        case BUILD_TYPE_CMAKE:
            return build_run_cmake(ctx);
        case BUILD_TYPE_MAKE:
            return TINYPKG_SUCCESS; // Make doesn't need configure
        case BUILD_TYPE_CUSTOM:
            return build_run_custom(ctx);
        default:
            log_error("Unknown build system: %d", pkg->build_system);
            return TINYPKG_ERROR;
    }
}

int build_compile_package(build_context_t *ctx) {
    if (!ctx || !ctx->package) return TINYPKG_ERROR;
    
    package_t *pkg = ctx->package;
    char cmd[MAX_CMD];
    
    if (strlen(pkg->build_cmd) > 0) {
        // Use custom build command
        strncpy(cmd, pkg->build_cmd, sizeof(cmd) - 1);
    } else {
        // Use standard make command
        int parallel_jobs = global_config ? global_config->parallel_jobs : 4;
        snprintf(cmd, sizeof(cmd), "make -j%d", parallel_jobs);
    }
    
    log_debug("Build command: %s", cmd);
    return utils_run_command(cmd, ctx->source_dir);
}

int build_install_files(build_context_t *ctx) {
    if (!ctx || !ctx->package) return TINYPKG_ERROR;
    
    package_t *pkg = ctx->package;
    char cmd[MAX_CMD];
    
    if (strlen(pkg->install_cmd) > 0) {
        // Use custom install command
        strncpy(cmd, pkg->install_cmd, sizeof(cmd) - 1);
    } else {
        // Use standard make install
        const char *prefix = global_config ? global_config->install_prefix : "/usr/local";
        snprintf(cmd, sizeof(cmd), "make install DESTDIR='%s' PREFIX='%s'",
                 ctx->install_dir, prefix);
    }
    
    log_debug("Install command: %s", cmd);
    int result = utils_run_command(cmd, ctx->source_dir);
    
    if (result == TINYPKG_SUCCESS) {
        // Copy installed files to system
        char copy_cmd[MAX_CMD];
        snprintf(copy_cmd, sizeof(copy_cmd), 
                 "cp -a '%s'/* / 2>/dev/null || true", ctx->install_dir);
        result = utils_run_command(copy_cmd, NULL);
    }
    
    return result;
}

// Build system detection
build_type_t build_detect_system(const char *source_dir) {
    if (!source_dir) return BUILD_TYPE_AUTOTOOLS;
    
    char path[MAX_PATH];
    
    // Check for CMakeLists.txt
    snprintf(path, sizeof(path), "%s/CMakeLists.txt", source_dir);
    if (utils_file_exists(path)) {
        return BUILD_TYPE_CMAKE;
    }
    
    // Check for configure script
    snprintf(path, sizeof(path), "%s/configure", source_dir);
    if (utils_file_exists(path)) {
        return BUILD_TYPE_AUTOTOOLS;
    }
    
    // Check for Makefile
    snprintf(path, sizeof(path), "%s/Makefile", source_dir);
    if (utils_file_exists(path)) {
        return BUILD_TYPE_MAKE;
    }
    
    // Default to autotools
    return BUILD_TYPE_AUTOTOOLS;
}

int build_run_autotools(build_context_t *ctx) {
    if (!ctx) return TINYPKG_ERROR;
    
    char configure_path[MAX_PATH];
    snprintf(configure_path, sizeof(configure_path), "%s/configure", ctx->source_dir);
    
    if (!utils_file_exists(configure_path)) {
        log_warn("No configure script found, trying autogen");
        
        // Try to run autogen or autoreconf
        char cmd[MAX_CMD];
        snprintf(cmd, sizeof(cmd), 
                 "./autogen.sh || autoreconf -fiv || ./bootstrap");
        
        int result = utils_run_command(cmd, ctx->source_dir);
        if (result != TINYPKG_SUCCESS) {
            log_warn("Failed to generate configure script");
        }
    }
    
    // Run configure
    char cmd[MAX_CMD];
    const char *prefix = global_config ? global_config->install_prefix : "/usr/local";
    const char *args = ctx->package->configure_args;
    
    if (strlen(args) > 0) {
        snprintf(cmd, sizeof(cmd), "./configure --prefix=%s %s", prefix, args);
    } else {
        snprintf(cmd, sizeof(cmd), "./configure --prefix=%s", prefix);
    }
    
    log_debug("Configure command: %s", cmd);
    return utils_run_command(cmd, ctx->source_dir);
}

int build_run_cmake(build_context_t *ctx) {
    if (!ctx) return TINYPKG_ERROR;
    
    const char *prefix = global_config ? global_config->install_prefix : "/usr/local";
    const char *build_type = global_config && global_config->debug_symbols ? "Debug" : "Release";
    
    char cmd[MAX_CMD];
    snprintf(cmd, sizeof(cmd), 
             "cmake -DCMAKE_BUILD_TYPE=%s -DCMAKE_INSTALL_PREFIX=%s %s .",
             build_type, prefix, 
             strlen(ctx->package->configure_args) > 0 ? ctx->package->configure_args : "");
    
    log_debug("CMake command: %s", cmd);
    return utils_run_command(cmd, ctx->source_dir);
}

int build_run_make(build_context_t *ctx) {
    UNUSED(ctx);
    // Make doesn't need a configure step
    return TINYPKG_SUCCESS;
}

int build_run_custom(build_context_t *ctx) {
    if (!ctx || !ctx->package) return TINYPKG_ERROR;
    
    if (strlen(ctx->package->build_cmd) == 0) {
        log_error("Custom build system specified but no build command provided");
        return TINYPKG_ERROR;
    }
    
    // For custom builds, the configure step is handled by the build command
    return TINYPKG_SUCCESS;
}

// Utility functions
const char *build_status_to_string(build_status_t status) {
    switch (status) {
        case BUILD_STATUS_INIT: return "Initializing";
        case BUILD_STATUS_DOWNLOADING: return "Downloading";
        case BUILD_STATUS_EXTRACTING: return "Extracting";
        case BUILD_STATUS_CONFIGURING: return "Configuring";
        case BUILD_STATUS_BUILDING: return "Building";
        case BUILD_STATUS_INSTALLING: return "Installing";
        case BUILD_STATUS_COMPLETE: return "Complete";
        case BUILD_STATUS_FAILED: return "Failed";
        default: return "Unknown";
    }
}

int build_is_running(const char *package_name) {
    if (!package_name) return 0;
    
    for (int i = 0; i < active_build_count; i++) {
        if (active_builds[i] && 
            strcmp(active_builds[i]->package->name, package_name) == 0) {
            return 1;
        }
    }
    
    return 0;
}

int build_clean_package(const char *package_name) {
    if (!package_name) return TINYPKG_ERROR;
    
    // Remove build directory for this package
    char build_dir[MAX_PATH];
    snprintf(build_dir, sizeof(build_dir), "%s/builds/%s-*", CACHE_DIR, package_name);
    
    char cmd[MAX_CMD];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", build_dir);
    
    return utils_run_command(cmd, NULL);
}

