/*
 * TinyPkg - Package Management Header
 * Defines package structures and management functions
 */

#ifndef TINYPKG_PACKAGE_H
#define TINYPKG_PACKAGE_H

#include "../include/tinypkg.h"

// Version structure for semantic versioning
typedef struct {
    int major;
    int minor;
    int patch;
    char prerelease[32];
    char build_metadata[64];
} version_t;

// Package information structure
typedef struct package {
    // Basic information
    char name[MAX_NAME];
    char version[MAX_VERSION];
    char description[MAX_DESCRIPTION];
    char maintainer[MAX_NAME];
    char homepage[MAX_URL];
    char license[64];
    
    // Source information
    char source_url[MAX_URL];
    char source_type[32];    // tarball, git, svn, etc.
    char checksum[128];      // SHA-256 checksum
    char signature[256];     // GPG signature
    
    // Build information
    build_type_t build_system;
    char build_cmd[MAX_CMD];
    char install_cmd[MAX_CMD];
    char pre_build_cmd[MAX_CMD];
    char post_install_cmd[MAX_CMD];
    char configure_args[MAX_CMD];
    
    // Dependencies
    char **dependencies;
    int dep_count;
    char **build_dependencies;
    int build_dep_count;
    char **conflicts;
    int conflict_count;
    char **provides;
    int provides_count;
    
    // Metadata
    char category[64];
    size_t size_estimate;       // Estimated installed size in bytes
    int build_time_estimate;    // Estimated build time in seconds
    package_state_t state;
    time_t install_time;
    version_t parsed_version;
    
    // Internal fields
    char json_file[MAX_PATH];
    int ref_count;
} package_t;

// Package database entry
typedef struct package_db_entry {
    char name[MAX_NAME];
    char version[MAX_VERSION];
    char description[MAX_DESCRIPTION];
    time_t install_time;
    size_t installed_size;
    package_state_t state;
    struct package_db_entry *next;
} package_db_entry_t;

// Package search result
typedef struct package_search_result {
    char name[MAX_NAME];
    char version[MAX_VERSION];
    char description[MAX_DESCRIPTION];
    int relevance_score;
    int installed;
} package_search_result_t;

// Function declarations

// Package lifecycle management
int package_install(const char *package_name);
int package_remove(const char *package_name);
int package_update(const char *package_name);
int package_update_all(void);

// Package information
int package_query(const char *package_name);
int package_list(const char *pattern);
int package_search(const char *pattern);
package_t *package_load_info(const char *package_name);
int package_is_installed(const char *package_name);

// Package database operations
int package_db_add(const package_t *pkg);
int package_db_remove(const char *package_name);
int package_db_update(const package_t *pkg);
package_db_entry_t *package_db_find(const char *package_name);
package_db_entry_t *package_db_get_all(void);
int package_db_save(void);
int package_db_load(void);

// Package validation
int package_validate(const package_t *pkg);
int package_check_conflicts(const package_t *pkg);
int package_verify_integrity(const char *package_name);

// Version handling
int version_parse(const char *version_str, version_t *version);
int version_compare(const version_t *a, const version_t *b);
char *version_to_string(const version_t *version);
int version_is_compatible(const version_t *required, const version_t *available);

// Package memory management
package_t *package_create(void);
void package_free(package_t *pkg);
package_t *package_clone(const package_t *pkg);
void package_db_entry_free(package_db_entry_t *entry);

// Package state management
const char *package_state_to_string(package_state_t state);
package_state_t package_state_from_string(const char *state_str);
int package_set_state(const char *package_name, package_state_t state);
package_state_t package_get_state(const char *package_name);

// Package utilities
int package_get_installed_size(const char *package_name);
char **package_get_file_list(const char *package_name, int *count);
int package_owns_file(const char *file_path, char *owner_package, size_t owner_size);
int package_backup_config_files(const package_t *pkg);
int package_restore_config_files(const package_t *pkg);

// Package statistics
typedef struct package_stats {
    int total_packages;
    int installed_packages;
    int available_packages;
    int broken_packages;
    size_t total_installed_size;
    time_t last_update;
} package_stats_t;

int package_get_stats(package_stats_t *stats);
void package_print_stats(const package_stats_t *stats);

#endif /* TINYPKG_PACKAGE_H */
