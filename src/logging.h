/*
 * TinyPkg - Logging System Header
 * Provides comprehensive logging functionality with multiple levels and outputs
 */

#ifndef TINYPKG_LOGGING_H
#define TINYPKG_LOGGING_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* conflicting macros, undefine these and redefine them */
#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif
#ifdef LOG_INFO
#undef LOG_INFO
#endif
#ifdef LOG_WARN
#undef LOG_WARN
#endif
#ifdef LOG_ERROR
#undef LOG_ERROR
#endif
#ifdef LOG_FATAL
#undef LOG_FATAL
#endif

// Log levels
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_FATAL = 4
} log_level_t;

// Log output types
typedef enum {
    LOG_OUTPUT_FILE = 1,
    LOG_OUTPUT_SYSLOG = 2,
    LOG_OUTPUT_CONSOLE = 4
} log_output_t;

// Log configuration
typedef struct {
    log_level_t min_level;
    int output_flags;
    char log_file[256];
    int max_file_size;
    int max_backup_files;
    int use_colors;
    int show_timestamps;
    int show_thread_id;
} log_config_t;

// Function declarations

// System initialization
int logging_init(void);
int logging_init_with_config(const log_config_t *config);
void logging_cleanup(void);

// Configuration
int logging_set_level(log_level_t level);
log_level_t logging_get_level(void);
int logging_set_output(int output_flags);
int logging_set_file(const char *filename);
int logging_enable_syslog(const char *ident);
void logging_disable_syslog(void);

// Core logging functions
void log_message(log_level_t level, const char *file, int line, 
                const char *func, const char *format, ...);
void log_message_va(log_level_t level, const char *file, int line,
                   const char *func, const char *format, va_list args);

// Convenience macros
#define log_debug(fmt, ...) \
    log_message(LOG_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define log_info(fmt, ...) \
    log_message(LOG_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define log_warn(fmt, ...) \
    log_message(LOG_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define log_error(fmt, ...) \
    log_message(LOG_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define log_fatal(fmt, ...) \
    log_message(LOG_FATAL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

// Conditional logging
#define log_debug_if(cond, fmt, ...) \
    do { if (cond) log_debug(fmt, ##__VA_ARGS__); } while(0)

#define log_info_if(cond, fmt, ...) \
    do { if (cond) log_info(fmt, ##__VA_ARGS__); } while(0)

// Binary data logging
void log_hex_dump(log_level_t level, const void *data, size_t size, 
                 const char *description);

// Performance logging
typedef struct {
    struct timespec start_time;
    char operation[128];
} log_timer_t;

void log_timer_start(log_timer_t *timer, const char *operation);
void log_timer_end(log_timer_t *timer);

// Log rotation
int logging_rotate_files(void);
int logging_cleanup_old_files(int keep_count);

// Statistics
typedef struct {
    unsigned long debug_count;
    unsigned long info_count;
    unsigned long warn_count;
    unsigned long error_count;
    unsigned long fatal_count;
    time_t start_time;
    size_t bytes_written;
} log_stats_t;

const log_stats_t *logging_get_stats(void);
void logging_reset_stats(void);
void logging_print_stats(void);

// Utility functions
const char *log_level_to_string(log_level_t level);
log_level_t log_level_from_string(const char *level_str);
const char *log_level_to_color(log_level_t level);

// Thread safety
int logging_set_thread_safe(int enabled);
int logging_is_thread_safe(void);

// Filtering
typedef int (*log_filter_func_t)(log_level_t level, const char *file,
                                int line, const char *func, 
                                const char *message);

int logging_add_filter(log_filter_func_t filter);
int logging_remove_filter(log_filter_func_t filter);
void logging_clear_filters(void);

#endif /* TINYPKG_LOGGING_H */
