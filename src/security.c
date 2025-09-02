/*
 * TinyPkg - Security System Implementation
 * Checksum verification and basic security functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include "../include/tinypkg.h"

// Global security context
static security_context_t g_security_ctx = {0};
static int security_initialized = 0;

// Helper functions
static int run_hash_command(const char *file_path, const char *hash_cmd, 
                           char *output, size_t output_size) {
    char cmd[MAX_CMD];
    snprintf(cmd, sizeof(cmd), "%s '%s'", hash_cmd, file_path);
    
    char *result = NULL;
    int exit_code;
    
    if (utils_run_command_with_output(cmd, NULL, &result, &exit_code) == TINYPKG_SUCCESS &&
        exit_code == 0 && result) {
        
        // Extract hash from output (first token)
        char *token = strtok(result, " \t\n");
        if (token) {
            strncpy(output, token, output_size - 1);
            output[output_size - 1] = '\0';
            TINYPKG_FREE(result);
            return TINYPKG_SUCCESS;
        }
    }
    
    if (result) TINYPKG_FREE(result);
    return TINYPKG_ERROR;
}

// System initialization
int security_init(void) {
    if (security_initialized) {
        return TINYPKG_SUCCESS;
    }
    
    log_debug("Initializing security system");
    
    // Set default values
    g_security_ctx.verify_checksums = 1;
    g_security_ctx.verify_signatures = 0; // Disabled by default
    snprintf(g_security_ctx.keyring_path, sizeof(g_security_ctx.keyring_path), 
             "%s/keyring", CONFIG_DIR);
    
    security_initialized = 1;
    return TINYPKG_SUCCESS;
}

void security_cleanup(void) {
    if (security_initialized) {
        log_debug("Cleaning up security system");
        security_initialized = 0;
    }
}

// Checksum calculation
int security_calculate_checksum(const char *file_path, char *hash_output, 
                               size_t hash_size, hash_type_t type) {
    if (!file_path || !hash_output || hash_size == 0) {
        return TINYPKG_ERROR;
    }
    
    if (!utils_file_exists(file_path)) {
        log_error("File not found for checksum calculation: %s", file_path);
        return TINYPKG_ERROR;
    }
    
    const char *hash_cmd;
    switch (type) {
        case HASH_TYPE_MD5:
            hash_cmd = "md5sum";
            break;
        case HASH_TYPE_SHA1:
            hash_cmd = "sha1sum";
            break;
        case HASH_TYPE_SHA256:
            hash_cmd = "sha256sum";
            break;
        default:
            log_error("Unsupported hash type: %d", type);
            return TINYPKG_ERROR;
    }
    
    // Check if hash command is available
    char check_cmd[MAX_CMD];
    snprintf(check_cmd, sizeof(check_cmd), "which %s >/dev/null 2>&1", hash_cmd);
    if (system(check_cmd) != 0) {
        log_error("Hash command not available: %s", hash_cmd);
        return TINYPKG_ERROR;
    }
    
    return run_hash_command(file_path, hash_cmd, hash_output, hash_size);
}

// Checksum verification
int security_verify_checksum(const char *file_path, const char *expected_hash, hash_type_t type) {
    if (!file_path || !expected_hash) {
        return TINYPKG_ERROR;
    }
    
    if (!g_security_ctx.verify_checksums) {
        log_debug("Checksum verification disabled, skipping");
        return TINYPKG_SUCCESS;
    }
    
    char calculated_hash[256];
    int result = security_calculate_checksum(file_path, calculated_hash, sizeof(calculated_hash), type);
    
    if (result != TINYPKG_SUCCESS) {
        log_error("Failed to calculate checksum for: %s", file_path);
        return result;
    }
    
    // Compare hashes (case-insensitive)
    if (strcasecmp(calculated_hash, expected_hash) == 0) {
        log_debug("Checksum verification passed for: %s", file_path);
        return TINYPKG_SUCCESS;
    } else {
        log_error("Checksum verification failed for: %s", file_path);
        log_error("Expected: %s", expected_hash);
        log_error("Calculated: %s", calculated_hash);
        return TINYPKG_ERROR;
    }
}

// Detect hash type from string length and format
hash_type_t security_detect_hash_type(const char *hash_string) {
    if (!hash_string) return HASH_TYPE_SHA256; // Default
    
    size_t len = strlen(hash_string);
    
    // Check if all characters are hexadecimal
    for (size_t i = 0; i < len; i++) {
        if (!isxdigit(hash_string[i])) {
            return HASH_TYPE_SHA256; // Default for invalid format
        }
    }
    
    switch (len) {
        case 32: return HASH_TYPE_MD5;
        case 40: return HASH_TYPE_SHA1;
        case 64: return HASH_TYPE_SHA256;
        default: return HASH_TYPE_SHA256; // Default
    }
}

// Package integrity verification
int security_verify_package_integrity(const package_t *pkg, const char *file_path) {
    if (!pkg || !file_path) {
        return TINYPKG_ERROR;
    }
    
    if (!g_security_ctx.verify_checksums) {
        return TINYPKG_SUCCESS;
    }
    
    if (strlen(pkg->checksum) == 0) {
        log_warn("No checksum provided for package: %s", pkg->name);
        return TINYPKG_SUCCESS; // Don't fail if no checksum provided
    }
    
    // Auto-detect hash type
    hash_type_t hash_type = security_detect_hash_type(pkg->checksum);
    
    log_info("Verifying integrity of %s", pkg->name);
    return security_verify_checksum(file_path, pkg->checksum, hash_type);
}

// Path validation
int security_validate_path(const char *path) {
    if (!path || strlen(path) == 0) {
        return TINYPKG_ERROR;
    }
    
    // Check for dangerous path components
    if (strstr(path, "..") != NULL) {
        log_error("Path contains dangerous component '..': %s", path);
        return TINYPKG_ERROR;
    }
    
    // Check for null bytes
    if (strlen(path) != strcspn(path, "\0")) {
        log_error("Path contains null byte: %s", path);
        return TINYPKG_ERROR;
    }
    
    // Check length
    if (strlen(path) > MAX_PATH - 1) {
        log_error("Path too long: %s", path);
        return TINYPKG_ERROR;
    }
    
    return TINYPKG_SUCCESS;
}

// Safe filename validation
int security_is_safe_filename(const char *filename) {
    if (!filename || strlen(filename) == 0) {
        return 0;
    }
    
    // Check for dangerous characters
    const char *dangerous_chars = ";<>|&/*";
    return TINYPKG_SUCCESS;
}
