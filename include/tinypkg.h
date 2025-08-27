/*
 * TinyPkg - Lightweight Source-Based Package Manager for Linux
 * Copyright (c) 2025 TinyPkg Development Team
 * 
 * This file is part of TinyPkg.
 * 
 * TinyPkg is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef TINYPKG_H
#define TINYPKG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

// Version information
#define TINYPKG_VERSION_MAJOR 1
#define TINYPKG_VERSION_MINOR 0
#define TINYPKG_VERSION_PATCH 0
#define TINYPKG_VERSION "1.0.0"

// Path constants
#define MAX_PATH 4096
#define MAX_CMD 8192
#define MAX_NAME 256
#define MAX_VERSION 64
#define MAX_DESCRIPTION 512
#define MAX_URL 512

// Default directories
#define CONFIG_DIR "/etc/tinypkg"
#define CACHE_DIR "/var/cache/tinypkg"
#define LIB_DIR "/var/lib/tinypkg"
#define REPO_DIR "/var/lib/tinypkg/repo"
#define LOG_DIR "/var/log/tinypkg"
#define BUILD_DIR "/tmp/tinypkg-build"

// Repository settings
#define DEFAULT_REPO_URL "https://github.com/user7210unix/tinypkg-repo.git"
#define REPO_BRANCH "main"

// Build settings
#define DEFAULT_PARALLEL_JOBS 4
#define BUILD_TIMEOUT 3600
#define MAX_DEPENDENCIES 64

// Return codes
#define TINYPKG_SUCCESS 0
#define TINYPKG_ERROR -1
#define TINYPKG_ERROR_MEMORY -2
#define TINYPKG_ERROR_FILE -3
#define TINYPKG_ERROR_NETWORK -4
#define TINYPKG_ERROR_BUILD -5
#define TINYPKG_ERROR_DEPENDENCY -6

// Package states
typedef enum {
    PKG_STATE_UNKNOWN = 0,
    PKG_STATE_AVAILABLE = 1,
    PKG_STATE_DOWNLOADING = 2,
    PKG_STATE_BUILDING = 3,
    PKG_STATE_INSTALLING = 4,
    PKG_STATE_INSTALLED = 5,
    PKG_STATE_FAILED = 6,
    PKG_STATE_BROKEN = 7
} package_state_t;

// Build types
typedef enum {
    BUILD_TYPE_AUTOTOOLS = 0,
    BUILD_TYPE_CMAKE = 1,
    BUILD_TYPE_MAKE = 2,
    BUILD_TYPE_CUSTOM = 3
} build_type_t;

// Forward declarations
typedef struct package package_t;
typedef struct config config_t;
typedef struct build_config build_config_t;
typedef struct dependency_node dependency_node_t;

// Global variables
extern int verbose_mode;
extern int debug_mode;
extern config_t *global_config;

// Common utility macros
#define UNUSED(x) ((void)(x))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Error handling macros
#define TINYPKG_CHECK_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            log_error("NULL pointer: %s at %s:%d", #ptr, __FILE__, __LINE__); \
            return TINYPKG_ERROR_MEMORY; \
        } \
    } while(0)

#define TINYPKG_CHECK_RESULT(result) \
    do { \
        if ((result) != TINYPKG_SUCCESS) { \
            log_error("Operation failed: %s at %s:%d", #result, __FILE__, __LINE__); \
            return (result); \
        } \
    } while(0)

// Memory management helpers
#define TINYPKG_MALLOC(size) malloc(size)
#define TINYPKG_CALLOC(count, size) calloc(count, size)
#define TINYPKG_REALLOC(ptr, size) realloc(ptr, size)
#define TINYPKG_FREE(ptr) do { if(ptr) { free(ptr); ptr = NULL; } } while(0)
#define TINYPKG_STRDUP(str) strdup(str)

// String utilities
#define TINYPKG_STREQ(a, b) (strcmp(a, b) == 0)
#define TINYPKG_STRNEQ(a, b, n) (strncmp(a, b, n) == 0)
#define TINYPKG_SAFE_STR(str) ((str) ? (str) : "(null)")

// Include module headers
#include "../src/repository.h"
#include "../src/download.h"
#include "../src/build.h"
#include "../src/config.h"
#include "../src/utils.h"
#include "../src/json_parser.h"
#include "../src/dependency.h"
#include "../src/security.h"
#include "../src/logging.h"
#include "../src/package.h"
//#include "package.h"
//#include "repository.h"
//#include "download.h"
//#include "build.h"
//#include "config.h"
//#include "utils.h"
//#include "json_parser.h"
//#include "dependency.h"
//#include "security.h"
//#include "logging.h"

#endif /* TINYPKG_H */
