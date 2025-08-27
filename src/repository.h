/*
 * TinyPkg - Repository Management Header
 * Git repository handling and package database synchronization
 */

#ifndef TINYPKG_REPOSITORY_H
#define TINYPKG_REPOSITORY_H

#include <time.h>

// Repository information structure
typedef struct repository {
    char name[128];
    char url[512];
    char branch[64];
    char local_path[256];
    int priority;
    int enabled;
    time_t last_sync;
    char last_commit[41]; // SHA-1 hash
} repository_t;

// Sync status
typedef enum {
    SYNC_STATUS_SUCCESS = 0,
    SYNC_STATUS_NO_CHANGES = 1,
    SYNC_STATUS_ERROR = -1,
    SYNC_STATUS_NETWORK_ERROR = -2,
    SYNC_STATUS_AUTH_ERROR = -3
} sync_status_t;

// Function declarations

// Repository management
int repository_init(void);
void repository_cleanup(void);
int repository_sync(void);
int repository_sync_specific(const char *repo_name);
int repository_add(const char *name, const char *url, const char *branch);
int repository_remove(const char *name);
int repository_list(void);

// Repository queries
repository_t *repository_get_by_name(const char *name);
repository_t *repository_get_all(int *count);
int repository_is_available(const char *package_name);
char *repository_get_package_path(const char *package_name);

// Repository status
sync_status_t repository_get_sync_status(const char *repo_name);
time_t repository_get_last_sync(const char *repo_name);
int repository_needs_sync(const char *repo_name);

// Low-level Git operations
int repository_clone(const char *url, const char *branch, const char *dest_path);
int repository_pull(const char *repo_path);
int repository_get_commit_hash(const char *repo_path, char *hash, size_t hash_size);
int repository_is_git_repo(const char *path);

#endif /* TINYPKG_REPOSITORY_H */
