/*
 * TinyPkg - Dependency Resolution Implementation
 * Package dependency graph and resolution algorithms
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/tinypkg.h"

// Main dependency resolution function using topological sort
int dependency_resolve(const char *package_name, char ***install_order, int *count) {
    if (!package_name || !install_order || !count) {
        return TINYPKG_ERROR;
    }
    
    *install_order = NULL;
    *count = 0;
    
    log_debug("Resolving dependencies for: %s", package_name);
    
    // Create dependency graph
    dependency_graph_t *graph = dependency_graph_create();
    if (!graph) {
        return TINYPKG_ERROR_MEMORY;
    }
    
    // Add the target package to the graph
    int result = dependency_graph_add_package(graph, package_name);
    if (result != TINYPKG_SUCCESS) {
        dependency_graph_free(graph);
        return result;
    }
    
    // Build the complete dependency graph
    result = dependency_graph_build(graph);
    if (result != TINYPKG_SUCCESS) {
        dependency_graph_free(graph);
        return result;
    }
    
    // Check for circular dependencies
    result = dependency_detect_cycles(graph);
    if (result != TINYPKG_SUCCESS) {
        log_error("Circular dependencies detected for package: %s", package_name);
        dependency_graph_free(graph);
        return TINYPKG_ERROR_DEPENDENCY;
    }
    
    // Perform topological sort to get installation order
    result = dependency_graph_topological_sort(graph, install_order, count);
    
    dependency_graph_free(graph);
    
    if (result == TINYPKG_SUCCESS) {
        log_info("Dependency resolution completed: %d packages to install", *count);
        for (int i = 0; i < *count; i++) {
            log_debug("Install order[%d]: %s", i, (*install_order)[i]);
        }
    }
    
    return result;
}

// Dependency graph management
dependency_graph_t *dependency_graph_create(void) {
    dependency_graph_t *graph = TINYPKG_CALLOC(1, sizeof(dependency_graph_t));
    return graph;
}

void dependency_graph_free(dependency_graph_t *graph) {
    if (!graph) return;
    
    dependency_node_t *node = graph->nodes;
    while (node) {
        dependency_node_t *next = node->next;
        dependency_node_free(node);
        node = next;
    }
    
    TINYPKG_FREE(graph);
}

int dependency_graph_add_package(dependency_graph_t *graph, const char *package_name) {
    if (!graph || !package_name) {
        return TINYPKG_ERROR;
    }
    
    // Check if package already exists in graph
    if (dependency_graph_find_node(graph, package_name)) {
        return TINYPKG_SUCCESS; // Already added
    }
    
    // Create new node
    dependency_node_t *node = dependency_node_create(package_name);
    if (!node) {
        return TINYPKG_ERROR_MEMORY;
    }
    
    // Add to linked list
    node->next = graph->nodes;
    graph->nodes = node;
    graph->node_count++;
    
    log_debug("Added package to dependency graph: %s", package_name);
    return TINYPKG_SUCCESS;
}

int dependency_graph_build(dependency_graph_t *graph) {
    if (!graph) return TINYPKG_ERROR;
    
    // For each node, load its dependencies and add them recursively
    dependency_node_t *node = graph->nodes;
    while (node) {
        if (!node->dependencies) {
            // Load package information to get dependencies
            package_t *pkg = package_load_info(node->package_name);
            if (pkg && pkg->dependencies && pkg->dep_count > 0) {
                // Copy dependencies
                node->dependencies = TINYPKG_MALLOC(sizeof(char*) * pkg->dep_count);
                if (node->dependencies) {
                    node->dep_count = pkg->dep_count;
                    for (int i = 0; i < pkg->dep_count; i++) {
                        node->dependencies[i] = TINYPKG_STRDUP(pkg->dependencies[i]);
                        
                        // Recursively add dependency to graph
                        dependency_graph_add_package(graph, pkg->dependencies[i]);
                    }
                }
                package_free(pkg);
            }
        }
        node = node->next;
    }
    
    return TINYPKG_SUCCESS;
}

int dependency_graph_topological_sort(dependency_graph_t *graph, 
                                     char ***sorted_packages, int *count) {
    if (!graph || !sorted_packages || !count) {
        return TINYPKG_ERROR;
    }
    
    *sorted_packages = NULL;
    *count = 0;
    
    if (graph->node_count == 0) {
        return TINYPKG_SUCCESS;
    }
    
    // Allocate result array
    char **result = TINYPKG_MALLOC(sizeof(char*) * graph->node_count);
    if (!result) {
        return TINYPKG_ERROR_MEMORY;
    }
    
    // Calculate in-degrees for all nodes
    int *in_degree = TINYPKG_CALLOC(graph->node_count, sizeof(int));
    if (!in_degree) {
        TINYPKG_FREE(result);
        return TINYPKG_ERROR_MEMORY;
    }
    
    // Create node index mapping
    dependency_node_t **node_array = TINYPKG_MALLOC(sizeof(dependency_node_t*) * graph->node_count);
    if (!node_array) {
        TINYPKG_FREE(result);
        TINYPKG_FREE(in_degree);
        return TINYPKG_ERROR_MEMORY;
    }
    
    // Fill node array and calculate in-degrees
    dependency_node_t *node = graph->nodes;
    int idx = 0;
    while (node) {
        node_array[idx] = node;
        
        // Count in-degrees
        for (int i = 0; i < node->dep_count; i++) {
            for (int j = 0; j < graph->node_count; j++) {
                dependency_node_t *dep_node = graph->nodes;
                int dep_idx = 0;
                while (dep_node && dep_idx < j) {
                    dep_node = dep_node->next;
                    dep_idx++;
                }
                if (dep_node && strcmp(dep_node->package_name, node->dependencies[i]) == 0) {
                    in_degree[j]++;
                    break;
                }
            }
        }
        
        node = node->next;
        idx++;
    }
    
    // Topological sort using Kahn's algorithm
    int result_count = 0;
    int queue[graph->node_count];
    int queue_start = 0, queue_end = 0;
    
    // Find nodes with no incoming edges
    for (int i = 0; i < graph->node_count; i++) {
        if (in_degree[i] == 0) {
            queue[queue_end++] = i;
        }
    }
    
    while (queue_start < queue_end) {
        int current_idx = queue[queue_start++];
        dependency_node_t *current = node_array[current_idx];
        
        // Add to result
        result[result_count] = TINYPKG_STRDUP(current->package_name);
        if (!result[result_count]) {
            // Cleanup on memory error
            for (int i = 0; i < result_count; i++) {
                TINYPKG_FREE(result[i]);
            }
            TINYPKG_FREE(result);
            TINYPKG_FREE(in_degree);
            TINYPKG_FREE(node_array);
            return TINYPKG_ERROR_MEMORY;
        }
        result_count++;
        
        // Decrease in-degree of dependent nodes
        for (int i = 0; i < current->dep_count; i++) {
            for (int j = 0; j < graph->node_count; j++) {
                if (strcmp(node_array[j]->package_name, current->dependencies[i]) == 0) {
                    in_degree[j]--;
                    if (in_degree[j] == 0) {
                        queue[queue_end++] = j;
                    }
                    break;
                }
            }
        }
    }
    
    TINYPKG_FREE(in_degree);
    TINYPKG_FREE(node_array);
    
    // Check for cycles (if not all nodes processed)
    if (result_count != graph->node_count) {
        for (int i = 0; i < result_count; i++) {
            TINYPKG_FREE(result[i]);
        }
        TINYPKG_FREE(result);
        return TINYPKG_ERROR_DEPENDENCY;
    }
    
    *sorted_packages = result;
    *count = result_count;
    return TINYPKG_SUCCESS;
}

// Cycle detection using DFS
static int dfs_cycle_check(dependency_node_t *node, dependency_graph_t *graph) {
    node->visited = 1;
    node->in_stack = 1;
    
    // Visit all dependencies
    for (int i = 0; i < node->dep_count; i++) {
        dependency_node_t *dep_node = dependency_graph_find_node(graph, node->dependencies[i]);
        if (dep_node) {
            if (dep_node->in_stack) {
                return 1; // Back edge found - cycle detected
            }
            if (!dep_node->visited && dfs_cycle_check(dep_node, graph)) {
                return 1;
            }
        }
    }
    
    node->in_stack = 0;
    return 0;
}

int dependency_detect_cycles(dependency_graph_t *graph) {
    if (!graph) return TINYPKG_ERROR;
    
    // Reset visited flags
    dependency_node_t *node = graph->nodes;
    while (node) {
        node->visited = 0;
        node->in_stack = 0;
        node = node->next;
    }
    
    // Check each unvisited node
    node = graph->nodes;
    while (node) {
        if (!node->visited) {
            if (dfs_cycle_check(node, graph)) {
                graph->has_cycles = 1;
                return TINYPKG_ERROR_DEPENDENCY;
            }
        }
        node = node->next;
    }
    
    return TINYPKG_SUCCESS;
}

// Node management
dependency_node_t *dependency_node_create(const char *package_name) {
    if (!package_name) return NULL;
    
    dependency_node_t *node = TINYPKG_CALLOC(1, sizeof(dependency_node_t));
    if (!node) return NULL;
    
    strncpy(node->package_name, package_name, sizeof(node->package_name) - 1);
    return node;
}

void dependency_node_free(dependency_node_t *node) {
    if (!node) return;
    
    if (node->dependencies) {
        for (int i = 0; i < node->dep_count; i++) {
            TINYPKG_FREE(node->dependencies[i]);
        }
        TINYPKG_FREE(node->dependencies);
    }
    
    TINYPKG_FREE(node);
}

dependency_node_t *dependency_graph_find_node(dependency_graph_t *graph, 
                                              const char *package_name) {
    if (!graph || !package_name) return NULL;
    
    dependency_node_t *node = graph->nodes;
    while (node) {
        if (strcmp(node->package_name, package_name) == 0) {
            return node;
        }
        node = node->next;
    }
    
    return NULL;
}

// Find packages that depend on the given package
int dependency_find_dependents(const char *package_name, char ***dependents, int *count) {
    if (!package_name || !dependents || !count) {
        return TINYPKG_ERROR;
    }
    
    *dependents = NULL;
    *count = 0;
    
    // Load package database and check each installed package
    if (package_db_load() != TINYPKG_SUCCESS) {
        return TINYPKG_ERROR;
    }
    
    char **result = NULL;
    int result_count = 0;
    int result_capacity = 16;
    
    result = TINYPKG_MALLOC(sizeof(char*) * result_capacity);
    if (!result) return TINYPKG_ERROR_MEMORY;
    
    package_db_entry_t *entry = package_db_get_all();
    while (entry) {
        package_t *pkg = package_load_info(entry->name);
        if (pkg && pkg->dependencies) {
            // Check if this package depends on the target package
            for (int i = 0; i < pkg->dep_count; i++) {
                if (strcmp(pkg->dependencies[i], package_name) == 0) {
                    // Expand array if needed
                    if (result_count >= result_capacity) {
                        result_capacity *= 2;
                        char **new_result = TINYPKG_REALLOC(result, sizeof(char*) * result_capacity);
                        if (!new_result) {
                            utils_string_array_free(result, result_count);
                            package_free(pkg);
                            return TINYPKG_ERROR_MEMORY;
                        }
                        result = new_result;
                    }
                    
                    result[result_count] = TINYPKG_STRDUP(entry->name);
                    if (!result[result_count]) {
                        utils_string_array_free(result, result_count);
                        package_free(pkg);
                        return TINYPKG_ERROR_MEMORY;
                    }
                    result_count++;
                    break; // Found dependency, no need to check others
                }
            }
        }
        if (pkg) package_free(pkg);
        entry = entry->next;
    }
    
    if (result_count == 0) {
        TINYPKG_FREE(result);
        result = NULL;
    }
    
    *dependents = result;
    *count = result_count;
    return TINYPKG_SUCCESS;
}

int dependency_check_satisfied(const char *package_name) {
    if (!package_name) return 0;
    
    // For now, simply check if package is installed
    // In a full implementation, this would check version requirements
    return package_is_installed(package_name);
}
