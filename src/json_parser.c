/*
 * TinyPkg - JSON Parser Implementation
 * Package definition parsing from JSON files
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include "../include/tinypkg.h"

// Load package from repository
package_t *json_parser_load_package(const char *package_name) {
    if (!package_name) {
        log_error("Package name is NULL");
        return NULL;
    }
    
    char *package_path = repository_get_package_path(package_name);
    if (!package_path) {
        log_error("Package not found in repository: %s", package_name);
        return NULL;
    }
    
    package_t *pkg = json_parser_load_package_file(package_path);
    TINYPKG_FREE(package_path);
    
    return pkg;
}

// Load package from specific JSON file
package_t *json_parser_load_package_file(const char *json_file) {
    if (!json_file) {
        log_error("JSON file path is NULL");
        return NULL;
    }
    
    if (!utils_file_exists(json_file)) {
        log_error("JSON file not found: %s", json_file);
        return NULL;
    }
    
    log_debug("Loading package from: %s", json_file);
    
    json_error_t error;
    json_t *root = json_load_file(json_file, 0, &error);
    
    if (!root) {
        log_error("JSON parse error in %s:%d: %s", json_file, error.line, error.text);
        return NULL;
    }
    
    // Validate JSON structure
    if (json_parser_validate_package_json(root) != TINYPKG_SUCCESS) {
        json_decref(root);
        return NULL;
    }
    
    package_t *pkg = package_create();
    if (!pkg) {
        json_decref(root);
        return NULL;
    }
    
    // Parse basic fields
    const char *name = json_parser_get_string(root, "name", "");
    const char *version = json_parser_get_string(root, "version", "");
    const char *description = json_parser_get_string(root, "description", "");
    
    if (strlen(name) == 0 || strlen(version) == 0) {
        log_error("Package missing required fields: name or version");
        package_free(pkg);
        json_decref(root);
        return NULL;
    }
    
    strncpy(pkg->name, name, sizeof(pkg->name) - 1);
    strncpy(pkg->version, version, sizeof(pkg->version) - 1);
    strncpy(pkg->description, description, sizeof(pkg->description) - 1);
    
    // Parse optional fields
    strncpy(pkg->maintainer, 
            json_parser_get_string(root, "maintainer", ""), 
            sizeof(pkg->maintainer) - 1);
    
    strncpy(pkg->homepage, 
            json_parser_get_string(root, "homepage", ""), 
            sizeof(pkg->homepage) - 1);
    
    strncpy(pkg->license, 
            json_parser_get_string(root, "license", ""), 
            sizeof(pkg->license) - 1);
    
    strncpy(pkg->category, 
            json_parser_get_string(root, "category", ""), 
            sizeof(pkg->category) - 1);
    
    // Parse source information
    strncpy(pkg->source_url, 
            json_parser_get_string(root, "source_url", ""), 
            sizeof(pkg->source_url) - 1);
    
    strncpy(pkg->source_type, 
            json_parser_get_string(root, "source_type", "tarball"), 
            sizeof(pkg->source_type) - 1);
    
    strncpy(pkg->checksum, 
            json_parser_get_string(root, "checksum", ""), 
            sizeof(pkg->checksum) - 1);
    
    // Parse build information
    const char *build_system = json_parser_get_string(root, "build_system", "autotools");
    if (strcmp(build_system, "cmake") == 0) {
        pkg->build_system = BUILD_TYPE_CMAKE;
    } else if (strcmp(build_system, "make") == 0) {
        pkg->build_system = BUILD_TYPE_MAKE;
    } else if (strcmp(build_system, "custom") == 0) {
        pkg->build_system = BUILD_TYPE_CUSTOM;
    } else {
        pkg->build_system = BUILD_TYPE_AUTOTOOLS;
    }
    
    strncpy(pkg->build_cmd, 
            json_parser_get_string(root, "build_cmd", ""), 
            sizeof(pkg->build_cmd) - 1);
    
    strncpy(pkg->install_cmd, 
            json_parser_get_string(root, "install_cmd", ""), 
            sizeof(pkg->install_cmd) - 1);
    
    strncpy(pkg->configure_args, 
            json_parser_get_string(root, "configure_args", ""), 
            sizeof(pkg->configure_args) - 1);
    
    // Parse numeric fields
    pkg->size_estimate = (size_t)json_parser_get_int(root, "size_estimate", 0);
    pkg->build_time_estimate = json_parser_get_int(root, "build_time_estimate", 0);
    
    // Parse arrays
    json_t *dependencies = json_parser_get_array(root, "dependencies");
    if (dependencies) {
        pkg->dependencies = json_parser_array_to_strings(dependencies, &pkg->dep_count);
    }
    
    json_t *build_dependencies = json_parser_get_array(root, "build_dependencies");
    if (build_dependencies) {
        pkg->build_dependencies = json_parser_array_to_strings(build_dependencies, &pkg->build_dep_count);
    }
    
    json_t *conflicts = json_parser_get_array(root, "conflicts");
    if (conflicts) {
        pkg->conflicts = json_parser_array_to_strings(conflicts, &pkg->conflict_count);
    }
    
    json_t *provides = json_parser_get_array(root, "provides");
    if (provides) {
        pkg->provides = json_parser_array_to_strings(provides, &pkg->provides_count);
    }
    
    // Store JSON file path
    strncpy(pkg->json_file, json_file, sizeof(pkg->json_file) - 1);
    
    json_decref(root);
    
    log_debug("Successfully loaded package: %s version %s", pkg->name, pkg->version);
    return pkg;
}

// Validate package JSON structure
int json_parser_validate_package_json(json_t *root) {
    if (!json_is_object(root)) {
        log_error("JSON root is not an object");
        return TINYPKG_ERROR;
    }
    
    // Check required fields
    const char *required_fields[] = {"name", "version", "source_url"};
    for (size_t i = 0; i < sizeof(required_fields) / sizeof(required_fields[0]); i++) {
        json_t *field = json_object_get(root, required_fields[i]);
        if (!field || !json_is_string(field)) {
            log_error("Missing or invalid required field: %s", required_fields[i]);
            return TINYPKG_ERROR;
        }
        
        const char *value = json_string_value(field);
        if (!value || strlen(value) == 0) {
            log_error("Empty required field: %s", required_fields[i]);
            return TINYPKG_ERROR;
        }
    }
    
    // Validate array fields if present
    const char *array_fields[] = {"dependencies", "build_dependencies", "conflicts", "provides"};
    for (size_t i = 0; i < sizeof(array_fields) / sizeof(array_fields[0]); i++) {
        json_t *field = json_object_get(root, array_fields[i]);
        if (field && !json_is_array(field)) {
            log_error("Field should be an array: %s", array_fields[i]);
            return TINYPKG_ERROR;
        }
    }
    
    return TINYPKG_SUCCESS;
}

// Validate package file
int json_parser_validate_package_file(const char *json_file) {
    if (!json_file) return TINYPKG_ERROR;
    
    json_error_t error;
    json_t *root = json_load_file(json_file, 0, &error);
    
    if (!root) {
        log_error("JSON parse error in %s:%d: %s", json_file, error.line, error.text);
        return TINYPKG_ERROR;
    }
    
    int result = json_parser_validate_package_json(root);
    json_decref(root);
    
    return result;
}

// JSON utility functions
json_t *json_parser_load_file(const char *filename) {
    if (!filename) return NULL;
    
    json_error_t error;
    json_t *root = json_load_file(filename, 0, &error);
    
    if (!root) {
        log_error("Failed to load JSON file %s: %s", filename, error.text);
    }
    
    return root;
}

int json_parser_save_file(json_t *root, const char *filename) {
    if (!root || !filename) return TINYPKG_ERROR;
    
    if (json_dump_file(root, filename, JSON_INDENT(2)) != 0) {
        log_error("Failed to save JSON file: %s", filename);
        return TINYPKG_ERROR;
    }
    
    return TINYPKG_SUCCESS;
}

char *json_parser_get_string(json_t *obj, const char *key, const char *default_value) {
    if (!obj || !key) return (char*)default_value;
    
    json_t *value = json_object_get(obj, key);
    if (!value || !json_is_string(value)) {
        return (char*)default_value;
    }
    
    const char *str = json_string_value(value);
    return str ? (char*)str : (char*)default_value;
}

int json_parser_get_int(json_t *obj, const char *key, int default_value) {
    if (!obj || !key) return default_value;
    
    json_t *value = json_object_get(obj, key);
    if (!value || !json_is_integer(value)) {
        return default_value;
    }
    
    return (int)json_integer_value(value);
}

json_t *json_parser_get_array(json_t *obj, const char *key) {
    if (!obj || !key) return NULL;
    
    json_t *value = json_object_get(obj, key);
    if (!value || !json_is_array(value)) {
        return NULL;
    }
    
    return value;
}

// String array helpers
char **json_parser_array_to_strings(json_t *array, int *count) {
    if (!array || !json_is_array(array) || !count) {
        if (count) *count = 0;
        return NULL;
    }
    
    size_t array_size = json_array_size(array);
    if (array_size == 0) {
        *count = 0;
        return NULL;
    }
    
    char **strings = TINYPKG_MALLOC(sizeof(char*) * array_size);
    if (!strings) {
        *count = 0;
        return NULL;
    }
    
    int valid_count = 0;
    for (size_t i = 0; i < array_size; i++) {
        json_t *item = json_array_get(array, i);
        if (json_is_string(item)) {
            const char *str = json_string_value(item);
            if (str && strlen(str) > 0) {
                strings[valid_count] = TINYPKG_STRDUP(str);
                if (strings[valid_count]) {
                    valid_count++;
                } else {
                    log_warn("Failed to duplicate string from JSON array");
                }
            }
        } else {
            log_warn("Non-string item in JSON array at index %zu", i);
        }
    }
    
    if (valid_count == 0) {
        TINYPKG_FREE(strings);
        *count = 0;
        return NULL;
    }
    
    // Resize array to actual count
    if (valid_count < (int)array_size) {
        char **resized = TINYPKG_REALLOC(strings, sizeof(char*) * valid_count);
        if (resized) {
            strings = resized;
        }
    }
    
    *count = valid_count;
    return strings;
}

json_t *json_parser_strings_to_array(char **strings, int count) {
    if (!strings || count <= 0) return json_array();
    
    json_t *array = json_array();
    if (!array) return NULL;
    
    for (int i = 0; i < count; i++) {
        if (strings[i] && strlen(strings[i]) > 0) {
            json_t *str = json_string(strings[i]);
            if (str) {
                json_array_append_new(array, str);
            }
        }
    }
    
    return array;
}

void json_parser_free_string_array(char **strings, int count) {
    utils_string_array_free(strings, count);
}

// Save package to JSON file
int json_parser_save_package(const package_t *pkg, const char *json_file) {
    if (!pkg || !json_file) return TINYPKG_ERROR;
    
    json_t *root = json_object();
    if (!root) return TINYPKG_ERROR_MEMORY;
    
    // Basic fields
    json_object_set_new(root, "name", json_string(pkg->name));
    json_object_set_new(root, "version", json_string(pkg->version));
    json_object_set_new(root, "description", json_string(pkg->description));
    
    if (strlen(pkg->maintainer) > 0) {
        json_object_set_new(root, "maintainer", json_string(pkg->maintainer));
    }
    
    if (strlen(pkg->homepage) > 0) {
        json_object_set_new(root, "homepage", json_string(pkg->homepage));
    }
    
    if (strlen(pkg->license) > 0) {
        json_object_set_new(root, "license", json_string(pkg->license));
    }
    
    if (strlen(pkg->category) > 0) {
        json_object_set_new(root, "category", json_string(pkg->category));
    }
    
    // Source information
    json_object_set_new(root, "source_url", json_string(pkg->source_url));
    json_object_set_new(root, "source_type", json_string(pkg->source_type));
    
    if (strlen(pkg->checksum) > 0) {
        json_object_set_new(root, "checksum", json_string(pkg->checksum));
    }
    
    // Build system
    const char *build_system_str;
    switch (pkg->build_system) {
        case BUILD_TYPE_CMAKE: build_system_str = "cmake"; break;
        case BUILD_TYPE_MAKE: build_system_str = "make"; break;
        case BUILD_TYPE_CUSTOM: build_system_str = "custom"; break;
        default: build_system_str = "autotools"; break;
    }
    json_object_set_new(root, "build_system", json_string(build_system_str));
    
    if (strlen(pkg->build_cmd) > 0) {
        json_object_set_new(root, "build_cmd", json_string(pkg->build_cmd));
    }
    
    if (strlen(pkg->install_cmd) > 0) {
        json_object_set_new(root, "install_cmd", json_string(pkg->install_cmd));
    }
    
    if (strlen(pkg->configure_args) > 0) {
        json_object_set_new(root, "configure_args", json_string(pkg->configure_args));
    }
    
    // Numeric fields
    if (pkg->size_estimate > 0) {
        json_object_set_new(root, "size_estimate", json_integer(pkg->size_estimate));
    }
    
    if (pkg->build_time_estimate > 0) {
        json_object_set_new(root, "build_time_estimate", json_integer(pkg->build_time_estimate));
    }
    
    // Arrays
    if (pkg->dependencies && pkg->dep_count > 0) {
        json_t *deps = json_parser_strings_to_array(pkg->dependencies, pkg->dep_count);
        json_object_set_new(root, "dependencies", deps);
    }
    
    if (pkg->build_dependencies && pkg->build_dep_count > 0) {
        json_t *build_deps = json_parser_strings_to_array(pkg->build_dependencies, pkg->build_dep_count);
        json_object_set_new(root, "build_dependencies", build_deps);
    }
    
    if (pkg->conflicts && pkg->conflict_count > 0) {
        json_t *conflicts = json_parser_strings_to_array(pkg->conflicts, pkg->conflict_count);
        json_object_set_new(root, "conflicts", conflicts);
    }
    
    if (pkg->provides && pkg->provides_count > 0) {
        json_t *provides = json_parser_strings_to_array(pkg->provides, pkg->provides_count);
        json_object_set_new(root, "provides", provides);
    }
    
    int result = json_parser_save_file(root, json_file);
    json_decref(root);
    
    return result;
}
