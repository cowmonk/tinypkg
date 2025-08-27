/*
 * TinyPkg - Configuration System Header
 * Handles configuration file parsing and system settings
 */

#ifndef TINYPKG_CONFIG_H
#define TINYPKG_CONFIG_H

#include <sys/types.h>

// Configuration structure
typedef struct config {
    // General settings
    char root_dir[256];
    char config_dir[256];
    char cache_dir[256];
    char lib_dir[256];
    char log_dir[256];
    char build_dir[256];
    
    // Repository settings
    char repo_url[512];
    char repo_branch[64];
    char repo_dir[256];
    int auto_sync;
    int sync_interval;
    
    // Build settings
    int parallel_jobs;
    int build_timeout;
    char build_flags[512];
    char install_prefix[256];
    int enable_optimizations;
    int debug_symbols;
    int keep_build_dir;
    
    // Package settings
    int force_mode;
    int assume_yes;
    int skip_dependencies;
    int verify_checksums;
    int verify_signatures;
    int create_backups;
    
    // Security settings
    int sandbox_builds;
    char sandbox_user[64];
    char sandbox_group[64];
    char allowed_build_paths[1024];
    
    // Logging settings
    int log_level;
    int log_to_file;
    int log_to_syslog;
    int log_colors;
    char log_file[256];
    int max_log_size;
    int max_log_files;
    
    // Network settings
    int connection_timeout;
    int max_retries;
    char user_agent[256];
    char proxy_url[256];
    int verify_ssl;
    
    // Mirror settings
    char **mirrors;
    int mirror_count;
    int mirror_timeout;
    int use_mirrors;
    
    // Advanced settings
    int max_concurrent_downloads;
    int compression_level;
    int use_progress_bar;
    int show_package_sizes;
    int cleanup_on_failure;
} config_t;

// Repository configuration
typedef struct repo_config {
    char name[128];
    char url[512];
    char branch[64];
    int priority;
    int enabled;
    time_t last_sync;
} repo_config_t;

// Mirror configuration
typedef struct mirror_config {
    char name[128];
    char url[512];
    int priority;
    int enabled;
    int response_time;
} mirror_config_t;

// Function declarations

// Configuration management
config_t *config_create_default(void);
config_t *config_load(void);
config_t *config_load_file(const char *config_file);
int config_save(const config_t *config);
int config_save_file(const config_t *config, const char *config_file);
void config_free(config_t *config);

// Configuration validation
int config_validate(const config_t *config);
int config_create_directories(const config_t *config);
int config_check_permissions(const config_t *config);

// Configuration queries
const char *config_get_string(const config_t *config, const char *key);
int config_get_int(const config_t *config, const char *key);
int config_get_bool(const config_t *config, const char *key);

// Configuration updates
int config_set_string(config_t *config, const char *key, const char *value);
int config_set_int(config_t *config, const char *key, int value);
int config_set_bool(config_t *config, const char *key, int value);

// Repository management
int config_add_repository(const char *name, const char *url, const char *branch);
int config_remove_repository(const char *name);
int config_list_repositories(void);
repo_config_t *config_get_repositories(int *count);
int config_enable_repository(const char *name, int enabled);
int config_set_repository_priority(const char *name, int priority);

// Mirror management
int config_add_mirror(const char *name, const char *url, int priority);
int config_remove_mirror(const char *name);
int config_list_mirrors(void);
mirror_config_t *config_get_mirrors(int *count);
int config_test_mirrors(void);
const char *config_get_fastest_mirror(void);

// Environment detection
int config_detect_system(config_t *config);
int config_detect_architecture(char *arch, size_t size);
int config_detect_distribution(char *distro, size_t size);
int config_detect_cpu_count(void);
size_t config_detect_memory_size(void);

// Path utilities
char *config_expand_path(const char *path);
int config_is_absolute_path(const char *path);
char *config_get_user_config_dir(void);
char *config_get_system_config_dir(void);

// Configuration file parsing utilities
typedef struct config_section {
    char name[128];
    struct config_entry *entries;
    struct config_section *next;
} config_section_t;

typedef struct config_parser {
    config_section_t *sections;
    int section_count;
    char error_message[512];
} config_parser_t;

// Configuration parsing functions
config_parser_t *config_parser_create(void);
void config_parser_free(config_parser_t *parser);
int config_parser_load_file(config_parser_t *parser, const char *filename);
int config_parser_load_string(config_parser_t *parser, const char *content);
const char *config_parser_get_value(config_parser_t *parser, 
                                   const char *section, const char *key);
int config_parser_set_value(config_parser_t *parser, 
                           const char *section, const char *key, 
                           const char *value);
int config_parser_save_file(config_parser_t *parser, const char *filename);

// Template and default configuration generation
int config_generate_default_file(const char *filename);
int config_generate_template(const char *filename);
int config_migrate_from_version(const char *old_config, const char *new_config);

// Configuration validation and cleanup
int config_check_syntax(const char *filename);
int config_cleanup_old_configs(void);
int config_backup_config(const char *config_file);

// Utility macros
#define CONFIG_DEFAULT_PARALLEL_JOBS 4
#define CONFIG_DEFAULT_BUILD_TIMEOUT 3600
#define CONFIG_DEFAULT_CONNECTION_TIMEOUT 30
#define CONFIG_DEFAULT_MAX_RETRIES 3
#define CONFIG_DEFAULT_COMPRESSION_LEVEL 6

#endif /* TINYPKG_CONFIG_H */
