/*
 * TinyPkg - Dependency Resolution Header
 * Package dependency graph and resolution algorithms
 */

#ifndef TINYPKG_DEPENDENCY_H
#define TINYPKG_DEPENDENCY_H

// Dependency node for graph representation
typedef struct dependency_node {
    char package_name[MAX_NAME];
    char **dependencies;
    int dep_count;
    int visited;
    int in_stack;
    struct dependency_node *next;
} dependency_node_t;

// Dependency graph
typedef struct dependency_graph {
    dependency_node_t *nodes;
    int node_count;
    int has_cycles;
} dependency_graph_t;

// Function declarations

// Dependency resolution
int dependency_resolve(const char *package_name, char ***install_order, int *count);
int dependency_resolve_all(char **package_names, int package_count, 
                          char ***install_order, int *count);

// Dependency graph operations
dependency_graph_t *dependency_graph_create(void);
void dependency_graph_free(dependency_graph_t *graph);
int dependency_graph_add_package(dependency_graph_t *graph, const char *package_name);
int dependency_graph_build(dependency_graph_t *graph);
int dependency_graph_topological_sort(dependency_graph_t *graph, 
                                     char ***sorted_packages, int *count);

// Cycle detection
int dependency_detect_cycles(dependency_graph_t *graph);
int dependency_find_cycle_path(dependency_graph_t *graph, const char *package_name,
                              char ***cycle_path, int *path_length);

// Dependency queries  
int dependency_find_dependents(const char *package_name, char ***dependents, int *count);
int dependency_get_recursive_deps(const char *package_name, char ***deps, int *count);
int dependency_check_satisfied(const char *package_name);

// Node management
dependency_node_t *dependency_node_create(const char *package_name);
void dependency_node_free(dependency_node_t *node);
dependency_node_t *dependency_graph_find_node(dependency_graph_t *graph, 
                                              const char *package_name);

#endif /* TINYPKG_DEPENDENCY_H */
