/*
 * TinyPkg - Repository Management Implementation
 * Git repository handling and package database synchronization
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include "../include/tinypkg.h"

// Global repository list
static repository_t *repositories = NULL;
static int repository_count = 0;
static int repositories_loaded = 0;

// Default repository
static const repository_t default_repo = {
    .name = "main",
    .url = DEFAULT_REPO_URL,
    .branch = REPO_BRANCH,
    .local_path = REPO_DIR,
    .priority = 100,
    .enabled = 1,
    .last_sync = 0,
    .last_commit = {0}
};

// Helper functions
static int load_repositories(void) {
    if (repositories_loaded) return TINYPKG_SUCCESS;
    
    // For now, just use the default repository
    // In a full implementation, this would load from config files
    repositories = TINYPKG_MALLOC(sizeof(repository_t));
    if (!repositories) return TINYPKG_ERROR_MEMORY;
    
    repositories[0] = default_repo;
    repository_count = 1;
    repositories_loaded = 1;
    
    log_debug("Loaded %d repositories", repository_count);
    return TINYPKG_SUCCESS;
}

static int is_git_command_available(void) {
    int exit_code;
    char *output = NULL;
    
    int result = utils_run_command_with_output("git --version", NULL, &output, &exit_code);
    
    if (output) TINYPKG_FREE(output);
    
    return (result == TINYPKG_SUCCESS && exit_code == 0);
}

static int create_directory_for_repo(const char *path) {
    char *parent_dir = utils_get_dirname(path);
    if (!parent_dir) return TINYPKG_ERROR;
    
    int result = utils_create_directory_recursive(parent_dir);
    TINYPKG_FREE(parent_dir);
    
    return result;
}

// Public functions
int repository_init(void) {
    log_debug("Initializing repository system");
    
    // Check if git is available
    if (!is_git_command_available()) {
        log_error("Git command not available. Please install git.");
        return TINYPKG_ERROR;
    }
    
    return load_repositories();
}

void repository_cleanup(void) {
    if (repositories) {
        TINYPKG_FREE(repositories);
        repositories = NULL;
        repository_count = 0;
        repositories_loaded = 0;
    }
}

int repository_sync(void) {
    log_info("Synchronizing package repositories");
    
    if (load_repositories() != TINYPKG_SUCCESS) {
        return TINYPKG_ERROR;
    }
    
    int success_count = 0;
    int total_count = 0;
    
    for (int i = 0; i < repository_count; i++) {
        if (!repositories[i].enabled) continue;
        
        total_count++;
        log_info("Syncing repository: %s", repositories[i].name);
        
        if (repository_sync_specific(repositories[i].name) == TINYPKG_SUCCESS) {
            success_count++;
        }
    }
    
    log_info("Repository sync completed: %d/%d successful", success_count, total_count);
    
    return (success_count == total_count) ? TINYPKG_SUCCESS : TINYPKG_ERROR;
}

int repository_sync_specific(const char *repo_name) {
    if (!repo_name) return TINYPKG_ERROR;
    
    repository_t *repo = repository_get_by_name(repo_name);
    if (!repo) {
        log_error("Repository not found: %s", repo_name);
        return TINYPKG_ERROR;
    }
    
    if (!repo->enabled) {
        log_warn("Repository is disabled: %s", repo_name);
        return TINYPKG_SUCCESS;
    }
    
    struct stat st;
    int result;
    
    if (stat(repo->local_path, &st) == 0) {
        // Repository exists, try to update it
        if (repository_is_git_repo(repo->local_path)) {
            log_debug("Pulling updates for repository: %s", repo->name);
            result = repository_pull(repo->local_path);
        } else {
            log_warn("Local path exists but is not a git repository: %s", repo->local_path);
            log_info("Removing and re-cloning repository");
            utils_remove_directory_recursive(repo->local_path);
            result = repository_clone(repo->url, repo->branch, repo->local_path);
        }
    } else {
        // Repository doesn't exist, clone it
        log_debug("Cloning repository: %s from %s", repo->name, repo->url);
        create_directory_for_repo(repo->local_path);
        result = repository_clone(repo->url, repo->branch, repo->local_path);
    }
    
    if (result == TINYPKG_SUCCESS) {
        repo->last_sync = time(NULL);
        repository_get_commit_hash(repo->local_path, repo->last_commit, sizeof(repo->last_commit));
        log_info("Successfully synced repository: %s", repo->name);
    } else {
        log_error("Failed to sync repository: %s", repo->name);
    }
    
    return result;
}

repository_t *repository_get_by_name(const char *name) {
    if (!name || load_repositories() != TINYPKG_SUCCESS) {
        return NULL;
    }
    
    for (int i = 0; i < repository_count; i++) {
        if (strcmp(repositories[i].name, name) == 0) {
            return &repositories[i];
        }
    }
    
    return NULL;
}

repository_t *repository_get_all(int *count) {
    if (!count) return NULL;
    
    if (load_repositories() != TINYPKG_SUCCESS) {
        *count = 0;
        return NULL;
    }
    
    *count = repository_count;
    return repositories;
}

int repository_is_available(const char *package_name) {
    if (!package_name) return 0;
    
    char *package_path = repository_get_package_path(package_name);
    if (!package_path) return 0;
    
    int exists = utils_file_exists(package_path);
    TINYPKG_FREE(package_path);
    
    return exists;
}

char *repository_get_package_path(const char *package_name) {
    if (!package_name || load_repositories() != TINYPKG_SUCCESS) {
        return NULL;
    }
    
    // Search through all enabled repositories
    for (int i = 0; i < repository_count; i++) {
        if (!repositories[i].enabled) continue;
        
        char *package_file = utils_join_path(repositories[i].local_path, 
                                           utils_join_path(package_name, 
                                           utils_join_path(package_name, ".json")));
        
        if (package_file && utils_file_exists(package_file)) {
            return package_file;
        }
        
        if (package_file) TINYPKG_FREE(package_file);
        
        // Try alternative path format: repo/package.json
        package_file = utils_join_path(repositories[i].local_path, 
                                     (char*)package_name);
        char *json_file = TINYPKG_MALLOC(strlen(package_file) + 6);
        if (json_file) {
            sprintf(json_file, "%s.json", package_file);
            if (utils_file_exists(json_file)) {
                TINYPKG_FREE(package_file);
                return json_file;
            }
            TINYPKG_FREE(json_file);
        }
        
        if (package_file) TINYPKG_FREE(package_file);
    }
    
    return NULL;
}

int repository_list(void) {
    if (load_repositories() != TINYPKG_SUCCESS) {
        log_error("Failed to load repositories");
        return TINYPKG_ERROR;
    }
    
    printf("Configured repositories:\n");
    printf("%-15s %-8s %-10s %-50s %s\n", "Name", "Enabled", "Priority", "URL", "Last Sync");
    printf("%.100s\n", "----------------------------------------------------------------------------------------------------");
    
    for (int i = 0; i < repository_count; i++) {
        repository_t *repo = &repositories[i];
        
        char sync_time[32] = "Never";
        if (repo->last_sync > 0) {
            struct tm *tm_info = localtime(&repo->last_sync);
            strftime(sync_time, sizeof(sync_time), "%Y-%m-%d %H:%M", tm_info);
        }
        
        printf("%-15s %-8s %-10d %-50s %s\n",
               repo->name,
               repo->enabled ? "Yes" : "No",
               repo->priority,
               repo->url,
               sync_time);
    }
    
    return TINYPKG_SUCCESS;
}

sync_status_t repository_get_sync_status(const char *repo_name) {
    repository_t *repo = repository_get_by_name(repo_name);
    if (!repo) return SYNC_STATUS_ERROR;
    
    if (!repo->enabled) return SYNC_STATUS_ERROR;
    
    // Check if repository directory exists and is valid
    if (!utils_directory_exists(repo->local_path)) {
        return SYNC_STATUS_ERROR;
    }
    
    if (!repository_is_git_repo(repo->local_path)) {
        return SYNC_STATUS_ERROR;
    }
    
    return SYNC_STATUS_SUCCESS;
}

time_t repository_get_last_sync(const char *repo_name) {
    repository_t *repo = repository_get_by_name(repo_name);
    return repo ? repo->last_sync : 0;
}

int repository_needs_sync(const char *repo_name) {
    repository_t *repo = repository_get_by_name(repo_name);
    if (!repo) return 1;
    
    // If never synced, needs sync
    if (repo->last_sync == 0) return 1;
    
    // If synced more than sync_interval seconds ago, needs sync
    time_t now = time(NULL);
    int sync_interval = global_config ? global_config->sync_interval : 3600;
    
    return (now - repo->last_sync) > sync_interval;
}

// Low-level Git operations
int repository_clone(const char *url, const char *branch, const char *dest_path) {
    if (!url || !dest_path) return TINYPKG_ERROR;
    
    char cmd[MAX_CMD];
    const char *branch_arg = branch ? branch : "main";
    
    snprintf(cmd, sizeof(cmd), 
             "git clone --depth=1 --branch=%s '%s' '%s'", 
             branch_arg, url, dest_path);
    
    log_debug("Cloning repository: %s", cmd);
    
    int result = utils_run_command(cmd, NULL);
    if (result != TINYPKG_SUCCESS) {
        log_error("Failed to clone repository from %s", url);
        return TINYPKG_ERROR_NETWORK;
    }
    
    return TINYPKG_SUCCESS;
}

int repository_pull(const char *repo_path) {
    if (!repo_path) return TINYPKG_ERROR;
    
    if (!repository_is_git_repo(repo_path)) {
        log_error("Not a git repository: %s", repo_path);
        return TINYPKG_ERROR;
    }
    
    char cmd[MAX_CMD];
    snprintf(cmd, sizeof(cmd), "git pull --ff-only");
    
    log_debug("Pulling repository updates in: %s", repo_path);
    
    int result = utils_run_command(cmd, repo_path);
    if (result != TINYPKG_SUCCESS) {
        log_warn("Git pull failed, trying reset to origin");
        
        // Try to reset to origin if pull failed
        snprintf(cmd, sizeof(cmd), "git fetch origin && git reset --hard origin/HEAD");
        result = utils_run_command(cmd, repo_path);
        
        if (result != TINYPKG_SUCCESS) {
            log_error("Failed to update repository: %s", repo_path);
            return TINYPKG_ERROR_NETWORK;
        }
    }
    
    return TINYPKG_SUCCESS;
}

int repository_get_commit_hash(const char *repo_path, char *hash, size_t hash_size) {
    if (!repo_path || !hash || hash_size < 41) return TINYPKG_ERROR;
    
    char cmd[MAX_CMD];
    snprintf(cmd, sizeof(cmd), "git rev-parse HEAD");
    
    char *output = NULL;
    int exit_code;
    
    int result = utils_run_command_with_output(cmd, repo_path, &output, &exit_code);
    
    if (result == TINYPKG_SUCCESS && exit_code == 0 && output) {
        // Trim whitespace and copy hash
        char *trimmed = utils_string_trim(output);
        strncpy(hash, trimmed, hash_size - 1);
        hash[hash_size - 1] = '\0';
        
        TINYPKG_FREE(output);
        return TINYPKG_SUCCESS;
    }
    
    if (output) TINYPKG_FREE(output);
    
    hash[0] = '\0';
    return TINYPKG_ERROR;
}

int repository_is_git_repo(const char *path) {
    if (!path) return 0;
    
    char git_dir[MAX_PATH];
    snprintf(git_dir, sizeof(git_dir), "%s/.git", path);
    
    struct stat st;
    return (stat(git_dir, &st) == 0);
}

int repository_add(const char *name, const char *url, const char *branch) {
    if (!name || !url) return TINYPKG_ERROR;
    
    // Check if repository already exists
    if (repository_get_by_name(name)) {
        log_error("Repository already exists: %s", name);
        return TINYPKG_ERROR;
    }
    
    // Expand repository array
    repository_t *new_repos = TINYPKG_REALLOC(repositories, 
                                            sizeof(repository_t) * (repository_count + 1));
    if (!new_repos) return TINYPKG_ERROR_MEMORY;
    
    repositories = new_repos;
    
    // Initialize new repository
    repository_t *new_repo = &repositories[repository_count];
    memset(new_repo, 0, sizeof(repository_t));
    
    strncpy(new_repo->name, name, sizeof(new_repo->name) - 1);
    strncpy(new_repo->url, url, sizeof(new_repo->url) - 1);
    strncpy(new_repo->branch, branch ? branch : "main", sizeof(new_repo->branch) - 1);
    
    snprintf(new_repo->local_path, sizeof(new_repo->local_path), 
             "%s/%s", REPO_DIR, name);
    
    new_repo->priority = 50;
    new_repo->enabled = 1;
    new_repo->last_sync = 0;
    
    repository_count++;
    
    log_info("Added repository: %s (%s)", name, url);
    return TINYPKG_SUCCESS;
}

int repository_remove(const char *name) {
    if (!name) return TINYPKG_ERROR;
    
    int found_index = -1;
    for (int i = 0; i < repository_count; i++) {
        if (strcmp(repositories[i].name, name) == 0) {
            found_index = i;
            break;
        }
    }
    
    if (found_index == -1) {
        log_error("Repository not found: %s", name);
        return TINYPKG_ERROR;
    }
    
    // Remove local repository directory
    if (utils_directory_exists(repositories[found_index].local_path)) {
        utils_remove_directory_recursive(repositories[found_index].local_path);
    }
    
    // Shift array elements
    for (int i = found_index; i < repository_count - 1; i++) {
        repositories[i] = repositories[i + 1];
    }
    
    repository_count--;
    
    if (repository_count > 0) {
        repositories = TINYPKG_REALLOC(repositories, sizeof(repository_t) * repository_count);
    } else {
        TINYPKG_FREE(repositories);
        repositories = NULL;
    }
    
    log_info("Removed repository: %s", name);
    return TINYPKG_SUCCESS;
}
