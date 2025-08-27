/*
 * TinyPkg - Logging System Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <pthread.h>
#include "../include/tinypkg.h"

// Global state
static log_config_t g_log_config = {
    .min_level = LOG_INFO,
    .output_flags = LOG_OUTPUT_CONSOLE,
    .log_file = {0},
    .max_file_size = 10 * 1024 * 1024, // 10MB
    .max_backup_files = 5,
    .use_colors = 1,
    .show_timestamps = 1,
    .show_thread_id = 0
};

static FILE *g_log_file = NULL;
static int g_syslog_enabled = 0;
static int g_initialized = 0;
static int g_thread_safe = 1;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static log_stats_t g_stats = {0};
static log_filter_func_t g_filters[8] = {0};
static int g_filter_count = 0;

// Color codes
static const char* const LOG_COLORS[] = {
    "\033[36m",   // DEBUG - Cyan
    "\033[32m",   // INFO - Green
    "\033[33m",   // WARN - Yellow
    "\033[31m",   // ERROR - Red
    "\033[35m"    // FATAL - Magenta
};
static const char* const LOG_RESET = "\033[0m";

// Level strings
static const char* const LOG_LEVEL_STRINGS[] = {
    "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

// Helper functions
static void lock_if_needed(void) {
    if (g_thread_safe) {
        pthread_mutex_lock(&g_log_mutex);
    }
}

static void unlock_if_needed(void) {
    if (g_thread_safe) {
        pthread_mutex_unlock(&g_log_mutex);
    }
}

static int should_log_level(log_level_t level) {
    return level >= g_log_config.min_level;
}

static void update_stats(log_level_t level, size_t bytes) {
    switch (level) {
        case LOG_DEBUG: g_stats.debug_count++; break;
        case LOG_INFO:  g_stats.info_count++; break;
        case LOG_WARN:  g_stats.warn_count++; break;
        case LOG_ERROR: g_stats.error_count++; break;
        case LOG_FATAL: g_stats.fatal_count++; break;
    }
    g_stats.bytes_written += bytes;
}

static int check_file_size_and_rotate(void) {
    if (!g_log_file) return 0;
    
    long size = ftell(g_log_file);
    if (size >= g_log_config.max_file_size) {
        return logging_rotate_files();
    }
    return 0;
}

static void format_timestamp(char *buffer, size_t size) {
    struct timespec ts;
    struct tm *tm_info;
    
    clock_gettime(CLOCK_REALTIME, &ts);
    tm_info = localtime(&ts.tv_sec);
    
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Add milliseconds
    char ms_buffer[16];
    snprintf(ms_buffer, sizeof(ms_buffer), ".%03ld", ts.tv_nsec / 1000000);
    strncat(buffer, ms_buffer, size - strlen(buffer) - 1);
}

static void format_log_message(char *output, size_t output_size,
                              log_level_t level, const char *file,
                              int line, const char *func,
                              const char *message) {
    char timestamp[64] = {0};
    char thread_id[32] = {0};
    const char *basename = strrchr(file, '/');
    if (basename) basename++;
    else basename = file;
    
    if (g_log_config.show_timestamps) {
        format_timestamp(timestamp, sizeof(timestamp));
    }
    
    if (g_log_config.show_thread_id) {
        snprintf(thread_id, sizeof(thread_id), "[%lu] ", 
                (unsigned long)pthread_self());
    }
    
    snprintf(output, output_size, "%s%s[%s] %s%s:%d %s(): %s",
             g_log_config.show_timestamps ? timestamp : "",
             g_log_config.show_timestamps ? " " : "",
             LOG_LEVEL_STRINGS[level],
             thread_id,
             basename, line, func, message);
}

static int apply_filters(log_level_t level, const char *file,
                        int line, const char *func,
                        const char *message) {
    for (int i = 0; i < g_filter_count; i++) {
        if (g_filters[i] && !g_filters[i](level, file, line, func, message)) {
            return 0; // Filter rejected the message
        }
    }
    return 1; // All filters passed
}

// Public functions
int logging_init(void) {
    return logging_init_with_config(&g_log_config);
}

int logging_init_with_config(const log_config_t *config) {
    lock_if_needed();
    
    if (g_initialized) {
        unlock_if_needed();
        return 0;
    }
    
    if (config) {
        g_log_config = *config;
    }
    
    // Initialize stats
    g_stats.start_time = time(NULL);
    
    // Open log file if specified
    if (g_log_config.output_flags & LOG_OUTPUT_FILE && 
        strlen(g_log_config.log_file) > 0) {
        
        // Create log directory if needed
        char log_dir[256];
        strcpy(log_dir, g_log_config.log_file);
        char *last_slash = strrchr(log_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            mkdir(log_dir, 0755);
        }
        
        g_log_file = fopen(g_log_config.log_file, "a");
        if (!g_log_file) {
            fprintf(stderr, "Failed to open log file: %s (%s)\n",
                    g_log_config.log_file, strerror(errno));
            unlock_if_needed();
            return -1;
        }
        setbuf(g_log_file, NULL); // Unbuffered for immediate writes
    }
    
    // Initialize syslog if enabled
    if (g_log_config.output_flags & LOG_OUTPUT_SYSLOG) {
        openlog("tinypkg", LOG_PID | LOG_NDELAY, LOG_USER);
        g_syslog_enabled = 1;
    }
    
    g_initialized = 1;
    
    unlock_if_needed();
    
    log_info("Logging system initialized");
    return 0;
}

void logging_cleanup(void) {
    lock_if_needed();
    
    if (!g_initialized) {
        unlock_if_needed();
        return;
    }
    
    log_info("Shutting down logging system");
    
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    
    if (g_syslog_enabled) {
        closelog();
        g_syslog_enabled = 0;
    }
    
    g_initialized = 0;
    
    unlock_if_needed();
}

void log_message(log_level_t level, const char *file, int line,
                const char *func, const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message_va(level, file, line, func, format, args);
    va_end(args);
}

void log_message_va(log_level_t level, const char *file, int line,
                   const char *func, const char *format, va_list args) {
    if (!should_log_level(level)) {
        return;
    }
    
    char message[2048];
    char formatted[2560];
    
    // Format the actual message
    vsnprintf(message, sizeof(message), format, args);
    
    // Apply filters
    if (!apply_filters(level, file, line, func, message)) {
        return;
    }
    
    lock_if_needed();
    
    // Format the complete log entry
    format_log_message(formatted, sizeof(formatted), 
                      level, file, line, func, message);
    
    size_t bytes_written = 0;
    
    // Output to console
    if (g_log_config.output_flags & LOG_OUTPUT_CONSOLE) {
        FILE *output = (level >= LOG_ERROR) ? stderr : stdout;
        
        if (g_log_config.use_colors && isatty(fileno(output))) {
            fprintf(output, "%s%s%s\n", 
                   LOG_COLORS[level], formatted, LOG_RESET);
        } else {
            fprintf(output, "%s\n", formatted);
        }
        bytes_written += strlen(formatted) + 1;
    }
    
    // Output to file
    if ((g_log_config.output_flags & LOG_OUTPUT_FILE) && g_log_file) {
        fprintf(g_log_file, "%s\n", formatted);
        bytes_written += strlen(formatted) + 1;
        check_file_size_and_rotate();
    }
    
    // Output to syslog
    if ((g_log_config.output_flags & LOG_OUTPUT_SYSLOG) && g_syslog_enabled) {
        int syslog_priority;
        switch (level) {
            case LOG_DEBUG: syslog_priority = LOG_DEBUG; break;
            case LOG_INFO:  syslog_priority = LOG_INFO; break;
            case LOG_WARN:  syslog_priority = LOG_WARNING; break;
            case LOG_ERROR: syslog_priority = LOG_ERR; break;
            case LOG_FATAL: syslog_priority = LOG_CRIT; break;
            default: syslog_priority = LOG_INFO; break;
        }
        syslog(syslog_priority, "%s", message);
    }
    
    update_stats(level, bytes_written);
    
    unlock_if_needed();
    
    // Exit on fatal errors
    if (level == LOG_FATAL) {
        logging_cleanup();
        exit(EXIT_FAILURE);
    }
}

int logging_set_level(log_level_t level) {
    lock_if_needed();
    g_log_config.min_level = level;
    unlock_if_needed();
    return 0;
}

log_level_t logging_get_level(void) {
    return g_log_config.min_level;
}

int logging_set_output(int output_flags) {
    lock_if_needed();
    g_log_config.output_flags = output_flags;
    unlock_if_needed();
    return 0;
}

int logging_set_file(const char *filename) {
    if (!filename) return -1;
    
    lock_if_needed();
    
    // Close existing file
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    
    strncpy(g_log_config.log_file, filename, sizeof(g_log_config.log_file) - 1);
    g_log_config.log_file[sizeof(g_log_config.log_file) - 1] = '\0';
    
    // Open new file
    g_log_file = fopen(filename, "a");
    if (!g_log_file) {
        unlock_if_needed();
        return -1;
    }
    
    g_log_config.output_flags |= LOG_OUTPUT_FILE;
    
    unlock_if_needed();
    return 0;
}

int logging_rotate_files(void) {
    if (!g_log_file) return 0;
    
    lock_if_needed();
    
    fclose(g_log_file);
    g_log_file = NULL;
    
    // Rotate backup files
    char old_file[512], new_file[512];
    
    // Remove oldest backup
    snprintf(old_file, sizeof(old_file), "%s.%d", 
             g_log_config.log_file, g_log_config.max_backup_files);
    unlink(old_file);
    
    // Rotate existing backups
    for (int i = g_log_config.max_backup_files - 1; i > 0; i--) {
        snprintf(old_file, sizeof(old_file), "%s.%d", 
                 g_log_config.log_file, i);
        snprintf(new_file, sizeof(new_file), "%s.%d", 
                 g_log_config.log_file, i + 1);
        rename(old_file, new_file);
    }
    
    // Move current log to .1
    snprintf(new_file, sizeof(new_file), "%s.1", g_log_config.log_file);
    rename(g_log_config.log_file, new_file);
    
    // Open new log file
    g_log_file = fopen(g_log_config.log_file, "w");
    
    unlock_if_needed();
    
    return (g_log_file != NULL) ? 0 : -1;
}

const log_stats_t *logging_get_stats(void) {
    return &g_stats;
}

void logging_reset_stats(void) {
    lock_if_needed();
    memset(&g_stats, 0, sizeof(g_stats));
    g_stats.start_time = time(NULL);
    unlock_if_needed();
}

void logging_print_stats(void) {
    lock_if_needed();
    
    time_t uptime = time(NULL) - g_stats.start_time;
    unsigned long total = g_stats.debug_count + g_stats.info_count + 
                         g_stats.warn_count + g_stats.error_count + 
                         g_stats.fatal_count;
    
    printf("Logging Statistics:\n");
    printf("  Uptime: %ld seconds\n", uptime);
    printf("  Total messages: %lu\n", total);
    printf("  DEBUG: %lu\n", g_stats.debug_count);
    printf("  INFO:  %lu\n", g_stats.info_count);
    printf("  WARN:  %lu\n", g_stats.warn_count);
    printf("  ERROR: %lu\n", g_stats.error_count);
    printf("  FATAL: %lu\n", g_stats.fatal_count);
    printf("  Bytes written: %zu\n", g_stats.bytes_written);
    
    unlock_if_needed();
}

const char *log_level_to_string(log_level_t level) {
    if (level >= 0 && level < (int)(sizeof(LOG_LEVEL_STRINGS) / sizeof(LOG_LEVEL_STRINGS[0]))) {
        return LOG_LEVEL_STRINGS[level];
    }
    return "UNKNOWN";
}

log_level_t log_level_from_string(const char *level_str) {
    if (!level_str) return LOG_INFO;
    
    if (strcasecmp(level_str, "debug") == 0) return LOG_DEBUG;
    if (strcasecmp(level_str, "info") == 0) return LOG_INFO;
    if (strcasecmp(level_str, "warn") == 0) return LOG_WARN;
    if (strcasecmp(level_str, "error") == 0) return LOG_ERROR;
    if (strcasecmp(level_str, "fatal") == 0) return LOG_FATAL;
    
    return LOG_INFO;
}

void log_hex_dump(log_level_t level, const void *data, size_t size, 
                 const char *description) {
    if (!should_log_level(level) || !data) return;
    
    const unsigned char *bytes = (const unsigned char *)data;
    char line[128];
    char hex_part[49];
    char ascii_part[17];
    
    log_message(level, __FILE__, __LINE__, __func__, 
               "Hex dump of %s (%zu bytes):", description, size);
    
    for (size_t i = 0; i < size; i += 16) {
        memset(hex_part, 0, sizeof(hex_part));
        memset(ascii_part, 0, sizeof(ascii_part));
        
        // Format hex part
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            snprintf(hex_part + j * 3, 4, "%02x ", bytes[i + j]);
            ascii_part[j] = (bytes[i + j] >= 32 && bytes[i + j] < 127) ? 
                           bytes[i + j] : '.';
        }
        
        snprintf(line, sizeof(line), "%08zx  %-48s |%s|", i, hex_part, ascii_part);
        log_message(level, __FILE__, __LINE__, __func__, "%s", line);
    }
}

void log_timer_start(log_timer_t *timer, const char *operation) {
    if (!timer) return;
    
    clock_gettime(CLOCK_MONOTONIC, &timer->start_time);
    strncpy(timer->operation, operation, sizeof(timer->operation) - 1);
    timer->operation[sizeof(timer->operation) - 1] = '\0';
}

void log_timer_end(log_timer_t *timer) {
    if (!timer) return;
    
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    
    long diff_ns = (end_time.tv_sec - timer->start_time.tv_sec) * 1000000000L +
                   (end_time.tv_nsec - timer->start_time.tv_nsec);
    
    double diff_ms = diff_ns / 1000000.0;
    
    log_info("Timer '%s': %.3f ms", timer->operation, diff_ms);
}

int logging_add_filter(log_filter_func_t filter) {
    if (!filter || g_filter_count >= 8) return -1;
    
    lock_if_needed();
    g_filters[g_filter_count++] = filter;
    unlock_if_needed();
    
    return 0;
}

int logging_remove_filter(log_filter_func_t filter) {
    if (!filter) return -1;
    
    lock_if_needed();
    
    for (int i = 0; i < g_filter_count; i++) {
        if (g_filters[i] == filter) {
            // Shift remaining filters
            for (int j = i; j < g_filter_count - 1; j++) {
                g_filters[j] = g_filters[j + 1];
            }
            g_filter_count--;
            g_filters[g_filter_count] = NULL;
            unlock_if_needed();
            return 0;
        }
    }
    
    unlock_if_needed();
    return -1;
}

void logging_clear_filters(void) {
    lock_if_needed();
    memset(g_filters, 0, sizeof(g_filters));
    g_filter_count = 0;
    unlock_if_needed();
}
