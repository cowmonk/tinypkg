/*
 * TinyPkg - Configuration System Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include "../include/tinypkg.h"

// Default configuration values
static const char *DEFAULT_CONFIG_TEMPLATE = 
"# TinyPkg Configuration File\n"
"# Generated automatically - modify as needed\n\n"

"[general]\n"
"root_dir = /\n"
"parallel_jobs = %d\n"
"force_mode = false\n"
"assume_yes = false\n"
"skip_dependencies = false\n"
"verify_checksums = true\n"
"verify_signatures = true\n"
"create_backups = true\n\n"

"[repository]\n"
"repo_url = %s\n"
"repo_branch = %s\n"
"auto_sync = true\n"
"sync_interval = 3600\n\n"

"[build]\n"
"build_timeout = 3600\n"
"enable_optimizations = true\n"
"debug_symbols = false\n"
"keep_build_dir = false\n"
"install_prefix = /usr/local\n"
"build_flags = -O2 -march=native\n\n"

"[security]\n"
"sandbox_builds = true\n"
"sandbox_user = nobody\n"
"sandbox_group = nobody\n\n"

"[logging]\n"
"log_level = info\n"
"log_to_file = true\n"
"log_to_syslog = true\n"
"log_colors = true\n"
"max_log_size = 10485760\n"
"max_log_files = 5\n\n"

"[network]\n"
"connection_timeout = 30\n"
"max_retries = 3\n"
"verify_ssl = true\n"
"max_concurrent_downloads = 4\n"
"user_agent = TinyPkg/%s\n\n";

// Global variables
static config_parser_t *g_parser = NULL;
static repo_config_t *g_repositories = NULL;
static int g_repo_count = 0;
static mirror_config_t *g_mirrors = NULL;
static int g_mirror_count = 0;

// Helper functions
static char *trim_whitespace(char *str) {
    char *end;
    
    // Trim leading space
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    *(end + 1) = 0;
    return str;
}

static int create_directory_if_needed(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    return mkdir(path, 0755);
}

static void set_default_paths(config_t *config) {
    snprintf(config->root_dir, sizeof(config->root_dir), "/");
    snprintf(config->config_dir, sizeof(config->config_dir), CONFIG_DIR);
    snprintf(config->cache_dir, sizeof(config->cache_dir), CACHE_DIR);
    snprintf(config->lib_dir, sizeof(config->lib_dir), LIB_DIR);
    snprintf(config->log_dir, sizeof(config->log_dir), LOG_DIR);
    snprintf(config->build_dir, sizeof(config->build_dir), BUILD_DIR);
    snprintf(config->repo_dir, sizeof(config->repo_dir), REPO_DIR);
    snprintf(config->log_file, sizeof(config->log_file), "%s/tinypkg.log", LOG_DIR);
}

// Configuration management
config_t *config_create_default(void) {
    config_t *config = TINYPKG_CALLOC(1, sizeof(config_t));
    if (!config) return NULL;
    
    // Set default paths
    set_default_paths(config);
    
    // Repository settings
    strncpy(config->repo_url, DEFAULT_REPO_URL, sizeof(config->repo_url) - 1);
    strncpy(config->repo_branch, REPO_BRANCH, sizeof(config->repo_branch) - 1);
    config->auto_sync = 1;
    config->sync_interval = 3600;
    
    // Build settings
    config->parallel_jobs = config_detect_cpu_count();
    if (config->parallel_jobs <= 0) config->parallel_jobs = DEFAULT_PARALLEL_JOBS;
    config->build_timeout = BUILD_TIMEOUT;
    config->enable_optimizations = 1;
    config->debug_symbols = 0;
    config->keep_build_dir = 0;
    strncpy(config->install_prefix, "/usr/local", sizeof(config->install_prefix) - 1);
    strncpy(config->build_flags, "-O2 -march=native", sizeof(config->build_flags) - 1);
    
    // Package settings
    config->force_mode = 0;
    config->assume_yes = 0;
    config->skip_dependencies = 0;
    config->verify_checksums = 1;
    config->verify_signatures = 1;
    config->create_backups = 1;
    
    // Security settings
    config->sandbox_builds = 1;
    strncpy(config->sandbox_user, "nobody", sizeof(config->sandbox_user) - 1);
    strncpy(config->sandbox_group, "nobody", sizeof(config->sandbox_group) - 1);
    
    // Logging settings
    config->log_level = LOG_INFO;
    config->log_to_file = 1;
    config->log_to_syslog = 1;
    config->log_colors = 1;
    config->max_log_size = 10 * 1024 * 1024;
    config->max_log_files = 5;
    
    // Network settings
    config->connection_timeout = 30;
    config->max_retries = 3;
    config->verify_ssl = 1;
    config->max_concurrent_downloads = 4;
    snprintf(config->user_agent, sizeof(config->user_agent), 
             "TinyPkg/%s", TINYPKG_VERSION);
    
    // Advanced settings
    config->compression_level = 6;
    config->use_progress_bar = 1;
    config->show_package_sizes = 1;
    config->cleanup_on_failure = 1;
    
    return config;
}

config_t *config_load(void) {
    char config_file[MAX_PATH];
    
    // Try user config first, then system config
    char *user_config = config_get_user_config_dir();
    if (user_config) {
        snprintf(config_file, sizeof(config_file), "%s/tinypkg.conf", user_config);
        free(user_config);
        if (access(config_file, R_OK) == 0) {
            return config_load_file(config_file);
        }
    }
    
    // Try system config
    snprintf(config_file, sizeof(config_file), "%s/tinypkg.conf", CONFIG_DIR);
    if (access(config_file, R_OK) == 0) {
        return config_load_file(config_file);
    }
    
    return NULL;
}

config_t *config_load_file(const char *config_file) {
    if (!config_file) return NULL;
    
    config_t *config = config_create_default();
    if (!config) return NULL;
    
    g_parser = config_parser_create();
    if (!g_parser) {
        config_free(config);
        return NULL;
    }
    
    if (config_parser_load_file(g_parser, config_file) != 0) {
        log_error("Failed to parse config file: %s", config_file);
        config_parser_free(g_parser);
        g_parser = NULL;
        return config; // Return default config
    }
    
    // Parse configuration values
    const char *value;
    
    // General settings
    if ((value = config_parser_get_value(g_parser, "general", "root_dir"))) {
        strncpy(config->root_dir, value, sizeof(config->root_dir) - 1);
    }
    
    if ((value = config_parser_get_value(g_parser, "general", "parallel_jobs"))) {
        config->parallel_jobs = atoi(value);
        if (config->parallel_jobs <= 0) config->parallel_jobs = DEFAULT_PARALLEL_JOBS;
    }
    
    if ((value = config_parser_get_value(g_parser, "general", "force_mode"))) {
        config->force_mode = (strcasecmp(value, "true") == 0) ? 1 : 0;
    }
    
    if ((value = config_parser_get_value(g_parser, "general", "assume_yes"))) {
        config->assume_yes = (strcasecmp(value, "true") == 0) ? 1 : 0;
    }
    
    if ((value = config_parser_get_value(g_parser, "general", "skip_dependencies"))) {
        config->skip_dependencies = (strcasecmp(value, "true") == 0) ? 1 : 0;
    }
    
    // Repository settings
    if ((value = config_parser_get_value(g_parser, "repository", "repo_url"))) {
        strncpy(config->repo_url, value, sizeof(config->repo_url) - 1);
    }
    
    if ((value = config_parser_get_value(g_parser, "repository", "repo_branch"))) {
        strncpy(config->repo_branch, value, sizeof(config->repo_branch) - 1);
    }
    
    if ((value = config_parser_get_value(g_parser, "repository", "auto_sync"))) {
        config->auto_sync = (strcasecmp(value, "true") == 0) ? 1 : 0;
    }
    
    // Build settings
    if ((value = config_parser_get_value(g_parser, "build", "build_timeout"))) {
        config->build_timeout = atoi(value);
    }
    
    if ((value = config_parser_get_value(g_parser, "build", "install_prefix"))) {
        strncpy(config->install_prefix, value, sizeof(config->install_prefix) - 1);
    }
    
    if ((value = config_parser_get_value(g_parser, "build", "build_flags"))) {
        strncpy(config->build_flags, value, sizeof(config->build_flags) - 1);
    }
    
    // Security settings
    if ((value = config_parser_get_value(g_parser, "security", "sandbox_builds"))) {
        config->sandbox_builds = (strcasecmp(value, "true") == 0) ? 1 : 0;
    }
    
    if ((value = config_parser_get_value(g_parser, "security", "sandbox_user"))) {
        strncpy(config->sandbox_user, value, sizeof(config->sandbox_user) - 1);
    }
    
    // Logging settings
    if ((value = config_parser_get_value(g_parser, "logging", "log_level"))) {
        config->log_level = log_level_from_string(value);
    }
    
    if ((value = config_parser_get_value(g_parser, "logging", "log_to_file"))) {
        config->log_to_file = (strcasecmp(value, "true") == 0) ? 1 : 0;
    }
    
    // Network settings
    if ((value = config_parser_get_value(g_parser, "network", "connection_timeout"))) {
        config->connection_timeout = atoi(value);
    }
    
    if ((value = config_parser_get_value(g_parser, "network", "max_retries"))) {
        config->max_retries = atoi(value);
    }
    
    if ((value = config_parser_get_value(g_parser, "network", "verify_ssl"))) {
        config->verify_ssl = (strcasecmp(value, "true") == 0) ? 1 : 0;
    }
    
    if ((value = config_parser_get_value(g_parser, "network", "user_agent"))) {
        strncpy(config->user_agent, value, sizeof(config->user_agent) - 1);
    }
    
    // Update paths based on root_dir if changed
    if (strcmp(config->root_dir, "/") != 0) {
        snprintf(config->config_dir, sizeof(config->config_dir), 
                 "%s%s", config->root_dir, CONFIG_DIR);
        snprintf(config->cache_dir, sizeof(config->cache_dir), 
                 "%s%s", config->root_dir, CACHE_DIR);
        snprintf(config->lib_dir, sizeof(config->lib_dir), 
                 "%s%s", config->root_dir, LIB_DIR);
        snprintf(config->log_dir, sizeof(config->log_dir), 
                 "%s%s", config->root_dir, LOG_DIR);
        snprintf(config->repo_dir, sizeof(config->repo_dir), 
                 "%s%s", config->root_dir, REPO_DIR);
        snprintf(config->log_file, sizeof(config->log_file), 
                 "%s/tinypkg.log", config->log_dir);
    }
    
    return config;
}

int config_save(const config_t *config) {
    if (!config) return -1;
    
    char config_file[MAX_PATH];
    snprintf(config_file, sizeof(config_file), "%s/tinypkg.conf", config->config_dir);
    
    return config_save_file(config, config_file);
}

int config_save_file(const config_t *config, const char *config_file) {
    if (!config || !config_file) return -1;
    
    FILE *fp = fopen(config_file, "w");
    if (!fp) {
        log_error("Failed to open config file for writing: %s", config_file);
        return -1;
    }
    
    fprintf(fp, "# TinyPkg Configuration File\n");
    fprintf(fp, "# Generated by TinyPkg %s\n\n", TINYPKG_VERSION);
    
    fprintf(fp, "[general]\n");
    fprintf(fp, "root_dir = %s\n", config->root_dir);
    fprintf(fp, "parallel_jobs = %d\n", config->parallel_jobs);
    fprintf(fp, "force_mode = %s\n", config->force_mode ? "true" : "false");
    fprintf(fp, "assume_yes = %s\n", config->assume_yes ? "true" : "false");
    fprintf(fp, "skip_dependencies = %s\n", config->skip_dependencies ? "true" : "false");
    fprintf(fp, "verify_checksums = %s\n", config->verify_checksums ? "true" : "false");
    fprintf(fp, "verify_signatures = %s\n", config->verify_signatures ? "true" : "false");
    fprintf(fp, "create_backups = %s\n\n", config->create_backups ? "true" : "false");
    
    fprintf(fp, "[repository]\n");
    fprintf(fp, "repo_url = %s\n", config->repo_url);
    fprintf(fp, "repo_branch = %s\n", config->repo_branch);
    fprintf(fp, "auto_sync = %s\n", config->auto_sync ? "true" : "false");
    fprintf(fp, "sync_interval = %d\n\n", config->sync_interval);
    
    fprintf(fp, "[build]\n");
    fprintf(fp, "build_timeout = %d\n", config->build_timeout);
    fprintf(fp, "enable_optimizations = %s\n", config->enable_optimizations ? "true" : "false");
    fprintf(fp, "debug_symbols = %s\n", config->debug_symbols ? "true" : "false");
    fprintf(fp, "keep_build_dir = %s\n", config->keep_build_dir ? "true" : "false");
    fprintf(fp, "install_prefix = %s\n", config->install_prefix);
    fprintf(fp, "build_flags = %s\n\n", config->build_flags);
    
    fprintf(fp, "[security]\n");
    fprintf(fp, "sandbox_builds = %s\n", config->sandbox_builds ? "true" : "false");
    fprintf(fp, "sandbox_user = %s\n", config->sandbox_user);
    fprintf(fp, "sandbox_group = %s\n\n", config->sandbox_group);
    
    fprintf(fp, "[logging]\n");
    fprintf(fp, "log_level = %s\n", log_level_to_string(config->log_level));
    fprintf(fp, "log_to_file = %s\n", config->log_to_file ? "true" : "false");
    fprintf(fp, "log_to_syslog = %s\n", config->log_to_syslog ? "true" : "false");
    fprintf(fp, "log_colors = %s\n", config->log_colors ? "true" : "false");
    fprintf(fp, "max_log_size = %d\n", config->max_log_size);
    fprintf(fp, "max_log_files = %d\n\n", config->max_log_files);
    
    fprintf(fp, "[network]\n");
    fprintf(fp, "connection_timeout = %d\n", config->connection_timeout);
    fprintf(fp, "max_retries = %d\n", config->max_retries);
    fprintf(fp, "verify_ssl = %s\n", config->verify_ssl ? "true" : "false");
    fprintf(fp, "max_concurrent_downloads = %d\n", config->max_concurrent_downloads);
    fprintf(fp, "user_agent = %s\n", config->user_agent);
    
    if (strlen(config->proxy_url) > 0) {
        fprintf(fp, "proxy_url = %s\n", config->proxy_url);
    }
    
    fclose(fp);
    return 0;
}

void config_free(config_t *config) {
    if (!config) return;
    
    if (config->mirrors) {
        for (int i = 0; i < config->mirror_count; i++) {
            TINYPKG_FREE(config->mirrors[i]);
        }
        TINYPKG_FREE(config->mirrors);
    }
    
    TINYPKG_FREE(config);
    
    if (g_parser) {
        config_parser_free(g_parser);
        g_parser = NULL;
    }
    
    if (g_repositories) {
        TINYPKG_FREE(g_repositories);
        g_repositories = NULL;
        g_repo_count = 0;
    }
    
    if (g_mirrors) {
        TINYPKG_FREE(g_mirrors);
        g_mirrors = NULL;
        g_mirror_count = 0;
    }
}

int config_validate(const config_t *config) {
    if (!config) return -1;
    
    // Check required directories exist and are writable
    if (access(config->cache_dir, W_OK) != 0) {
        log_error("Cache directory not writable: %s", config->cache_dir);
        return -1;
    }
    
    if (access(config->lib_dir, W_OK) != 0) {
        log_error("Library directory not writable: %s", config->lib_dir);
        return -1;
    }
    
    if (access(config->log_dir, W_OK) != 0) {
        log_error("Log directory not writable: %s", config->log_dir);
        return -1;
    }
    
    // Validate numeric ranges
    if (config->parallel_jobs < 1 || config->parallel_jobs > 64) {
        log_error("Invalid parallel_jobs value: %d", config->parallel_jobs);
        return -1;
    }
    
    if (config->build_timeout < 60 || config->build_timeout > 86400) {
        log_error("Invalid build_timeout value: %d", config->build_timeout);
        return -1;
    }
    
    if (config->connection_timeout < 5 || config->connection_timeout > 300) {
        log_error("Invalid connection_timeout value: %d", config->connection_timeout);
        return -1;
    }
    
    // Validate URLs
    if (strlen(config->repo_url) == 0) {
        log_error("Repository URL cannot be empty");
        return -1;
    }
    
    return 0;
}

int config_create_directories(const config_t *config) {
    if (!config) return -1;
    
    int result = 0;
    
    result |= create_directory_if_needed(config->config_dir);
    result |= create_directory_if_needed(config->cache_dir);
    result |= create_directory_if_needed(config->lib_dir);
    result |= create_directory_if_needed(config->log_dir);
    result |= create_directory_if_needed(config->build_dir);
    result |= create_directory_if_needed(config->repo_dir);
    
    // Create subdirectories
    char path[MAX_PATH];
    
    snprintf(path, sizeof(path), "%s/sources", config->cache_dir);
    result |= create_directory_if_needed(path);
    
    snprintf(path, sizeof(path), "%s/builds", config->cache_dir);
    result |= create_directory_if_needed(path);
    
    snprintf(path, sizeof(path), "%s/packages", config->cache_dir);
    result |= create_directory_if_needed(path);
    
    return result;
}

int config_detect_cpu_count(void) {
    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count <= 0) cpu_count = 1;
    if (cpu_count > 32) cpu_count = 32; // Reasonable maximum
    return (int)cpu_count;
}

size_t config_detect_memory_size(void) {
    long page_size = sysconf(_SC_PAGESIZE);
    long num_pages = sysconf(_SC_PHYS_PAGES);
    
    if (page_size > 0 && num_pages > 0) {
        return (size_t)(page_size * num_pages);
    }
    
    return 0;
}

int config_detect_architecture(char *arch, size_t size) {
    struct utsname uts;
    if (uname(&uts) != 0) return -1;
    
    strncpy(arch, uts.machine, size - 1);
    arch[size - 1] = '\0';
    return 0;
}

int config_detect_distribution(char *distro, size_t size) {
    FILE *fp;
    char line[256];
    
    // Try /etc/os-release first
    fp = fopen("/etc/os-release", "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "ID=", 3) == 0) {
                char *value = line + 3;
                value = trim_whitespace(value);
                // Remove quotes if present
                if (value[0] == '"') {
                    value++;
                    char *end = strchr(value, '"');
                    if (end) *end = '\0';
                }
                strncpy(distro, value, size - 1);
                distro[size - 1] = '\0';
                fclose(fp);
                return 0;
            }
        }
        fclose(fp);
    }
    
    // Fallback to lsb_release
    fp = popen("lsb_release -si 2>/dev/null", "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp)) {
            char *value = trim_whitespace(line);
            strncpy(distro, value, size - 1);
            distro[size - 1] = '\0';
            pclose(fp);
            return 0;
        }
        pclose(fp);
    }
    
    // Last resort - check for specific files
    if (access("/etc/debian_version", R_OK) == 0) {
        strncpy(distro, "debian", size - 1);
        return 0;
    }
    
    if (access("/etc/redhat-release", R_OK) == 0) {
        strncpy(distro, "redhat", size - 1);
        return 0;
    }
    
    if (access("/etc/arch-release", R_OK) == 0) {
        strncpy(distro, "arch", size - 1);
        return 0;
    }
    
    strncpy(distro, "unknown", size - 1);
    distro[size - 1] = '\0';
    return -1;
}

char *config_get_user_config_dir(void) {
    char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    
    if (!home) return NULL;
    
    char *config_dir = TINYPKG_MALLOC(strlen(home) + 32);
    if (!config_dir) return NULL;
    
    sprintf(config_dir, "%s/.config/tinypkg", home);
    return config_dir;
}

char *config_get_system_config_dir(void) {
    return TINYPKG_STRDUP(CONFIG_DIR);
}

int config_generate_default_file(const char *filename) {
    if (!filename) return -1;
    
    // Create directory if needed
    char dir[MAX_PATH];
    strcpy(dir, filename);
    char *last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        create_directory_if_needed(dir);
    }
    
    FILE *fp = fopen(filename, "w");
    if (!fp) return -1;
    
    int cpu_count = config_detect_cpu_count();
    
    fprintf(fp, DEFAULT_CONFIG_TEMPLATE, 
            cpu_count, DEFAULT_REPO_URL, REPO_BRANCH, TINYPKG_VERSION);
    
    fclose(fp);
    return 0;
}

// Configuration parser implementation
config_parser_t *config_parser_create(void) {
    config_parser_t *parser = TINYPKG_CALLOC(1, sizeof(config_parser_t));
    return parser;
}

void config_parser_free(config_parser_t *parser) {
    if (!parser) return;
    
    config_section_t *section = parser->sections;
    while (section) {
        config_section_t *next_section = section->next;
        
        config_entry_t *entry = section->entries;
        while (entry) {
            config_entry_t *next_entry = entry->next;
            TINYPKG_FREE(entry);
            entry = next_entry;
        }
        
        TINYPKG_FREE(section);
        section = next_section;
    }
    
    TINYPKG_FREE(parser);
}

int config_parser_load_file(config_parser_t *parser, const char *filename) {
    if (!parser || !filename) return -1;
    
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        snprintf(parser->error_message, sizeof(parser->error_message),
                 "Cannot open file: %s", filename);
        return -1;
    }
    
    char line[1024];
    config_section_t *current_section = NULL;
    int line_number = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        line_number++;
        
        char *trimmed = trim_whitespace(line);
        
        // Skip empty lines and comments
        if (strlen(trimmed) == 0 || trimmed[0] == '#') {
            continue;
        }
        
        // Check for section header
        if (trimmed[0] == '[') {
            char *end = strchr(trimmed, ']');
            if (!end) {
                snprintf(parser->error_message, sizeof(parser->error_message),
                         "Invalid section header at line %d", line_number);
                fclose(fp);
                return -1;
            }
            
            *end = '\0';
            char *section_name = trimmed + 1;
            
            // Create new section
            config_section_t *section = TINYPKG_CALLOC(1, sizeof(config_section_t));
            if (!section) {
                fclose(fp);
                return -1;
            }
            
            strncpy(section->name, section_name, sizeof(section->name) - 1);
            section->next = parser->sections;
            parser->sections = section;
            current_section = section;
            continue;
        }
        
        // Parse key=value pair
        char *equals = strchr(trimmed, '=');
        if (!equals) {
            snprintf(parser->error_message, sizeof(parser->error_message),
                     "Invalid configuration line at %d: %s", line_number, trimmed);
            fclose(fp);
            return -1;
        }
        
        if (!current_section) {
            snprintf(parser->error_message, sizeof(parser->error_message),
                     "Configuration entry outside of section at line %d", line_number);
            fclose(fp);
            return -1;
        }
        
        *equals = '\0';
        char *key = trim_whitespace(trimmed);
        char *value = trim_whitespace(equals + 1);
        
        // Remove quotes from value if present
        if (value[0] == '"' || value[0] == '\'') {
            char quote = value[0];
            value++;
            char *end = strrchr(value, quote);
            if (end) *end = '\0';
        }
        
        // Create new entry
        config_entry_t *entry = TINYPKG_CALLOC(1, sizeof(config_entry_t));
        if (!entry) {
            fclose(fp);
            return -1;
        }
        
        strncpy(entry->key, key, sizeof(entry->key) - 1);
        strncpy(entry->value, value, sizeof(entry->value) - 1);
        entry->next = current_section->entries;
        current_section->entries = entry;
    }
    
    fclose(fp);
    return 0;
}

const char *config_parser_get_value(config_parser_t *parser, 
                                   const char *section_name, 
                                   const char *key) {
    if (!parser || !section_name || !key) return NULL;
    
    config_section_t *section = parser->sections;
    while (section) {
        if (strcmp(section->name, section_name) == 0) {
            config_entry_t *entry = section->entries;
            while (entry) {
                if (strcmp(entry->key, key) == 0) {
                    return entry->value;
                }
                entry = entry->next;
            }
            break;
        }
        section = section->next;
    }
    
    return NULL;
}

int config_parser_set_value(config_parser_t *parser, 
                           const char *section_name, 
                           const char *key, 
                           const char *value) {
    if (!parser || !section_name || !key || !value) return -1;
    
    config_section_t *section = parser->sections;
    while (section) {
        if (strcmp(section->name, section_name) == 0) {
            break;
        }
        section = section->next;
    }
    
    // Create section if it doesn't exist
    if (!section) {
        section = TINYPKG_CALLOC(1, sizeof(config_section_t));
        if (!section) return -1;
        
        strncpy(section->name, section_name, sizeof(section->name) - 1);
        section->next = parser->sections;
        parser->sections = section;
    }
    
    // Look for existing entry
    config_entry_t *entry = section->entries;
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            strncpy(entry->value, value, sizeof(entry->value) - 1);
            return 0;
        }
        entry = entry->next;
    }
    
    // Create new entry
    entry = TINYPKG_CALLOC(1, sizeof(config_entry_t));
    if (!entry) return -1;
    
    strncpy(entry->key, key, sizeof(entry->key) - 1);
    strncpy(entry->value, value, sizeof(entry->value) - 1);
    entry->next = section->entries;
    section->entries = entry;
    
    return 0;
}
