/*
 * TinyPkg - Utility Functions Implementation
 * Core utility functions for file operations, string handling, etc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <fts.h>
#include <libgen.h>
#include "../include/tinypkg.h"

// Directory operations
int utils_init_directories(void) {
    struct stat st;
    const char *dirs[] = {
        CONFIG_DIR,
        CACHE_DIR,
        LIB_DIR,
        REPO_DIR,
        LOG_DIR
    };
    
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        if (stat(dirs[i], &st) != 0) {
            if (mkdir(dirs[i], 0755) != 0) {
                log_error("Failed to create directory %s: %s", dirs[i], strerror(errno));
                return TINYPKG_ERROR;
            }
            log_debug("Created directory: %s", dirs[i]);
        } else if (!S_ISDIR(st.st_mode)) {
            log_error("Path exists but is not a directory: %s", dirs[i]);
            return TINYPKG_ERROR;
        }
    }
    
    // Create subdirectories
    char subdir[MAX_PATH];
    const char *subdirs[] = {
        "/sources", "/builds", "/packages"
    };
    
    for (size_t i = 0; i < sizeof(subdirs) / sizeof(subdirs[0]); i++) {
        snprintf(subdir, sizeof(subdir), "%s%s", CACHE_DIR, subdirs[i]);
        if (stat(subdir, &st) != 0) {
            if (mkdir(subdir, 0755) != 0) {
                log_warn("Failed to create subdirectory %s: %s", subdir, strerror(errno));
            }
        }
    }
    
    return TINYPKG_SUCCESS;
}

int utils_create_directory_recursive(const char *path) {
    if (!path || strlen(path) == 0) {
        return TINYPKG_ERROR;
    }
    
    char *path_copy = TINYPKG_STRDUP(path);
    if (!path_copy) {
        return TINYPKG_ERROR_MEMORY;
    }
    
    char *p = path_copy;
    if (*p == '/') p++; // Skip leading slash
    
    while (*p) {
        while (*p && *p != '/') p++;
        
        char saved = *p;
        *p = '\0';
        
        if (mkdir(path_copy, 0755) != 0 && errno != EEXIST) {
            log_error("Failed to create directory %s: %s", path_copy, strerror(errno));
            TINYPKG_FREE(path_copy);
            return TINYPKG_ERROR;
        }
        
        *p = saved;
        if (*p) p++;
    }
    
    TINYPKG_FREE(path_copy);
    return TINYPKG_SUCCESS;
}

int utils_remove_directory_recursive(const char *path) {
    if (!path) return TINYPKG_ERROR;
    
    char *paths[] = {(char *)path, NULL};
    FTS *tree = fts_open(paths, FTS_NOCHDIR | FTS_PHYSICAL, NULL);
    if (!tree) return TINYPKG_ERROR;
    
    FTSENT *node;
    int result = TINYPKG_SUCCESS;
    
    while ((node = fts_read(tree))) {
        switch (node->fts_info) {
            case FTS_D:
                break; // Directory - will be removed on FTS_DP
            case FTS_DP:
                if (rmdir(node->fts_accpath) != 0) {
                    log_warn("Failed to remove directory %s: %s", 
                             node->fts_accpath, strerror(errno));
                    result = TINYPKG_ERROR;
                }
                break;
            case FTS_F:
            case FTS_SL:
            case FTS_SLNONE:
                if (unlink(node->fts_accpath) != 0) {
                    log_warn("Failed to remove file %s: %s", 
                             node->fts_accpath, strerror(errno));
                    result = TINYPKG_ERROR;
                }
                break;
            case FTS_ERR:
                log_error("FTS error on %s: %s", 
                          node->fts_accpath, strerror(node->fts_errno));
                result = TINYPKG_ERROR;
                break;
        }
    }
    
    fts_close(tree);
    return result;
}

int utils_directory_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

int utils_file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

int utils_copy_file(const char *src, const char *dest) {
    if (!src || !dest) return TINYPKG_ERROR;
    
    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0) {
        log_error("Failed to open source file %s: %s", src, strerror(errno));
        return TINYPKG_ERROR;
    }
    
    struct stat st;
    if (fstat(src_fd, &st) != 0) {
        close(src_fd);
        return TINYPKG_ERROR;
    }
    
    int dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dest_fd < 0) {
        log_error("Failed to open destination file %s: %s", dest, strerror(errno));
        close(src_fd);
        return TINYPKG_ERROR;
    }
    
    char buffer[8192];
    ssize_t bytes_read, bytes_written;
    int result = TINYPKG_SUCCESS;
    
    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(dest_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            log_error("Write error copying %s to %s", src, dest);
            result = TINYPKG_ERROR;
            break;
        }
    }
    
    if (bytes_read < 0) {
        log_error("Read error copying %s: %s", src, strerror(errno));
        result = TINYPKG_ERROR;
    }
    
    close(src_fd);
    close(dest_fd);
    return result;
}

// String utilities
char *utils_string_duplicate(const char *str) {
    return str ? TINYPKG_STRDUP(str) : NULL;
}

char *utils_string_trim(char *str) {
    if (!str) return NULL;
    
    // Trim leading whitespace
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    // Trim trailing whitespace
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    
    return str;
}

int utils_string_starts_with(const char *str, const char *prefix) {
    if (!str || !prefix) return 0;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

int utils_string_ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    
    if (suffix_len > str_len) return 0;
    
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

char **utils_string_split(const char *str, const char *delimiter, int *count) {
    if (!str || !delimiter || !count) return NULL;
    
    *count = 0;
    char *str_copy = TINYPKG_STRDUP(str);
    if (!str_copy) return NULL;
    
    // Count tokens
    char *token = strtok(str_copy, delimiter);
    while (token) {
        (*count)++;
        token = strtok(NULL, delimiter);
    }
    TINYPKG_FREE(str_copy);
    
    if (*count == 0) return NULL;
    
    // Allocate array
    char **result = TINYPKG_MALLOC(sizeof(char*) * (*count));
    if (!result) {
        *count = 0;
        return NULL;
    }
    
    // Split again and store tokens
    str_copy = TINYPKG_STRDUP(str);
    token = strtok(str_copy, delimiter);
    int i = 0;
    while (token && i < *count) {
        result[i] = TINYPKG_STRDUP(token);
        if (!result[i]) {
            utils_string_array_free(result, i);
            TINYPKG_FREE(str_copy);
            *count = 0;
            return NULL;
        }
        token = strtok(NULL, delimiter);
        i++;
    }
    
    TINYPKG_FREE(str_copy);
    return result;
}

void utils_string_array_free(char **array, int count) {
    if (!array) return;
    
    for (int i = 0; i < count; i++) {
        TINYPKG_FREE(array[i]);
    }
    TINYPKG_FREE(array);
}

// Path utilities
char *utils_join_path(const char *dir, const char *file) {
    if (!dir || !file) return NULL;
    
    size_t dir_len = strlen(dir);
    size_t file_len = strlen(file);
    size_t total_len = dir_len + file_len + 2;
    
    char *result = TINYPKG_MALLOC(total_len);
    if (!result) return NULL;
    
    strcpy(result, dir);
    if (dir_len > 0 && dir[dir_len - 1] != '/') {
        strcat(result, "/");
    }
    strcat(result, file);
    
    return result;
}

char *utils_get_basename(const char *path) {
    if (!path) return NULL;
    
    char *path_copy = TINYPKG_STRDUP(path);
    if (!path_copy) return NULL;
    
    char *base = basename(path_copy);
    char *result = TINYPKG_STRDUP(base);
    
    TINYPKG_FREE(path_copy);
    return result;
}

char *utils_get_dirname(const char *path) {
    if (!path) return NULL;
    
    char *path_copy = TINYPKG_STRDUP(path);
    if (!path_copy) return NULL;
    
    char *dir = dirname(path_copy);
    char *result = TINYPKG_STRDUP(dir);
    
    TINYPKG_FREE(path_copy);
    return result;
}

// Command execution
int utils_run_command(const char *cmd, const char *work_dir) {
    if (!cmd) return TINYPKG_ERROR;
    
    log_debug("Executing command: %s", cmd);
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        if (work_dir && chdir(work_dir) != 0) {
            log_error("Failed to change directory to %s: %s", work_dir, strerror(errno));
            exit(1);
        }
        
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        log_error("Failed to execute command: %s", strerror(errno));
        exit(1);
    } else if (pid > 0) {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            log_error("Failed to wait for child process: %s", strerror(errno));
            return TINYPKG_ERROR;
        }
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            log_debug("Command exited with code: %d", exit_code);
            return (exit_code == 0) ? TINYPKG_SUCCESS : TINYPKG_ERROR;
        } else {
            log_error("Command terminated abnormally");
            return TINYPKG_ERROR;
        }
    } else {
        log_error("Failed to fork process: %s", strerror(errno));
        return TINYPKG_ERROR;
    }
}

int utils_run_command_with_output(const char *cmd, const char *work_dir, 
                                 char **output, int *exit_code) {
    if (!cmd || !output || !exit_code) return TINYPKG_ERROR;
    
    *output = NULL;
    *exit_code = -1;
    
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        log_error("Failed to create pipe: %s", strerror(errno));
        return TINYPKG_ERROR;
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        close(pipefd[0]); // Close read end
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        
        if (work_dir && chdir(work_dir) != 0) {
            fprintf(stderr, "Failed to change directory to %s: %s\n", 
                    work_dir, strerror(errno));
            exit(1);
        }
        
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        fprintf(stderr, "Failed to execute command: %s\n", strerror(errno));
        exit(1);
    } else if (pid > 0) {
        // Parent process
        close(pipefd[1]); // Close write end
        
        // Read output
        char buffer[4096];
        size_t total_size = 0;
        size_t buffer_size = 4096;
        *output = TINYPKG_MALLOC(buffer_size);
        
        if (!*output) {
            close(pipefd[0]);
            waitpid(pid, NULL, 0);
            return TINYPKG_ERROR_MEMORY;
        }
        
        (*output)[0] = '\0';
        
        ssize_t bytes_read;
        while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            
            size_t needed = total_size + bytes_read + 1;
            if (needed > buffer_size) {
                buffer_size = needed * 2;
                char *new_output = TINYPKG_REALLOC(*output, buffer_size);
                if (!new_output) {
                    TINYPKG_FREE(*output);
                    *output = NULL;
                    close(pipefd[0]);
                    waitpid(pid, NULL, 0);
                    return TINYPKG_ERROR_MEMORY;
                }
                *output = new_output;
            }
            
            strcat(*output, buffer);
            total_size += bytes_read;
        }
        
        close(pipefd[0]);
        
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            log_error("Failed to wait for child process: %s", strerror(errno));
            TINYPKG_FREE(*output);
            *output = NULL;
            return TINYPKG_ERROR;
        }
        
        if (WIFEXITED(status)) {
            *exit_code = WEXITSTATUS(status);
            return TINYPKG_SUCCESS;
        } else {
            *exit_code = -1;
            return TINYPKG_ERROR;
        }
    } else {
        log_error("Failed to fork process: %s", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return TINYPKG_ERROR;
    }
}

// Progress display
void utils_progress_init(progress_info_t *progress, size_t total, const char *message) {
    if (!progress) return;
    
    memset(progress, 0, sizeof(progress_info_t));
    progress->total = total;
    progress->start_time = time(NULL);
    progress->last_update = progress->start_time;
    
    if (message) {
        strncpy(progress->message, message, sizeof(progress->message) - 1);
        progress->message[sizeof(progress->message) - 1] = '\0';
    }
}

void utils_progress_update(progress_info_t *progress, size_t current) {
    if (!progress) return;
    
    progress->current = current;
    time_t now = time(NULL);
    
    if (progress->total > 0) {
        progress->percentage = (int)((current * 100) / progress->total);
    }
    
    if (now > progress->start_time && current > 0) {
        progress->rate = (double)current / (now - progress->start_time);
        
        if (progress->rate > 0 && progress->total > current) {
            progress->eta_seconds = (int)((progress->total - current) / progress->rate);
        }
    }
    
    progress->last_update = now;
}

void utils_progress_display(const progress_info_t *progress) {
    if (!progress) return;
    
    printf("\r%s: %zu/%zu (%d%%) ", 
           progress->message, progress->current, progress->total, progress->percentage);
    
    // Show progress bar
    int bar_width = 30;
    int filled = (progress->percentage * bar_width) / 100;
    
    printf("[");
    for (int i = 0; i < bar_width; i++) {
        printf("%c", (i < filled) ? '=' : ' ');
    }
    printf("]");
    
    if (progress->rate > 0) {
        printf(" %.1f/s", progress->rate);
    }
    
    if (progress->eta_seconds > 0) {
        printf(" ETA: %ds", progress->eta_seconds);
    }
    
    fflush(stdout);
}

void utils_progress_finish(progress_info_t *progress) {
    if (!progress) return;
    
    printf("\n");
    fflush(stdout);
}

// Clean cache
int utils_clean_cache(void) {
    log_info("Cleaning build cache...");
    
    char cache_paths[][MAX_PATH] = {
        "%s/sources",
        "%s/builds",
        "%s/packages"
    };
    
    int result = TINYPKG_SUCCESS;
    
    for (size_t i = 0; i < sizeof(cache_paths) / sizeof(cache_paths[0]); i++) {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), cache_paths[i], CACHE_DIR);
        
        if (utils_directory_exists(path)) {
            log_debug("Cleaning directory: %s", path);
            if (utils_remove_directory_recursive(path) != TINYPKG_SUCCESS) {
                log_warn("Failed to clean directory: %s", path);
                result = TINYPKG_ERROR;
            } else {
                // Recreate the directory
                if (mkdir(path, 0755) != 0) {
                    log_warn("Failed to recreate directory: %s", path);
                }
            }
        }
    }
    
    return result;
}

// Format size utility
void utils_format_size(size_t bytes) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = (double)bytes;
    int unit = 0;
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    if (unit == 0) {
        printf("%.0f %s", size, units[unit]);
    } else {
        printf("%.1f %s", size, units[unit]);
    }
}

// Format time utility  
void utils_format_time(time_t timestamp) {
    struct tm *tm_info = localtime(&timestamp);
    char buffer[64];
    
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    printf("%s", buffer);
}

char *utils_get_timestamp_string(void) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char *buffer = TINYPKG_MALLOC(64);
    
    if (!buffer) return NULL;
    
    strftime(buffer, 64, "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}
