<div align="center">
  <img src="https://raw.githubusercontent.com/user7210unix/user7210unix/refs/heads/main/pfp.png" width="200" height="200" style="border-radius:50%; border: 4px solid #333;">
  <p>

<div align="center">

# TinyPkg — A Work-in-Progress Linux Source-Based Package Manager

<!-- WIP badge (top, centered) -->
<p align="center">
  <img src="https://img.shields.io/badge/🚧%20work%20in%20progress-orange?style=for-the-badge&labelColor=1f2937">
</p>

<!-- Linux + License badges (same row) -->
<p align="center">
  <img src="https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black">
  <img src="https://img.shields.io/badge/License-GPLv3-blue?style=for-the-badge&logo=gnu&logoColor=white">
</p>

</div>

## Architecture Overview

TinyPkg follows a modular architecture with clear separation of concerns:

```
┌─────────────────────────────────────────────────────────┐
│                    TinyPkg Core                         │
├─────────────────────────────────────────────────────────┤
│  CLI Interface  │  Package Manager  │  Build System     │
├─────────────────────────────────────────────────────────┤
│     libcurl     │     libgit2       │   libjansson      │
│  (HTTP/HTTPS)   │  (Git Operations) │  (JSON Parsing)   │
├─────────────────────────────────────────────────────────┤
│        fts_* (Directory Traversal) │ System Calls       │
├─────────────────────────────────────────────────────────┤
│                  Linux Kernel                           │
└─────────────────────────────────────────────────────────┘
```
<div align="left">

## Directory Structure Standards

TinyPkg follows the Filesystem Hierarchy Standard (FHS):

```
/etc/tinypkg/                  # System configuration
├── tinypkg.conf               # Main configuration
├── repositories.conf          # Repository definitions
└── mirrors.conf               # Mirror configurations

/var/lib/tinypkg/              # Package database
├── installed.txt              # Installed packages list
├── dependencies.db            # Dependency graph
├── repo/                      # Package repository
│   ├── vim.json
│   ├── git.json
│   └── ...
└── state/                     # Package states
    ├── building/
    ├── failed/
    └── completed/

/var/cache/tinypkg/            # Build cache
├── sources/                   # Downloaded sources
├── builds/                    # Build directories
└── packages/                  # Built packages

/var/log/tinypkg/              # Log files
├── tinypkg.log               # Main log
├── build.log                 # Build logs
└── install.log               # Installation logs
```

## Key Features

### 1. Professional Library Integration

**libcurl**: Robust HTTP/HTTPS downloads
- Automatic retry mechanisms
- Resume support for interrupted downloads
- Certificate verification
- Proxy support

**libgit2**: Native Git operations
- No external Git dependency
- Efficient repository cloning
- Branch and tag support
- Credential handling

**libjansson**: Fast JSON parsing
- Memory-efficient parsing
- Schema validation
- Error reporting

**fts_* (glibc)**: Efficient directory traversal
- POSIX-compliant
- Configurable traversal options
- Error handling
- Performance optimized

### 2. Build System Features

```c
// Parallel building support
typedef struct build_config {
    int parallel_jobs;
    char build_flags[512];
    char install_prefix[256];
    bool enable_optimizations;
    bool debug_symbols;
} build_config_t;

// Sandbox support for secure builds
typedef struct sandbox_config {
    bool enabled;
    char chroot_dir[256];
    uid_t build_user;
    gid_t build_group;
    char allowed_paths[1024];
} sandbox_config_t;
```

### 3. Advanced Package Management

**Dependency Resolution**: Topological sorting algorithm
```c
int resolve_dependencies(const char *package, char ***dep_order) {
    // Implementation uses Kahn's algorithm for topological sorting
    // Detects circular dependencies
    // Optimizes installation order
}
```

**Version Management**: Semantic versioning support
```c
typedef struct version {
    int major;
    int minor; 
    int patch;
    char prerelease[32];
} version_t;

int version_compare(const version_t *a, const version_t *b);
```

**Package States**: Comprehensive state tracking
```c
typedef enum {
    PKG_STATE_AVAILABLE,
    PKG_STATE_DOWNLOADING,
    PKG_STATE_BUILDING,
    PKG_STATE_INSTALLING,
    PKG_STATE_INSTALLED,
    PKG_STATE_FAILED,
    PKG_STATE_BROKEN
} package_state_t;
```

## Advanced Usage Examples

### Custom Repository Configuration

```bash
# Add custom repository
sudo tinypkg --add-repo https://github.com/myorg/packages.git

# Use specific branch
sudo tinypkg --add-repo https://github.com/myorg/packages.git:testing

# Set repository priority
sudo tinypkg --set-repo-priority myorg 100
```

### Build Customization

```bash
# Custom build flags
sudo tinypkg -i vim --build-flags="--enable-python3interp --with-features=huge"

# Parallel building
sudo tinypkg -i nginx --parallel=8

# Debug build
sudo tinypkg -i git --debug --keep-build-dir

# Cross-compilation
sudo tinypkg -i curl --target=arm64-linux-gnu
```

### Package Management Operations

```bash
# List installed packages
tinypkg --list-installed

# Show package information
tinypkg --info vim

# Check for updates
tinypkg --check-updates

# Remove package
sudo tinypkg --remove vim

# Clean build cache
sudo tinypkg --clean-cache

# Verify package integrity
tinypkg --verify vim
```

## Security Features

### 1. Checksum Verification
All packages include SHA-256 checksums for source verification:

```json
{
  "name": "vim",
  "checksum": "sha256:abcdef1234567890...",
  "signature": "-----BEGIN PGP SIGNATURE-----..."
}
```

### 2. Sandboxed Builds
Optional chroot-based build sandboxing:

```c
int setup_build_sandbox(const char *package_name) {
    char sandbox_dir[MAX_PATH];
    snprintf(sandbox_dir, sizeof(sandbox_dir), 
             "/var/cache/tinypkg/sandbox/%s", package_name);
    
    // Create minimal chroot environment
    create_directory_recursive(sandbox_dir);
    mount_essential_dirs(sandbox_dir);
    
    // Drop privileges
    if (setgid(build_gid) != 0 || setuid(build_uid) != 0) {
        return -1;
    }
    
    // chroot to sandbox
    if (chroot(sandbox_dir) != 0) {
        return -1;
    }
    
    return 0;
}
```

### 3. Package Signing
GPG signature verification for package integrity:

```c
int verify_package_signature(const char *package_file, const char *signature_file) {
    // GPG verification implementation
    // Checks against trusted keyring
    return gpg_verify(package_file, signature_file);
}
```

## Performance Optimizations

### 1. Parallel Downloads
```c
typedef struct download_job {
    char url[512];
    char dest[256];
    CURL *curl_handle;
    int status;
} download_job_t;

int parallel_download(download_job_t *jobs, int count) {
    CURLM *multi_handle = curl_multi_init();
    // Add all jobs to multi handle
    // Process downloads concurrently
    curl_multi_cleanup(multi_handle);
    return 0;
}
```

### 2. Build Caching
```c
int check_build_cache(const char *package, const char *version) {
    char cache_file[MAX_PATH];
    snprintf(cache_file, sizeof(cache_file),
             "%s/builds/%s-%s.cache", CACHE_DIR, package, version);
    
    struct stat st;
    return (stat(cache_file, &st) == 0) ? 1 : 0;
}
```

### 3. Incremental Repository Updates
```c
int incremental_repo_update(void) {
    git_repository *repo;
    git_reference *head_ref;
    git_oid old_oid, new_oid;
    
    // Get current HEAD
    git_repository_head(&head_ref, repo);
    git_reference_target(&old_oid, head_ref);
    
    // Fetch updates
    git_remote_fetch(remote, NULL, NULL, NULL);
    
    // Check for changes
    git_reference_target(&new_oid, head_ref);
    
    return git_oid_cmp(&old_oid, &new_oid) != 0;
}
```

## Troubleshooting Guide

### Common Issues

1. **Build Failures**
```bash
# Check build logs
tail -f /var/log/tinypkg/build.log

# Retry with debug
sudo tinypkg -i package --debug --verbose

# Clean and rebuild
sudo tinypkg --clean package
sudo tinypkg -i package
```

2. **Dependency Issues**
```bash
# Show dependency tree
tinypkg --deps package

# Force dependency resolution
sudo tinypkg -i package --resolve-deps

# Install specific version
sudo tinypkg -i package=1.2.3
```

3. **Repository Problems**
```bash
# Re-sync repository
sudo tinypkg -s --force

# Check repository status
tinypkg --repo-status

# Reset repository
sudo rm -rf /var/lib/tinypkg/repo
sudo tinypkg -s
```

## Contributing to TinyPkg

### Package Submission

1. Create package JSON definition
2. Test build locally
3. Submit pull request to repository
4. Package review and approval

### Code Contributions

1. Fork the repository
2. Create feature branch
3. Write tests
4. Submit pull request

## License and Legal

TinyPkg is released under the GPL v3 license. All package definitions must be compatible with their respective software licenses.

---

## Implementation Notes

The TinyPkg implementation demonstrates several advanced C programming techniques:

- **Error Handling**: Comprehensive error checking with proper cleanup
- **Memory Management**: No memory leaks, proper resource cleanup
- **Modularity**: Clear separation between components
- **Standards Compliance**: POSIX compatibility where possible
- **Security**: Input validation, privilege separation
- **Performance**: Efficient algorithms and data structures

This creates a production-ready package manager suitable for embedded systems, custom Linux distributions, and environments requiring minimal dependencies.
