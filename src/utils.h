/*
 * TinyPkg - Utility Functions Header
 * Common utility functions for file operations, string handling, etc.
 */

#ifndef TINYPKG_UTILS_H
#define TINYPKG_UTILS_H

#include <sys/stat.h>
#include <dirent.h>
#include <fts.h>

// Directory operations
int utils_init_directories(void);
int utils_create_directory_recursive(const char *path);
int utils_remove_directory_recursive(const char *path);
int utils_directory_exists(const char *path);
int utils_is_directory(const char *path);
int utils_get_directory_size(const char *path, size_t *total_size);
int utils_clean_cache(void);

// File operations
int utils_file_exists(const char *path);
int utils_is_file(const char *path);
int utils_copy_file(const char *src, const char *dest);
int utils_move_file(const char *src, const char *dest);
int utils_remove_file(const char *path);
size_t utils_get_file_size(const char *path);
int utils_create_temp_file(char *template_path);
int utils_create_temp_directory(char *template_path);

// Path utilities
char *utils_join_path(const char *dir, const char *file);
char *utils_get_basename(const char *path);
char *utils_get_dirname(const char *path);
char *utils_get_extension(const char *path);
char *utils_resolve_path(const char *path);
int utils_is_absolute_path(const char *path);
char *utils_make_absolute_path(const char *path);

// String utilities
char *utils_string_duplicate(const char *str);
char *utils_string_trim(char *str);
char *utils_string_replace(const char *str, const char *old, const char *new);
int utils_string_starts_with(const char *str, const char *prefix);
int utils_string_ends_with(const char *str, const char *suffix);
char **utils_string_split(const char *str, const char *delimiter, int *count);
void utils_string_array_free(char **array, int count);
char *utils_string_join(char **array, int count, const char *separator);

// Command execution
int utils_run_command(const char *cmd, const char *work_dir);
int utils_run_command_with_output(const char *cmd, const char *work_dir, 
                                 char **output, int *exit_code);
int utils_run_command_async(const char *cmd, const char *work_dir, pid_t *pid);
int utils_wait_for_process(pid_t pid, int *exit_code);
int utils_kill_process(pid_t pid, int signal);

// Process utilities
int utils_is_process_running(pid_t pid);
pid_t utils_get_process_by_name(const char *name);
int utils_get_system_info(void);

// Archive handling
int utils_extract_archive(const char *archive_path, const char *dest_dir);
int utils_create_archive(const char *archive_path, const char *src_dir);
int utils_is_archive(const char *path);
char *utils_get_archive_type(const char *path);

// Checksum and hashing
int utils_calculate_sha256(const char *file_path, char *hash_str, size_t hash_size);
int utils_calculate_md5(const char *file_path, char *hash_str, size_t hash_size);
int utils_verify_checksum(const char *file_path, const char *expected_hash, const char *type);

// System information
typedef struct system_info {
    char hostname[256];
    char kernel_name[64];
    char kernel_release[64];
    char kernel_version[256];
    char machine[64];
    char processor[64];
    char operating_system[64];
    long total_memory;
    long free_memory;
    int cpu_count;
    double load_average[3];
} system_info_t;

int utils_get_system_info_detailed(system_info_t *info);
long utils_get_available_memory(void);
int utils_get_cpu_count(void);
double utils_get_load_average(void);

// Time and date utilities
void utils_format_time(time_t timestamp);
char *utils_format_duration(int seconds);
char *utils_get_timestamp_string(void);
time_t utils_parse_timestamp(const char *timestamp_str);

// Size formatting
void utils_format_size(size_t bytes);
char *utils_format_size_string(size_t bytes);
size_t utils_parse_size_string(const char *size_str);

// Progress and status
typedef struct progress_info {
    size_t current;
    size_t total;
    time_t start_time;
    time_t last_update;
    char message[256];
    int percentage;
    double rate;
    int eta_seconds;
} progress_info_t;

void utils_progress_init(progress_info_t *progress, size_t total, const char *message);
void utils_progress_update(progress_info_t *progress, size_t current);
void utils_progress_finish(progress_info_t *progress);
void utils_progress_display(const progress_info_t *progress);

// Lock file management
int utils_create_lock_file(const char *lock_path, pid_t *existing_pid);
int utils_remove_lock_file(const char *lock_path);
int utils_check_lock_file(const char *lock_path, pid_t *pid);

// Configuration file parsing
typedef struct config_entry {
    char key[128];
    char value[512];
    struct config_entry *next;
} config_entry_t;

config_entry_t *utils_parse_config_file(const char *config_path);
void utils_free_config_entries(config_entry_t *entries);
const char *utils_get_config_value(config_entry_t *entries, const char *key);
int utils_set_config_value(config_entry_t **entries, const char *key, const char *value);
int utils_write_config_file(const char *config_path, config_entry_t *entries);

// Network utilities
int utils_check_internet_connection(void);
int utils_resolve_hostname(const char *hostname);
int utils_download_file_simple(const char *url, const char *dest_path);

// Permission and ownership
int utils_set_file_permissions(const char *path, mode_t mode);
int utils_set_file_owner(const char *path, uid_t uid, gid_t gid);
int utils_get_file_permissions(const char *path, mode_t *mode);
int utils_get_file_owner(const char *path, uid_t *uid, gid_t *gid);

// User and group utilities
uid_t utils_get_user_id(const char *username);
gid_t utils_get_group_id(const char *groupname);
char *utils_get_username(uid_t uid);
char *utils_get_groupname(gid_t gid);
int utils_user_exists(const char *username);
int utils_group_exists(const char *groupname);

// Environment utilities
char *utils_get_env_var(const char *name, const char *default_value);
int utils_set_env_var(const char *name, const char *value);
int utils_unset_env_var(const char *name);
char **utils_get_environment(void);

// Logging helpers (utility functions for logging system)
void utils_log_system_info(void);
void utils_log_environment_info(void);
void utils_log_directory_contents(const char *path);

// Error handling utilities
const char *utils_strerror_safe(int errnum);
void utils_print_error(const char *operation, const char *details);
void utils_print_warning(const char *message);
void utils_print_info(const char *message);

// Validation utilities
int utils_validate_package_name(const char *name);
int utils_validate_version_string(const char *version);
int utils_validate_url(const char *url);
int utils_validate_path(const char *path);
int utils_validate_email(const char *email);

// Random utilities
void utils_init_random(void);
int utils_random_int(int min, int max);
void utils_random_string(char *buffer, size_t length);
char *utils_generate_uuid(void);

// Backup utilities
int utils_create_backup(const char *source_path, const char *backup_dir);
int utils_restore_backup(const char *backup_path, const char *dest_path);
int utils_cleanup_old_backups(const char *backup_dir, int keep_count);

// Debugging utilities
void utils_dump_hex(const void *data, size_t size);
void utils_print_stack_trace(void);
void utils_debug_memory_usage(void);

#endif /* TINYPKG_UTILS_H */
