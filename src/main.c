/*
 * TinyPkg - Main Entry Point
 * Handles command line argument parsing and program flow
 */

#include <getopt.h>
#include <signal.h>
#include "../include/tinypkg.h"

// Global variables
int verbose_mode = 0;
int debug_mode = 0;
config_t *global_config = NULL;

// Signal handling
static volatile sig_atomic_t interrupted = 0;

static void signal_handler(int sig) {
    UNUSED(sig);
    interrupted = 1;
    log_warn("Received interrupt signal, cleaning up...");
}

static void setup_signal_handlers(void) {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void print_version(void) {
    printf("TinyPkg version %s\n", TINYPKG_VERSION);
    printf("Lightweight source-based package manager for Linux\n");
    printf("Built with: libcurl, libgit2, libjansson\n");
}

static void print_usage(const char *prog_name) {
    printf("TinyPkg v%s - Source-based Linux Package Manager\n\n", TINYPKG_VERSION);
    printf("Usage: %s [OPTIONS] [COMMAND]\n\n", prog_name);
    
    printf("Commands:\n");
    printf("  -i, --install PACKAGE    Install a package from source\n");
    printf("  -r, --remove PACKAGE     Remove an installed package\n");
    printf("  -s, --sync               Synchronize package repository\n");
    printf("  -u, --update [PACKAGE]   Update package(s) to latest version\n");
    printf("  -l, --list [PATTERN]     List installed packages\n");
    printf("  -q, --query PACKAGE      Show package information\n");
    printf("  -S, --search PATTERN     Search for packages\n");
    printf("  -c, --clean              Clean build cache\n");
    
    printf("\nOptions:\n");
    printf("  -v, --verbose            Enable verbose output\n");
    printf("  -d, --debug              Enable debug output\n");
    printf("  -f, --force              Force operation\n");
    printf("  -y, --yes                Assume yes to all prompts\n");
    printf("  -n, --no-deps            Skip dependency resolution\n");
    printf("  -j, --parallel N         Use N parallel build jobs\n");
    printf("      --config FILE        Use alternative config file\n");
    printf("      --root DIR           Use alternative root directory\n");
    printf("  -h, --help               Show this help message\n");
    printf("      --version            Show version information\n");
    
    printf("\nExamples:\n");
    printf("  %s -s                    # Sync repository\n", prog_name);
    printf("  %s -i vim                # Install vim package\n", prog_name);
    printf("  %s -r vim                # Remove vim package\n", prog_name);
    printf("  %s -q vim                # Show vim package info\n", prog_name);
    printf("  %s -S editor             # Search for editor packages\n", prog_name);
    printf("  %s -i git -j 8           # Install git with 8 parallel jobs\n", prog_name);
    
    printf("\nDirectories:\n");
    printf("  Configuration: %s\n", CONFIG_DIR);
    printf("  Cache:         %s\n", CACHE_DIR);
    printf("  Database:      %s\n", LIB_DIR);
    printf("  Repository:    %s\n", REPO_DIR);
    printf("  Logs:          %s\n", LOG_DIR);
}

static int check_privileges(void) {
    if (geteuid() != 0) {
        log_error("TinyPkg requires root privileges for system operations");
        log_info("Try running with sudo or as root user");
        return TINYPKG_ERROR;
    }
    return TINYPKG_SUCCESS;
}

static int initialize_system(void) {
    int result;
    
    // Initialize logging first
    result = logging_init();
    if (result != TINYPKG_SUCCESS) {
        fprintf(stderr, "Failed to initialize logging system\n");
        return result;
    }
    
    log_info("TinyPkg %s starting up", TINYPKG_VERSION);
    
    // Load configuration
    global_config = config_load();
    if (!global_config) {
        log_warn("Failed to load configuration, using defaults");
        global_config = config_create_default();
        if (!global_config) {
            log_error("Failed to create default configuration");
            return TINYPKG_ERROR_MEMORY;
        }
    }
    
    // Initialize directories
    result = utils_init_directories();
    if (result != TINYPKG_SUCCESS) {
        log_error("Failed to initialize system directories");
        return result;
    }
    
    // Initialize download system
    result = download_init();
    if (result != TINYPKG_SUCCESS) {
        log_error("Failed to initialize download system");
        return result;
    }
    
    log_info("System initialization completed successfully");
    return TINYPKG_SUCCESS;
}

static void cleanup_system(void) {
    log_info("Shutting down TinyPkg");
    
    download_cleanup();
    config_free(global_config);
    logging_cleanup();
}

int main(int argc, char *argv[]) {
    int opt, option_index = 0;
    int result = TINYPKG_SUCCESS;
    
    // Command flags
    int install_flag = 0;
    int remove_flag = 0;
    int sync_flag = 0;
    int update_flag = 0;
    int list_flag = 0;
    int query_flag = 0;
    int search_flag = 0;
    int clean_flag = 0;
    int force_flag = 0;
    int yes_flag = 0;
    int no_deps_flag = 0;
    
    // Command arguments
    char *package_name = NULL;
    char *search_pattern = NULL;
    char *config_file = NULL;
    char *root_dir = NULL;
    int parallel_jobs = 0;
    
    static struct option long_options[] = {
        {"install",     required_argument, 0, 'i'},
        {"remove",      required_argument, 0, 'r'},
        {"sync",        no_argument,       0, 's'},
        {"update",      optional_argument, 0, 'u'},
        {"list",        optional_argument, 0, 'l'},
        {"query",       required_argument, 0, 'q'},
        {"search",      required_argument, 0, 'S'},
        {"clean",       no_argument,       0, 'c'},
        {"verbose",     no_argument,       0, 'v'},
        {"debug",       no_argument,       0, 'd'},
        {"force",       no_argument,       0, 'f'},
        {"yes",         no_argument,       0, 'y'},
        {"no-deps",     no_argument,       0, 'n'},
        {"parallel",    required_argument, 0, 'j'},
        {"config",      required_argument, 0, 1001},
        {"root",        required_argument, 0, 1002},
        {"help",        no_argument,       0, 'h'},
        {"version",     no_argument,       0, 1000},
        {0, 0, 0, 0}
    };
    
    if (argc == 1) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Parse command line options
    while ((opt = getopt_long(argc, argv, "i:r:su::l::q:S:cvdfynj:h", 
                              long_options, &option_index)) != -1) {
        switch (opt) {
            case 'i':
                install_flag = 1;
                package_name = optarg;
                break;
            case 'r':
                remove_flag = 1;
                package_name = optarg;
                break;
            case 's':
                sync_flag = 1;
                break;
            case 'u':
                update_flag = 1;
                package_name = optarg; // Can be NULL for update all
                break;
            case 'l':
                list_flag = 1;
                search_pattern = optarg; // Can be NULL for list all
                break;
            case 'q':
                query_flag = 1;
                package_name = optarg;
                break;
            case 'S':
                search_flag = 1;
                search_pattern = optarg;
                break;
            case 'c':
                clean_flag = 1;
                break;
            case 'v':
                verbose_mode = 1;
                break;
            case 'd':
                debug_mode = 1;
                verbose_mode = 1; // Debug implies verbose
                break;
            case 'f':
                force_flag = 1;
                break;
            case 'y':
                yes_flag = 1;
                break;
            case 'n':
                no_deps_flag = 1;
                break;
            case 'j':
                parallel_jobs = atoi(optarg);
                if (parallel_jobs <= 0 || parallel_jobs > 32) {
                    fprintf(stderr, "Invalid parallel jobs count: %d\n", parallel_jobs);
                    return 1;
                }
                break;
            case 1000: // --version
                print_version();
                return 0;
            case 1001: // --config
                config_file = optarg;
                break;
            case 1002: // --root
                root_dir = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Setup signal handlers
    setup_signal_handlers();
    
    // Check for root privileges (except for query operations)
    if (install_flag || remove_flag || sync_flag || update_flag || clean_flag) {
        if (check_privileges() != TINYPKG_SUCCESS) {
            return 1;
        }
    }
    
    // Initialize system
    result = initialize_system();
    if (result != TINYPKG_SUCCESS) {
        cleanup_system();
        return 1;
    }
    
    // Override configuration with command line options
    if (parallel_jobs > 0) {
        global_config->parallel_jobs = parallel_jobs;
    }
    if (force_flag) {
        global_config->force_mode = 1;
    }
    if (yes_flag) {
        global_config->assume_yes = 1;
    }
    if (no_deps_flag) {
        global_config->skip_dependencies = 1;
    }
    
    // Execute commands
    if (sync_flag) {
        log_info("Synchronizing package repository");
        result = repository_sync();
        if (result != TINYPKG_SUCCESS) {
            log_error("Repository sync failed");
        } else {
            log_info("Repository sync completed successfully");
        }
    }
    
    if (install_flag && package_name) {
        if (interrupted) goto cleanup;
        log_info("Installing package: %s", package_name);
        result = package_install(package_name);
        if (result != TINYPKG_SUCCESS) {
            log_error("Package installation failed: %s", package_name);
        } else {
            log_info("Package installed successfully: %s", package_name);
        }
    }
    
    if (remove_flag && package_name) {
        if (interrupted) goto cleanup;
        log_info("Removing package: %s", package_name);
        result = package_remove(package_name);
        if (result != TINYPKG_SUCCESS) {
            log_error("Package removal failed: %s", package_name);
        } else {
            log_info("Package removed successfully: %s", package_name);
        }
    }
    
    if (update_flag) {
        if (interrupted) goto cleanup;
        if (package_name) {
            log_info("Updating package: %s", package_name);
            result = package_update(package_name);
        } else {
            log_info("Updating all packages");
            result = package_update_all();
        }
    }
    
    if (query_flag && package_name) {
        result = package_query(package_name);
    }
    
    if (search_flag && search_pattern) {
        result = package_search(search_pattern);
    }
    
    if (list_flag) {
        result = package_list(search_pattern);
    }
    
    if (clean_flag) {
        if (interrupted) goto cleanup;
        log_info("Cleaning build cache");
        result = utils_clean_cache();
        if (result != TINYPKG_SUCCESS) {
            log_error("Cache cleaning failed");
        } else {
            log_info("Cache cleaned successfully");
        }
    }

cleanup:
    cleanup_system();
    
    if (interrupted) {
        log_info("Operation interrupted by user");
        return 130; // Standard exit code for SIGINT
    }
    
    return (result == TINYPKG_SUCCESS) ? 0 : 1;
}
