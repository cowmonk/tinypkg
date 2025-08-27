/*
 * TinyPkg - Package Management Implementation
 * Core package installation, removal, and management functions
 */

#include "../include/tinypkg.h"

// Global package database
static package_db_entry_t *package_db_head = NULL;
static int package_db_loaded = 0;

// Package installation function
int package_install(const char *package_name) {
    package_t *pkg = NULL;
    int result = TINYPKG_SUCCESS;
    
    if (!package_name) {
        log_error("Package name is NULL");
        return TINYPKG_ERROR;
    }
    
    log_info("Starting installation of package: %s", package_name);
    
    // Check if already installed
    if (package_is_installed(package_name)) {
        if (!global_config->force_mode) {
            log_warn("Package '%s' is already installed", package_name);
            return TINYPKG_SUCCESS;
        }
        log_info("Force mode enabled, reinstalling package");
    }
    
    // Load package information
    pkg = package_load_info(package_name);
    if (!pkg) {
        log_error("Failed to load package information: %s", package_name);
        return TINYPKG_ERROR;
    }
    
    // Validate package
    result = package_validate(pkg);
    if (result != TINYPKG_SUCCESS) {
        log_error("Package validation failed: %s", package_name);
        package_free(pkg);
        return result;
    }
    
    // Check conflicts
    result = package_check_conflicts(pkg);
    if (result != TINYPKG_SUCCESS) {
        log_error("Package conflicts detected: %s", package_name);
        package_free(pkg);
        return result;
    }
    
    // Set package state
    package_set_state(package_name, PKG_STATE_DOWNLOADING);
    
    // Install dependencies first
    if (!global_config->skip_dependencies && pkg->dependencies) {
        log_info("Resolving dependencies for %s", package_name);
        
        char **install_order = NULL;
        int install_count = 0;
        
        result = dependency_resolve(package_name, &install_order, &install_count);
        if (result != TINYPKG_SUCCESS) {
            log_error("Dependency resolution failed for %s", package_name);
            package_set_state(package_name, PKG_STATE_FAILED);
            package_free(pkg);
            return result;
        }
        
        // Install dependencies in order
        for (int i = 0; i < install_count - 1; i++) { // -1 to skip the package itself
            if (!package_is_installed(install_order[i])) {
                log_info("Installing dependency: %s", install_order[i]);
                result = package_install(install_order[i]);
                if (result != TINYPKG_SUCCESS) {
                    log_error("Failed to install dependency: %s", install_order[i]);
                    package_set_state(package_name, PKG_STATE_FAILED);
                    // Free install_order
                    for (int j = 0; j < install_count; j++) {
                        TINYPKG_FREE(install_order[j]);
                    }
                    TINYPKG_FREE(install_order);
                    package_free(pkg);
                    return result;
                }
            }
        }
        
        // Free install_order
        for (int i = 0; i < install_count; i++) {
            TINYPKG_FREE(install_order[i]);
        }
        TINYPKG_FREE(install_order);
    }
    
    // Build and install the package
    package_set_state(package_name, PKG_STATE_BUILDING);
    
    result = build_package(pkg);
    if (result != TINYPKG_SUCCESS) {
        log_error("Package build failed: %s", package_name);
        package_set_state(package_name, PKG_STATE_FAILED);
        package_free(pkg);
        return result;
    }
    
    package_set_state(package_name, PKG_STATE_INSTALLING);
    
    // Install the built package
    result = build_install_package(pkg);
    if (result != TINYPKG_SUCCESS) {
        log_error("Package installation failed: %s", package_name);
        package_set_state(package_name, PKG_STATE_FAILED);
        package_free(pkg);
        return result;
    }
    
    // Update package database
    pkg->install_time = time(NULL);
    package_set_state(package_name, PKG_STATE_INSTALLED);
    
    result = package_db_add(pkg);
    if (result != TINYPKG_SUCCESS) {
        log_warn("Failed to update package database for %s", package_name);
    }
    
    // Run post-install commands
    if (strlen(pkg->post_install_cmd) > 0) {
        log_info("Running post-install commands for %s", package_name);
        result = utils_run_command(pkg->post_install_cmd, NULL);
        if (result != TINYPKG_SUCCESS) {
            log_warn("Post-install commands failed for %s", package_name);
        }
    }
    
    log_info("Package '%s' installed successfully", package_name);
    package_free(pkg);
    return TINYPKG_SUCCESS;
}

int package_remove(const char *package_name) {
    package_db_entry_t *db_entry;
    int result;
    
    if (!package_name) {
        return TINYPKG_ERROR;
    }
    
    log_info("Removing package: %s", package_name);
    
    // Check if installed
    if (!package_is_installed(package_name)) {
        log_warn("Package '%s' is not installed", package_name);
        return TINYPKG_SUCCESS;
    }
    
    // Find package in database
    db_entry = package_db_find(package_name);
    if (!db_entry) {
        log_error("Package '%s' not found in database", package_name);
        return TINYPKG_ERROR;
    }
    
    // Check for packages that depend on this one
    if (!global_config->force_mode) {
        char **dependents = NULL;
        int dependent_count = 0;
        
        result = dependency_find_dependents(package_name, &dependents, &dependent_count);
        if (result == TINYPKG_SUCCESS && dependent_count > 0) {
            log_error("Cannot remove '%s': required by %d other package(s):", 
                     package_name, dependent_count);
            for (int i = 0; i < dependent_count; i++) {
                log_error("  - %s", dependents[i]);
                TINYPKG_FREE(dependents[i]);
            }
            TINYPKG_FREE(dependents);
            return TINYPKG_ERROR_DEPENDENCY;
        }
        
        if (dependents) {
            TINYPKG_FREE(dependents);
        }
    }
    
    // Get file list for removal
    char **file_list = NULL;
    int file_count = 0;
    
    file_list = package_get_file_list(package_name, &file_count);
    if (file_list) {
        log_info("Removing %d files for package %s", file_count, package_name);
        
        // Remove files in reverse order (deepest first)
        for (int i = file_count - 1; i >= 0; i--) {
            if (unlink(file_list[i]) != 0 && errno != ENOENT) {
                log_warn("Failed to remove file: %s (%s)", file_list[i], strerror(errno));
            }
            TINYPKG_FREE(file_list[i]);
        }
        TINYPKG_FREE(file_list);
    }
    
    // Remove from package database
    result = package_db_remove(package_name);
    if (result != TINYPKG_SUCCESS) {
        log_warn("Failed to remove package from database: %s", package_name);
    }
    
    log_info("Package '%s' removed successfully", package_name);
    return TINYPKG_SUCCESS;
}

int package_update(const char *package_name) {
    package_t *current_pkg = NULL;
    package_t *new_pkg = NULL;
    version_t current_version, new_version;
    int result;
    
    if (!package_name) {
        return TINYPKG_ERROR;
    }
    
    log_info("Updating package: %s", package_name);
    
    // Check if installed
    if (!package_is_installed(package_name)) {
        log_info("Package '%s' not installed, installing instead", package_name);
        return package_install(package_name);
    }
    
    // Get current version
    package_db_entry_t *db_entry = package_db_find(package_name);
    if (!db_entry) {
        log_error("Package '%s' not found in database", package_name);
        return TINYPKG_ERROR;
    }
    
    // Load new package information
    new_pkg = package_load_info(package_name);
    if (!new_pkg) {
        log_error("Failed to load package information: %s", package_name);
        return TINYPKG_ERROR;
    }
    
    // Compare versions
    result = version_parse(db_entry->version, &current_version);
    if (result != TINYPKG_SUCCESS) {
        log_error("Failed to parse current version: %s", db_entry->version);
        package_free(new_pkg);
        return result;
    }
    
    result = version_parse(new_pkg->version, &new_version);
    if (result != TINYPKG_SUCCESS) {
        log_error("Failed to parse new version: %s", new_pkg->version);
        package_free(new_pkg);
        return result;
    }
    
    int version_cmp = version_compare(&current_version, &new_version);
    if (version_cmp >= 0 && !global_config->force_mode) {
        log_info("Package '%s' is already up to date (version %s)", 
                package_name, db_entry->version);
        package_free(new_pkg);
        return TINYPKG_SUCCESS;
    }
    
    log_info("Updating package '%s' from version %s to %s", 
             package_name, db_entry->version, new_pkg->version);
    
    // Backup config files
    package_backup_config_files(new_pkg);
    
    // Remove old version
    result = package_remove(package_name);
    if (result != TINYPKG_SUCCESS) {
        log_error("Failed to remove old version of %s", package_name);
        package_free(new_pkg);
        return result;
    }
    
    // Install new version
    result = package_install(package_name);
    if (result != TINYPKG_SUCCESS) {
        log_error("Failed to install new version of %s", package_name);
        // Try to restore old version if possible
        log_info("Attempting to restore previous version...");
        package_free(new_pkg);
        return result;
    }
    
    // Restore config files
    package_restore_config_files(new_pkg);
    
    package_free(new_pkg);
    log_info("Package '%s' updated successfully", package_name);
    return TINYPKG_SUCCESS;
}

int package_update_all(void) {
    package_db_entry_t *entry;
    int updated_count = 0;
    int failed_count = 0;
    
    log_info("Updating all installed packages");
    
    // Ensure database is loaded
    if (package_db_load() != TINYPKG_SUCCESS) {
        log_error("Failed to load package database");
        return TINYPKG_ERROR;
    }
    
    entry = package_db_head;
    while (entry) {
        int result = package_update(entry->name);
        if (result == TINYPKG_SUCCESS) {
            updated_count++;
        } else {
            failed_count++;
            log_warn("Failed to update package: %s", entry->name);
        }
        entry = entry->next;
    }
    
    log_info("Package update completed: %d updated, %d failed", updated_count, failed_count);
    
    if (failed_count > 0) {
        return TINYPKG_ERROR;
    }
    
    return TINYPKG_SUCCESS;
}

int package_query(const char *package_name) {
    package_t *pkg;
    package_db_entry_t *db_entry;
    
    if (!package_name) {
        return TINYPKG_ERROR;
    }
    
    // Load package information
    pkg = package_load_info(package_name);
    if (!pkg) {
        printf("Package '%s' not found in repository\n", package_name);
        return TINYPKG_ERROR;
    }
    
    // Check if installed
    db_entry = package_db_find(package_name);
    
    printf("Package: %s\n", pkg->name);
    printf("Version: %s\n", pkg->version);
    printf("Description: %s\n", pkg->description);
    printf("Maintainer: %s\n", pkg->maintainer);
    printf("Homepage: %s\n", pkg->homepage);
    printf("License: %s\n", pkg->license);
    printf("Category: %s\n", pkg->category);
    printf("Source URL: %s\n", pkg->source_url);
    
    if (pkg->size_estimate > 0) {
        printf("Estimated Size: ");
        utils_format_size(pkg->size_estimate);
        printf("\n");
    }
    
    if (pkg->build_time_estimate > 0) {
        printf("Build Time: %d seconds\n", pkg->build_time_estimate);
    }
    
    printf("Status: ");
    if (db_entry) {
        printf("Installed (version %s, installed on ", db_entry->version);
        utils_format_time(db_entry->install_time);
        printf(")\n");
        
        if (db_entry->installed_size > 0) {
            printf("Installed Size: ");
            utils_format_size(db_entry->installed_size);
            printf("\n");
        }
    } else {
        printf("Not installed\n");
    }
    
    if (pkg->dep_count > 0) {
        printf("Dependencies (%d): ", pkg->dep_count);
        for (int i = 0; i < pkg->dep_count; i++) {
            printf("%s", pkg->dependencies[i]);
            if (i < pkg->dep_count - 1) printf(", ");
        }
        printf("\n");
    }
    
    if (pkg->conflict_count > 0) {
        printf("Conflicts (%d): ", pkg->conflict_count);
        for (int i = 0; i < pkg->conflict_count; i++) {
            printf("%s", pkg->conflicts[i]);
            if (i < pkg->conflict_count - 1) printf(", ");
        }
        printf("\n");
    }
    
    if (pkg->provides_count > 0) {
        printf("Provides (%d): ", pkg->provides_count);
        for (int i = 0; i < pkg->provides_count; i++) {
            printf("%s", pkg->provides[i]);
            if (i < pkg->provides_count - 1) printf(", ");
        }
        printf("\n");
    }
    
    package_free(pkg);
    return TINYPKG_SUCCESS;
}

int package_list(const char *pattern) {
    package_db_entry_t *entry;
    int count = 0;
    
    // Ensure database is loaded
    if (package_db_load() != TINYPKG_SUCCESS) {
        log_error("Failed to load package database");
        return TINYPKG_ERROR;
    }
    
    printf("Installed packages:\n");
    printf("%-20s %-12s %-50s %s\n", "Name", "Version", "Description", "Installed");
    printf("%.80s\n", "--------------------------------------------------------------------------------");
    
    entry = package_db_head;
    while (entry) {
        if (!pattern || strstr(entry->name, pattern) || strstr(entry->description, pattern)) {
            printf("%-20s %-12s %-50.50s ", entry->name, entry->version, entry->description);
            utils_format_time(entry->install_time);
            printf("\n");
            count++;
        }
        entry = entry->next;
    }
    
    printf("\nTotal: %d packages\n", count);
    return TINYPKG_SUCCESS;
}

int package_search(const char *pattern) {
    // This would search through the repository
    // For now, simplified implementation
    printf("Searching for packages matching: %s\n", pattern);
    
    // Search through repository files
    char search_cmd[MAX_CMD];
    snprintf(search_cmd, sizeof(search_cmd), 
             "find %s -name '*.json' -exec grep -l '%s' {} \\;", 
             REPO_DIR, pattern);
    
    return utils_run_command(search_cmd, NULL);
}

package_t *package_load_info(const char *package_name) {
    if (!package_name) {
        return NULL;
    }
    
    return json_parser_load_package(package_name);
}

int package_is_installed(const char *package_name) {
    if (package_db_load() != TINYPKG_SUCCESS) {
        return 0;
    }
    
    return (package_db_find(package_name) != NULL);
}

// Package database operations
int package_db_add(const package_t *pkg) {
    package_db_entry_t *entry;
    
    if (!pkg) {
        return TINYPKG_ERROR;
    }
    
    // Remove existing entry if present
    package_db_remove(pkg->name);
    
    // Create new entry
    entry = TINYPKG_CALLOC(1, sizeof(package_db_entry_t));
    if (!entry) {
        return TINYPKG_ERROR_MEMORY;
    }
    
    strncpy(entry->name, pkg->name, sizeof(entry->name) - 1);
    strncpy(entry->version, pkg->version, sizeof(entry->version) - 1);
    strncpy(entry->description, pkg->description, sizeof(entry->description) - 1);
    entry->install_time = pkg->install_time;
    entry->installed_size = pkg->size_estimate; // This should be calculated
    entry->state = PKG_STATE_INSTALLED;
    
    // Add to linked list
    entry->next = package_db_head;
    package_db_head = entry;
    
    // Save database
    return package_db_save();
}

int package_db_remove(const char *package_name) {
    package_db_entry_t *entry, *prev = NULL;
    
    if (!package_name) {
        return TINYPKG_ERROR;
    }
    
    entry = package_db_head;
    while (entry) {
        if (TINYPKG_STREQ(entry->name, package_name)) {
            if (prev) {
                prev->next = entry->next;
            } else {
                package_db_head = entry->next;
            }
            
            package_db_entry_free(entry);
            return package_db_save();
        }
        prev = entry;
        entry = entry->next;
    }
    
    return TINYPKG_SUCCESS; // Not found, but not an error
}

package_db_entry_t *package_db_find(const char *package_name) {
    package_db_entry_t *entry;
    
    if (!package_name) {
        return NULL;
    }
    
    entry = package_db_head;
    while (entry) {
        if (TINYPKG_STREQ(entry->name, package_name)) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

int package_db_save(void) {
    char db_file[MAX_PATH];
    FILE *fp;
    package_db_entry_t *entry;
    
    snprintf(db_file, sizeof(db_file), "%s/installed.txt", LIB_DIR);
    
    fp = fopen(db_file, "w");
    if (!fp) {
        log_error("Failed to open database file for writing: %s", db_file);
        return TINYPKG_ERROR_FILE;
    }
    
    fprintf(fp, "# TinyPkg Installed Packages Database\n");
    fprintf(fp, "# Format: name\tversion\tdescription\tinstall_time\tinstalled_size\tstate\n");
    
    entry = package_db_head;
    while (entry) {
        fprintf(fp, "%s\t%s\t%s\t%ld\t%zu\t%d\n",
                entry->name, entry->version, entry->description,
                entry->install_time, entry->installed_size, entry->state);
        entry = entry->next;
    }
    
    fclose(fp);
    return TINYPKG_SUCCESS;
}

int package_db_load(void) {
    char db_file[MAX_PATH];
    FILE *fp;
    char line[1024];
    package_db_entry_t *entry;
    
    if (package_db_loaded) {
        return TINYPKG_SUCCESS;
    }
    
    snprintf(db_file, sizeof(db_file), "%s/installed.txt", LIB_DIR);
    
    fp = fopen(db_file, "r");
    if (!fp) {
        // Database doesn't exist yet, not an error
        package_db_loaded = 1;
        return TINYPKG_SUCCESS;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        
        entry = TINYPKG_CALLOC(1, sizeof(package_db_entry_t));
        if (!entry) {
            fclose(fp);
            return TINYPKG_ERROR_MEMORY;
        }
        
        int fields = sscanf(line, "%255s\t%63s\t%511[^\t]\t%ld\t%zu\t%d",
                           entry->name, entry->version, entry->description,
                           &entry->install_time, &entry->installed_size, 
                           (int*)&entry->state);
        
        if (fields >= 3) { // Minimum required fields
            entry->next = package_db_head;
            package_db_head = entry;
        } else {
            package_db_entry_free(entry);
        }
    }
    
    fclose(fp);
    package_db_loaded = 1;
    return TINYPKG_SUCCESS;
}

// Memory management functions
package_t *package_create(void) {
    package_t *pkg = TINYPKG_CALLOC(1, sizeof(package_t));
    if (pkg) {
        pkg->ref_count = 1;
        pkg->state = PKG_STATE_UNKNOWN;
        pkg->build_system = BUILD_TYPE_AUTOTOOLS;
    }
    return pkg;
}

void package_free(package_t *pkg) {
    if (!pkg) return;
    
    pkg->ref_count--;
    if (pkg->ref_count > 0) return;
    
    // Free string arrays
    if (pkg->dependencies) {
        for (int i = 0; i < pkg->dep_count; i++) {
            TINYPKG_FREE(pkg->dependencies[i]);
        }
        TINYPKG_FREE(pkg->dependencies);
    }
    
    if (pkg->build_dependencies) {
        for (int i = 0; i < pkg->build_dep_count; i++) {
            TINYPKG_FREE(pkg->build_dependencies[i]);
        }
        TINYPKG_FREE(pkg->build_dependencies);
    }
    
    if (pkg->conflicts) {
        for (int i = 0; i < pkg->conflict_count; i++) {
            TINYPKG_FREE(pkg->conflicts[i]);
        }
        TINYPKG_FREE(pkg->conflicts);
    }
    
    if (pkg->provides) {
        for (int i = 0; i < pkg->provides_count; i++) {
            TINYPKG_FREE(pkg->provides[i]);
        }
        TINYPKG_FREE(pkg->provides);
    }
    
    TINYPKG_FREE(pkg);
}

void package_db_entry_free(package_db_entry_t *entry) {
    TINYPKG_FREE(entry);
}

// Validation functions
int package_validate(const package_t *pkg) {
    if (!pkg) {
        return TINYPKG_ERROR;
    }
    
    if (strlen(pkg->name) == 0) {
        log_error("Package name is empty");
        return TINYPKG_ERROR;
    }
    
    if (strlen(pkg->version) == 0) {
        log_error("Package version is empty");
        return TINYPKG_ERROR;
    }
    
    if (strlen(pkg->source_url) == 0) {
        log_error("Package source URL is empty");
        return TINYPKG_ERROR;
    }
    
    return TINYPKG_SUCCESS;
}

int package_check_conflicts(const package_t *pkg) {
    if (!pkg || !pkg->conflicts) {
        return TINYPKG_SUCCESS;
    }
    
    for (int i = 0; i < pkg->conflict_count; i++) {
        if (package_is_installed(pkg->conflicts[i])) {
            log_error("Package '%s' conflicts with installed package '%s'", 
                     pkg->name, pkg->conflicts[i]);
            return TINYPKG_ERROR_DEPENDENCY;
        }
    }
    
    return TINYPKG_SUCCESS;
}

// State management
const char *package_state_to_string(package_state_t state) {
    switch (state) {
        case PKG_STATE_UNKNOWN: return "unknown";
        case PKG_STATE_AVAILABLE: return "available";
        case PKG_STATE_DOWNLOADING: return "downloading";
        case PKG_STATE_BUILDING: return "building";
        case PKG_STATE_INSTALLING: return "installing";
        case PKG_STATE_INSTALLED: return "installed";
        case PKG_STATE_FAILED: return "failed";
        case PKG_STATE_BROKEN: return "broken";
        default: return "invalid";
    }
}

package_state_t package_state_from_string(const char *state_str) {
    if (!state_str) return PKG_STATE_UNKNOWN;
    
    if (TINYPKG_STREQ(state_str, "available")) return PKG_STATE_AVAILABLE;
    if (TINYPKG_STREQ(state_str, "downloading")) return PKG_STATE_DOWNLOADING;
    if (TINYPKG_STREQ(state_str, "building")) return PKG_STATE_BUILDING;
    if (TINYPKG_STREQ(state_str, "installing")) return PKG_STATE_INSTALLING;
    if (TINYPKG_STREQ(state_str, "installed")) return PKG_STATE_INSTALLED;
    if (TINYPKG_STREQ(state_str, "failed")) return PKG_STATE_FAILED;
    if (TINYPKG_STREQ(state_str, "broken")) return PKG_STATE_BROKEN;
    
    return PKG_STATE_UNKNOWN;
}

int package_set_state(const char *package_name, package_state_t state) {
    package_db_entry_t *entry = package_db_find(package_name);
    if (entry) {
        entry->state = state;
        return package_db_save();
    }
    
    // Package not in database yet, just log the state change
    log_debug("State change for %s: %s", package_name, package_state_to_string(state));
    return TINYPKG_SUCCESS;
}

package_state_t package_get_state(const char *package_name) {
    package_db_entry_t *entry = package_db_find(package_name);
    if (entry) {
        return entry->state;
    }
    return PKG_STATE_UNKNOWN;
}
