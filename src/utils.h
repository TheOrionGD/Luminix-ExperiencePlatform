#ifndef UTILS_H
#define UTILS_H

#ifdef _WIN32
#include <windows.h>   // <-- REQUIRED for LARGE_INTEGER
#endif
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


// Constants
#define MAX_TABLE_NAME_LEN 64
#define MAX_COLUMN_NAME_LEN 64
#define MAX_DATABASE_NAME_LEN 64
#define MAX_PATH_LEN 256
#define MAX_QUERY_LEN 4096
#define INITIAL_CAPACITY 100

// Color codes
extern const char *COLOR_RESET;
extern const char *COLOR_BOLD;
extern const char *COLOR_RED;
extern const char *COLOR_GREEN;
extern const char *COLOR_YELLOW;
extern const char *COLOR_BLUE;
extern const char *COLOR_MAGENTA;
extern const char *COLOR_CYAN;
extern const char *COLOR_WHITE;
extern const char *COLOR_BG_BLUE;
extern const char *COLOR_BG_GREEN;
extern const char *COLOR_BG_RED;

// Configuration structure
typedef struct {
    char default_database_path[MAX_PATH_LEN];
    int auto_save;
    int auto_backup;
    int max_history_size;
    int enable_colors;
    int log_level;
    int max_cache_size;
} Config;

// Log levels
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR 3

// Logger structure
typedef struct {
    FILE* file;
    int level;
} Logger;

// Performance timer structure
typedef struct {
    #ifdef _WIN32
        LARGE_INTEGER start;
    #else
        struct timespec start;
    #endif
} PerformanceTimer;

// Dynamic array structure
typedef struct {
    void* data;
    size_t element_size;
    size_t size;
    size_t capacity;
} DynamicArray;

// ==================== String Utilities ====================
char* trim_whitespace(char* str);
char* to_lower_case(char* str);
char* to_upper_case(char* str);
int string_starts_with(const char* str, const char* prefix);
int string_ends_with(const char* str, const char* suffix);
char* string_replace(const char* str, const char* old, const char* new);
char** string_split(const char* str, const char* delimiter, int* count);
void free_string_array(char** array, int count);
char* join_strings(char** strings, int count, const char* delimiter);
int string_contains(const char* str, const char* substring);

// ==================== File System Utilities ====================
int file_exists(const char* filename);
int directory_exists(const char* dirname);
int create_directory(const char* dirname);
long get_file_size(const char* filename);
time_t get_file_modified_time(const char* filename);
char* get_file_extension(const char* filename);
char* get_filename_without_extension(const char* filename);
int copy_file(const char* source, const char* destination);
int delete_file(const char* filename);
int copy_directory(const char* source, const char* destination);

// ==================== Memory Management Utilities ====================
void* safe_malloc(size_t size);
void* safe_calloc(size_t num, size_t size);
void* safe_realloc(void* ptr, size_t size);
void safe_free(void** ptr);

// ==================== Date/Time Utilities ====================
char* get_current_timestamp();
char* get_current_date();
char* format_timestamp(time_t timestamp);
time_t parse_timestamp(const char* timestamp_str);

// ==================== Input/Output Utilities ====================
void clear_screen();
void print_header(const char* title);
void print_success(const char* message);
void print_error(const char* message);
void print_warning(const char* message);
void print_info(const char* message);
void print_table_header(const char** headers, int num_columns, const int* column_widths);
void print_table_row(const char** values, int num_columns, const int* column_widths);
void print_table_footer(int num_columns, const int* column_widths);
void print_progress_bar(int percentage, int width);

// ==================== Data Type Utilities ====================
int is_integer(const char* str);
int is_float(const char* str);
int is_boolean(const char* str);
int is_date(const char* str);
int string_to_int(const char* str, int* result);
int string_to_double(const char* str, double* result);
int string_to_bool(const char* str);

// ==================== Validation Utilities ====================
int validate_table_name(const char* name);
int validate_column_name(const char* name);
int validate_database_name(const char* name);
int validate_sql_identifier(const char* identifier);

// ==================== Hash and Comparison Utilities ====================
unsigned int hash_string(const char* str);
int compare_strings(const void* a, const void* b);
int compare_ints(const void* a, const void* b);
int compare_doubles(const void* a, const void* b);

// ==================== Configuration Utilities ====================
Config* load_config(const char* filename);
Config* create_default_config();
int save_config(const char* filename, Config* config);
void free_config(Config* config);

// ==================== Logging Utilities ====================
Logger* create_logger(const char* filename, int level);
void log_message(Logger* logger, int level, const char* message, ...);
void close_logger(Logger* logger);

// ==================== Backup Utilities ====================
int create_backup(const char* source_dir, const char* backup_dir);

// ==================== Miscellaneous Utilities ====================
void sleep_ms(int milliseconds);
int get_terminal_width();
int get_terminal_height();
char* read_line(FILE* stream);
char* read_multiline_input();

// ==================== Data Structure Utilities ====================
DynamicArray* create_dynamic_array(size_t element_size);
void dynamic_array_push(DynamicArray* array, void* element);
void* dynamic_array_get(DynamicArray* array, size_t index);
void free_dynamic_array(DynamicArray* array);

// ==================== Serialization Utilities ====================
char* serialize_int(int value);
char* serialize_double(double value);
char* serialize_bool(int value);
int deserialize_int(const char* str);
double deserialize_double(const char* str);
int deserialize_bool(const char* str);

// ==================== Performance Utilities ====================
PerformanceTimer* start_timer();
double stop_timer(PerformanceTimer* timer);
void print_performance_stats(const char* operation, double time_seconds, long bytes_processed);

#endif // UTILS_H