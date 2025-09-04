/*
 * TinyPkg - Build System Implementation
 * Package building and installation functions
 */

#include "../include/tinypkg.h"
#include "download.h"
#include "package.h"
#include "security.h"
#include "utils.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// Global build tracking
static build_context_t *active_builds[16] = {0};
static int active_build_count = 0;

// Helper functions for safe path operations
static int safe_path_join(char *dest, size_t dest_size, const char *base,
                          const char *suffix)
{
    size_t base_len = strlen(base);
    size_t suffix_len = strlen(suffix);

    if (base_len + suffix_len + 1 >= dest_size)
    {
        return TINYPKG_ERROR; // Path too long
    }

    strcpy(dest, base);
    strcat(dest, suffix);
    return TINYPKG_SUCCESS;
}

static int build_download_command(char *cmd, size_t cmd_size,
                                  const char *download_path,
                                  const char *source_url)
{
    int needed = snprintf(NULL, 0, "wget -O '%s' '%s' || curl -o '%s' -L '%s'",
                          download_path, source_url, download_path, source_url);

    if (needed >= (int)cmd_size)
    {
        log_error("Download command too long: %d bytes needed, %zu available",
                  needed, cmd_size);
        return TINYPKG_ERROR;
    }

    snprintf(cmd, cmd_size, "wget -O '%s' '%s' || curl -o '%s' -L '%s'",
             download_path, source_url, download_path, source_url);
    return TINYPKG_SUCCESS;
}

static int build_extract_command(char *cmd, size_t cmd_size,
                                 const char *archive_path, const char *dest_dir,
                                 const char *format)
{
    const char *extract_templates[] = {
        "tar -xzf '%s' -C '%s' --strip-components=1", // .tar.gz
        "tar -xjf '%s' -C '%s' --strip-components=1", // .tar.bz2
        "tar -xJf '%s' -C '%s' --strip-components=1", // .tar.xz
        "unzip -q '%s' -d '%s'"                       // .zip
    };

    int template_idx = 0;
    if (strstr(format, ".tar.gz") || strstr(format, ".tgz"))
    {
        template_idx = 0;
    }
    else if (strstr(format, ".tar.bz2") || strstr(format, ".tbz2"))
    {
        template_idx = 1;
    }
    else if (strstr(format, ".tar.xz"))
    {
        template_idx = 2;
    }
    else if (strstr(format, ".zip"))
    {
        template_idx = 3;
    }
    else
    {
        return TINYPKG_ERROR;
    }

    int needed = snprintf(NULL, 0, extract_templates[template_idx],
                          archive_path, dest_dir);
    if (needed >= (int)cmd_size)
    {
        log_error("Extract command too long: %d bytes needed, %zu available",
                  needed, cmd_size);
        return TINYPKG_ERROR;
    }

    snprintf(cmd, cmd_size, extract_templates[template_idx], archive_path,
             dest_dir);
    return TINYPKG_SUCCESS;
}

static int safe_configure_command(char *cmd, size_t cmd_size,
                                  const char *prefix, const char *args)
{
    int needed;

    if (args && strlen(args) > 0)
    {
        needed = snprintf(NULL, 0, "./configure --prefix=%s %s", prefix, args);
        if (needed >= (int)cmd_size)
        {
            log_error(
                "Configure command too long: %d bytes needed, %zu available",
                needed, cmd_size);
            return TINYPKG_ERROR;
        }
        snprintf(cmd, cmd_size, "./configure --prefix=%s %s", prefix, args);
    }
    else
    {
        needed = snprintf(NULL, 0, "./configure --prefix=%s", prefix);
        if (needed >= (int)cmd_size)
        {
            log_error(
                "Configure command too long: %d bytes needed, %zu available",
                needed, cmd_size);
            return TINYPKG_ERROR;
        }
        snprintf(cmd, cmd_size, "./configure --prefix=%s", prefix);
    }

    return TINYPKG_SUCCESS;
}

static int safe_cmake_command(char *cmd, size_t cmd_size,
                              const char *build_type, const char *prefix,
                              const char *args)
{
    const char *cmake_args = (args && strlen(args) > 0) ? args : "";

    int needed = snprintf(
        NULL, 0, "cmake -DCMAKE_BUILD_TYPE=%s -DCMAKE_INSTALL_PREFIX=%s %s .",
        build_type, prefix, cmake_args);

    if (needed >= (int)cmd_size)
    {
        log_error("CMake command too long: %d bytes needed, %zu available",
                  needed, cmd_size);
        return TINYPKG_ERROR;
    }

    snprintf(cmd, cmd_size,
             "cmake -DCMAKE_BUILD_TYPE=%s -DCMAKE_INSTALL_PREFIX=%s %s .",
             build_type, prefix, cmake_args);
    return TINYPKG_SUCCESS;
}

// Helper functions for active builds
static int add_active_build(build_context_t *ctx)
{
    if (active_build_count >= 16)
        return TINYPKG_ERROR;

    active_builds[active_build_count] = ctx;
    active_build_count++;
    return TINYPKG_SUCCESS;
}

static void remove_active_build(build_context_t *ctx)
{
    for (int i = 0; i < active_build_count; i++)
    {
        if (active_builds[i] == ctx)
        {
            for (int j = i; j < active_build_count - 1; j++)
            {
                active_builds[j] = active_builds[j + 1];
            }
            active_builds[active_build_count - 1] = NULL;
            active_build_count--;
            break;
        }
    }
}

// Main build function
int build_package(package_t *pkg)
{
    if (!pkg)
        return TINYPKG_ERROR;

    log_info("Building package: %s %s", pkg->name, pkg->version);

    build_context_t *ctx = build_context_create(pkg);
    if (!ctx)
        return TINYPKG_ERROR_MEMORY;

    int result = TINYPKG_SUCCESS;

    ctx->start_time = time(NULL);
    add_active_build(ctx);

    // Step 1: Download source
    ctx->status = BUILD_STATUS_DOWNLOADING;
    log_info("Downloading source for %s", pkg->name);
    result = build_download_source(ctx);
    if (result != TINYPKG_SUCCESS)
    {
        log_error("Failed to download source for %s", pkg->name);
        goto cleanup;
    }

    // Step 2: Extract source
    ctx->status = BUILD_STATUS_EXTRACTING;
    log_info("Extracting source for %s", pkg->name);
    result = build_extract_source(ctx);
    if (result != TINYPKG_SUCCESS)
    {
        log_error("Failed to extract source for %s", pkg->name);
        goto cleanup;
    }

    // Step 3: Configure
    ctx->status = BUILD_STATUS_CONFIGURING;
    log_info("Configuring %s", pkg->name);
    result = build_configure_package(ctx);
    if (result != TINYPKG_SUCCESS)
    {
        log_error("Failed to configure %s", pkg->name);
        goto cleanup;
    }

    // Step 4: Build
    ctx->status = BUILD_STATUS_BUILDING;
    log_info("Compiling %s", pkg->name);
    result = build_compile_package(ctx);
    if (result != TINYPKG_SUCCESS)
    {
        log_error("Failed to compile %s", pkg->name);
        goto cleanup;
    }

    ctx->status = BUILD_STATUS_COMPLETE;
    ctx->end_time = time(NULL);
    log_info("Successfully built %s in %ld seconds", pkg->name,
             ctx->end_time - ctx->start_time);

cleanup:
    if (result != TINYPKG_SUCCESS)
    {
        ctx->status = BUILD_STATUS_FAILED;
        ctx->end_time = time(NULL);
    }

    remove_active_build(ctx);

    // Cleanup unless configured to keep build directory
    if (!global_config->keep_build_dir || result != TINYPKG_SUCCESS)
    {
        build_context_cleanup(ctx);
    }

    build_context_free(ctx);
    return result;
}

int build_install_package(package_t *pkg)
{
    if (!pkg)
        return TINYPKG_ERROR;

    log_info("Installing package: %s %s", pkg->name, pkg->version);

    build_context_t *ctx = build_context_create(pkg);
    if (!ctx)
        return TINYPKG_ERROR_MEMORY;

    // For installation, we assume the package was already built
    // and we're installing from the build directory

    ctx->status = BUILD_STATUS_INSTALLING;
    int result = build_install_files(ctx);

    if (result == TINYPKG_SUCCESS)
    {
        log_info("Successfully installed %s", pkg->name);
    }
    else
    {
        log_error("Failed to install %s", pkg->name);
    }

    build_context_free(ctx);
    return result;
}

// Build context management
build_context_t *build_context_create(package_t *pkg)
{
    if (!pkg)
        return NULL;

    build_context_t *ctx = TINYPKG_CALLOC(1, sizeof(build_context_t));
    if (!ctx)
        return NULL;

    ctx->package = pkg;
    ctx->status = BUILD_STATUS_INIT;
    ctx->build_pid = 0;

    // Set up directories with safe path joining
    int result = 0;
    result |= snprintf(ctx->build_dir, sizeof(ctx->build_dir),
                       "%s/builds/%s-%s", CACHE_DIR, pkg->name, pkg->version);
    if (result >= (int)sizeof(ctx->build_dir))
    {
        log_error("Build directory path too long");
        build_context_free(ctx);
        return NULL;
    }

    if (safe_path_join(ctx->source_dir, sizeof(ctx->source_dir), ctx->build_dir,
                       "/source") != TINYPKG_SUCCESS)
    {
        log_error("Source directory path too long");
        build_context_free(ctx);
        return NULL;
    }

    if (safe_path_join(ctx->install_dir, sizeof(ctx->install_dir),
                       ctx->build_dir, "/install") != TINYPKG_SUCCESS)
    {
        log_error("Install directory path too long");
        build_context_free(ctx);
        return NULL;
    }

    if (build_context_setup_directories(ctx) != TINYPKG_SUCCESS)
    {
        build_context_free(ctx);
        return NULL;
    }

    return ctx;
}

void build_context_free(build_context_t *ctx)
{
    if (!ctx)
        return;
    TINYPKG_FREE(ctx);
}

int build_context_setup_directories(build_context_t *ctx)
{
    if (!ctx)
        return TINYPKG_ERROR;

    int result = 0;
    result |= utils_create_directory_recursive(ctx->build_dir);
    result |= utils_create_directory_recursive(ctx->source_dir);
    result |= utils_create_directory_recursive(ctx->install_dir);

    return (result == 0) ? TINYPKG_SUCCESS : TINYPKG_ERROR;
}

int build_context_cleanup(build_context_t *ctx)
{
    if (!ctx)
        return TINYPKG_ERROR;

    log_debug("Cleaning up build directory: %s", ctx->build_dir);
    return utils_remove_directory_recursive(ctx->build_dir);
}

// Build steps implementation
int build_download_source(build_context_t *ctx)
{
    if (!ctx || !ctx->package)
        return TINYPKG_ERROR;

    package_t *pkg = ctx->package;
    char download_path[MAX_PATH];

    // Determine download filename
    char *basename = utils_get_basename(pkg->source_url);
    if (!basename)
    {
        log_error("Failed to determine filename from URL: %s", pkg->source_url);
        return TINYPKG_ERROR;
    }

    int path_result = snprintf(download_path, sizeof(download_path),
                               "%s/sources/%s", CACHE_DIR, basename);
    if (path_result >= (int)sizeof(download_path))
    {
        log_error("Download path too long");
        TINYPKG_FREE(basename);
        return TINYPKG_ERROR;
    }

    // Check if already downloaded
    if (utils_file_exists(download_path))
    {
        log_info("Source already downloaded: %s", basename);
        TINYPKG_FREE(basename);
        return TINYPKG_SUCCESS;
    }

    // Create sources directory
    char sources_dir[MAX_PATH];
    int sources_result =
        snprintf(sources_dir, sizeof(sources_dir), "%s/sources", CACHE_DIR);
    if (sources_result >= (int)sizeof(sources_dir))
    {
        log_error("Sources directory path too long");
        TINYPKG_FREE(basename);
        return TINYPKG_ERROR;
    }
    utils_create_directory_recursive(sources_dir);

    // Download using wget or curl with safe command building
    char cmd[MAX_CMD];
    int result = build_download_command(cmd, sizeof(cmd), download_path,
                                        pkg->source_url);
    if (result != TINYPKG_SUCCESS)
    {
        TINYPKG_FREE(basename);
        return result;
    }

    log_debug("Download command: %s", cmd);
    result = utils_run_command(cmd, NULL);

    TINYPKG_FREE(basename);
    return result;
}

int build_extract_source(build_context_t *ctx)
{
    if (!ctx || !ctx->package)
        return TINYPKG_ERROR;

    package_t *pkg = ctx->package;
    char *basename = utils_get_basename(pkg->source_url);
    if (!basename)
        return TINYPKG_ERROR;

    char archive_path[MAX_PATH];
    int path_result = snprintf(archive_path, sizeof(archive_path),
                               "%s/sources/%s", CACHE_DIR, basename);
    if (path_result >= (int)sizeof(archive_path))
    {
        log_error("Archive path too long");
        TINYPKG_FREE(basename);
        return TINYPKG_ERROR;
    }

    if (!utils_file_exists(archive_path))
    {
        log_error("Source archive not found: %s", archive_path);
        TINYPKG_FREE(basename);
        return TINYPKG_ERROR;
    }

    // Use safe command building for extraction
    char cmd[MAX_CMD];
    int result = build_extract_command(cmd, sizeof(cmd), archive_path,
                                       ctx->source_dir, basename);
    if (result != TINYPKG_SUCCESS)
    {
        log_error("Unsupported archive format: %s", basename);
        TINYPKG_FREE(basename);
        return result;
    }

    log_debug("Extract command: %s", cmd);
    result = utils_run_command(cmd, NULL);

    TINYPKG_FREE(basename);
    return result;
}

int build_configure_package(build_context_t *ctx)
{
    if (!ctx || !ctx->package)
        return TINYPKG_ERROR;

    package_t *pkg = ctx->package;

    // Auto-detect build system if not specified
    if (pkg->build_system == BUILD_TYPE_AUTOTOOLS &&
        strlen(pkg->build_cmd) == 0)
    {
        pkg->build_system = build_detect_system(ctx->source_dir);
    }

    // Run appropriate configure step
    switch (pkg->build_system)
    {
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

int build_compile_package(build_context_t *ctx)
{
    if (!ctx || !ctx->package)
        return TINYPKG_ERROR;

    package_t *pkg = ctx->package;
    char cmd[MAX_CMD];

    if (strlen(pkg->build_cmd) > 0)
    {
        // Use custom build command - safe copy
        size_t cmd_len = strlen(pkg->build_cmd);
        if (cmd_len >= sizeof(cmd))
        {
            log_error("Build command too long: %zu bytes", cmd_len);
            return TINYPKG_ERROR;
        }
        strncpy(cmd, pkg->build_cmd, sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
    }
    else
    {
        // Use standard make command
        int parallel_jobs = global_config ? global_config->parallel_jobs : 4;
        int result = snprintf(cmd, sizeof(cmd), "make -j%d", parallel_jobs);
        if (result >= (int)sizeof(cmd))
        {
            log_error("Make command too long");
            return TINYPKG_ERROR;
        }
    }

    log_debug("Build command: %s", cmd);
    return utils_run_command(cmd, ctx->source_dir);
}

int build_install_files(build_context_t *ctx)
{
    if (!ctx || !ctx->package)
        return TINYPKG_ERROR;

    package_t *pkg = ctx->package;
    char cmd[MAX_CMD];

    if (strlen(pkg->install_cmd) > 0)
    {
        // Use custom install command - safe copy
        size_t cmd_len = strlen(pkg->install_cmd);
        if (cmd_len >= sizeof(cmd))
        {
            log_error("Install command too long: %zu bytes", cmd_len);
            return TINYPKG_ERROR;
        }
        strncpy(cmd, pkg->install_cmd, sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
    }
    else
    {
        // Use standard make install
        const char *prefix =
            global_config ? global_config->install_prefix : "/usr/local";
        int result =
            snprintf(cmd, sizeof(cmd), "make install DESTDIR='%s' PREFIX='%s'",
                     ctx->install_dir, prefix);
        if (result >= (int)sizeof(cmd))
        {
            log_error("Install command too long");
            return TINYPKG_ERROR;
        }
    }

    log_debug("Install command: %s", cmd);
    int result = utils_run_command(cmd, ctx->source_dir);

    if (result == TINYPKG_SUCCESS)
    {
        // Copy installed files to system
        char copy_cmd[MAX_CMD];
        int copy_result =
            snprintf(copy_cmd, sizeof(copy_cmd),
                     "cp -a '%s'/* / 2>/dev/null || true", ctx->install_dir);
        if (copy_result >= (int)sizeof(copy_cmd))
        {
            log_error("Copy command too long");
            return TINYPKG_ERROR;
        }
        result = utils_run_command(copy_cmd, NULL);
    }

    return result;
}

// Build system detection
build_type_t build_detect_system(const char *source_dir)
{
    if (!source_dir)
        return BUILD_TYPE_AUTOTOOLS;

    char path[MAX_PATH];

    // Check for CMakeLists.txt
    int result = snprintf(path, sizeof(path), "%s/CMakeLists.txt", source_dir);
    if (result < (int)sizeof(path) && utils_file_exists(path))
    {
        return BUILD_TYPE_CMAKE;
    }

    // Check for configure script
    result = snprintf(path, sizeof(path), "%s/configure", source_dir);
    if (result < (int)sizeof(path) && utils_file_exists(path))
    {
        return BUILD_TYPE_AUTOTOOLS;
    }

    // Check for Makefile
    result = snprintf(path, sizeof(path), "%s/Makefile", source_dir);
    if (result < (int)sizeof(path) && utils_file_exists(path))
    {
        return BUILD_TYPE_MAKE;
    }

    // Default to autotools
    return BUILD_TYPE_AUTOTOOLS;
}

int build_run_autotools(build_context_t *ctx)
{
    if (!ctx)
        return TINYPKG_ERROR;

    char configure_path[MAX_PATH];
    int path_result = snprintf(configure_path, sizeof(configure_path),
                               "%s/configure", ctx->source_dir);
    if (path_result >= (int)sizeof(configure_path))
    {
        log_error("Configure path too long");
        return TINYPKG_ERROR;
    }

    if (!utils_file_exists(configure_path))
    {
        log_warn("No configure script found, trying autogen");

        // Try to run autogen or autoreconf
        char cmd[MAX_CMD];
        int cmd_result = snprintf(
            cmd, sizeof(cmd), "./autogen.sh || autoreconf -fiv || ./bootstrap");
        if (cmd_result >= (int)sizeof(cmd))
        {
            log_error("Autogen command too long");
            return TINYPKG_ERROR;
        }

        int result = utils_run_command(cmd, ctx->source_dir);
        if (result != TINYPKG_SUCCESS)
        {
            log_warn("Failed to generate configure script");
        }
    }

    // Run configure with safe command building
    char cmd[MAX_CMD];
    const char *prefix =
        global_config ? global_config->install_prefix : "/usr/local";
    const char *args = ctx->package->configure_args;

    int result = safe_configure_command(cmd, sizeof(cmd), prefix, args);
    if (result != TINYPKG_SUCCESS)
    {
        return result;
    }

    log_debug("Configure command: %s", cmd);
    return utils_run_command(cmd, ctx->source_dir);
}

int build_run_cmake(build_context_t *ctx)
{
    if (!ctx)
        return TINYPKG_ERROR;

    const char *prefix =
        global_config ? global_config->install_prefix : "/usr/local";
    const char *build_type =
        global_config && global_config->debug_symbols ? "Debug" : "Release";

    char cmd[MAX_CMD];
    int result = safe_cmake_command(cmd, sizeof(cmd), build_type, prefix,
                                    ctx->package->configure_args);
    if (result != TINYPKG_SUCCESS)
    {
        return result;
    }

    log_debug("CMake command: %s", cmd);
    return utils_run_command(cmd, ctx->source_dir);
}

int build_run_make(build_context_t *ctx)
{
    UNUSED(ctx);
    // Make doesn't need a configure step
    return TINYPKG_SUCCESS;
}

int build_run_custom(build_context_t *ctx)
{
    if (!ctx || !ctx->package)
        return TINYPKG_ERROR;

    if (strlen(ctx->package->build_cmd) == 0)
    {
        log_error(
            "Custom build system specified but no build command provided");
        return TINYPKG_ERROR;
    }

    // For custom builds, the configure step is handled by the build command
    return TINYPKG_SUCCESS;
}

// Utility functions
const char *build_status_to_string(build_status_t status)
{
    switch (status)
    {
    case BUILD_STATUS_INIT:
        return "Initializing";
    case BUILD_STATUS_DOWNLOADING:
        return "Downloading";
    case BUILD_STATUS_EXTRACTING:
        return "Extracting";
    case BUILD_STATUS_CONFIGURING:
        return "Configuring";
    case BUILD_STATUS_BUILDING:
        return "Building";
    case BUILD_STATUS_INSTALLING:
        return "Installing";
    case BUILD_STATUS_COMPLETE:
        return "Complete";
    case BUILD_STATUS_FAILED:
        return "Failed";
    default:
        return "Unknown";
    }
}

int build_is_running(const char *package_name)
{
    if (!package_name)
        return 0;

    for (int i = 0; i < active_build_count; i++)
    {
        if (active_builds[i] &&
            strcmp(active_builds[i]->package->name, package_name) == 0)
        {
            return 1;
        }
    }

    return 0;
}

int build_clean_package(const char *package_name)
{
    if (!package_name)
        return TINYPKG_ERROR;

    // Remove build directory for this package
    char build_dir[MAX_PATH];
    int result = snprintf(build_dir, sizeof(build_dir), "%s/builds/%s-*",
                          CACHE_DIR, package_name);
    if (result >= (int)sizeof(build_dir))
    {
        log_error("Build directory path too long");
        return TINYPKG_ERROR;
    }

    char cmd[MAX_CMD];
    result = snprintf(cmd, sizeof(cmd), "rm -rf %s", build_dir);
    if (result >= (int)sizeof(cmd))
    {
        log_error("Clean command too long");
        return TINYPKG_ERROR;
    }

    return utils_run_command(cmd, NULL);
}
