/*
 * TinyPkg - Security System Header
 * Checksum verification and basic security functions
 */

#ifndef TINYPKG_SECURITY_H
#define TINYPKG_SECURITY_H

// Hash types
typedef enum {
    HASH_TYPE_MD5 = 0,
    HASH_TYPE_SHA1 = 1,
    HASH_TYPE_SHA256 = 2
} hash_type_t;

// Security context
typedef struct security_context {
    int verify_checksums;
    int verify_signatures;
    char keyring_path[MAX_PATH];
} security_context_t;

// Function declarations

// System initialization
int security_init(void);
void security_cleanup(void);

// Checksum verification
int security_verify_checksum(const char *file_path, const char *expected_hash, hash_type_t type);
int security_calculate_checksum(const char *file_path, char *hash_output, size_t hash_size, hash_type_t type);
hash_type_t security_detect_hash_type(const char *hash_string);

// File integrity
int security_verify_package_integrity(const package_t *pkg, const char *file_path);
int security_create_checksum_file(const char *file_path, const char *checksum_path);

// Basic validation
int security_validate_path(const char *path);
int security_is_safe_filename(const char *filename);

#endif /* TINYPKG_SECURITY_H */
