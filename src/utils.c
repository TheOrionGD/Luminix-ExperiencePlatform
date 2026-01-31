#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

// Color codes
const char *COLOR_RESET = "\033[0m";
const char *COLOR_BOLD = "\033[1m";
const char *COLOR_RED = "\033[31m";
const char *COLOR_GREEN = "\033[32m";
const char *COLOR_YELLOW = "\033[33m";
const char *COLOR_BLUE = "\033[34m";
const char *COLOR_MAGENTA = "\033[35m";
const char *COLOR_CYAN = "\033[36m";
const char *COLOR_WHITE = "\033[37m";
const char *COLOR_BG_BLUE = "\033[44m";
const char *COLOR_BG_GREEN = "\033[42m";
const char *COLOR_BG_RED = "\033[41m";

// ==================== String Utilities ====================

char* trim_whitespace(char* str) {
    if (str == NULL) return NULL;
    
    // Trim leading space
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    // Trim trailing space
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    // Write new null terminator
    *(end + 1) = 0;
    
    return str;
}

char* to_lower_case(char* str) {
    if (str == NULL) return NULL;
    
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
    
    return str;
}

char* to_upper_case(char* str) {
    if (str == NULL) return NULL;
    
    for (int i = 0; str[i]; i++) {
        str[i] = toupper(str[i]);
    }
    
    return str;
}

int string_starts_with(const char* str, const char* prefix) {
    if (str == NULL || prefix == NULL) return 0;
    
    size_t str_len = strlen(str);
    size_t prefix_len = strlen(prefix);
    
    if (prefix_len > str_len) return 0;
    
    return strncmp(str, prefix, prefix_len) == 0;
}

int string_ends_with(const char* str, const char* suffix) {
    if (str == NULL || suffix == NULL) return 0;
    
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    
    if (suffix_len > str_len) return 0;
    
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

char* string_replace(const char* str, const char* old, const char* new) {
    if (str == NULL || old == NULL || new == NULL) return NULL;
    
    char* result;
    int i, count = 0;
    size_t new_len = strlen(new);
    size_t old_len = strlen(old);
    
    // Count occurrences
    for (i = 0; str[i] != '\0'; i++) {
        if (strstr(&str[i], old) == &str[i]) {
            count++;
            i += old_len - 1;
        }
    }
    
    // Allocate memory
    result = (char*)malloc(i + count * (new_len - old_len) + 1);
    if (result == NULL) return NULL;
    
    i = 0;
    while (*str) {
        if (strstr(str, old) == str) {
            strcpy(&result[i], new);
            i += new_len;
            str += old_len;
        } else {
            result[i++] = *str++;
        }
    }
    
    result[i] = '\0';
    return result;
}

char** string_split(const char* str, const char* delimiter, int* count) {
    if (str == NULL || delimiter == NULL || count == NULL) return NULL;
    
    char* str_copy = strdup(str);
    if (str_copy == NULL) return NULL;
    
    // Count tokens
    *count = 0;
    char* token = strtok(str_copy, delimiter);
    while (token != NULL) {
        (*count)++;
        token = strtok(NULL, delimiter);
    }
    
    free(str_copy);
    
    if (*count == 0) return NULL;
    
    // Allocate array
    char** tokens = (char**)malloc((*count + 1) * sizeof(char*));
    if (tokens == NULL) return NULL;
    
    // Split string
    str_copy = strdup(str);
    token = strtok(str_copy, delimiter);
    
    for (int i = 0; i < *count; i++) {
        tokens[i] = strdup(trim_whitespace(token));
        token = strtok(NULL, delimiter);
    }
    
    tokens[*count] = NULL; // Null-terminate array
    free(str_copy);
    
    return tokens;
}

void free_string_array(char** array, int count) {
    if (array == NULL) return;
    
    for (int i = 0; i < count; i++) {
        if (array[i] != NULL) {
            free(array[i]);
        }
    }
    
    free(array);
}

char* join_strings(char** strings, int count, const char* delimiter) {
    if (strings == NULL || count <= 0) return strdup("");
    
    // Calculate total length
    size_t total_len = 0;
    for (int i = 0; i < count; i++) {
        total_len += strlen(strings[i]) + strlen(delimiter);
    }
    
    // Allocate memory
    char* result = (char*)malloc(total_len + 1);
    if (result == NULL) return NULL;
    
    result[0] = '\0';
    
    // Join strings
    for (int i = 0; i < count; i++) {
        strcat(result, strings[i]);
        if (i < count - 1) {
            strcat(result, delimiter);
        }
    }
    
    return result;
}

int string_contains(const char* str, const char* substring) {
    if (str == NULL || substring == NULL) return 0;
    return strstr(str, substring) != NULL;
}

// ==================== File System Utilities ====================

int file_exists(const char* filename) {
    if (filename == NULL) return 0;
    
    FILE* file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

int directory_exists(const char* dirname) {
    if (dirname == NULL) return 0;
    
    DIR* dir = opendir(dirname);
    if (dir) {
        closedir(dir);
        return 1;
    }
    return 0;
}

int create_directory(const char* dirname) {
    if (dirname == NULL) return 0;
    
    #ifdef _WIN32
        return _mkdir(dirname) == 0;
    #else
        return mkdir(dirname, 0755) == 0;
    #endif
}

long get_file_size(const char* filename) {
    if (filename == NULL) return -1;
    
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

time_t get_file_modified_time(const char* filename) {
    if (filename == NULL) return 0;
    
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

char* get_file_extension(const char* filename) {
    if (filename == NULL) return NULL;
    
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) return NULL;
    
    return strdup(dot + 1);
}

char* get_filename_without_extension(const char* filename) {
    if (filename == NULL) return NULL;
    
    const char* dot = strrchr(filename, '.');
    const char* slash = strrchr(filename, '/');
    if (slash == NULL) {
        slash = strrchr(filename, '\\');
    }
    
    if (dot == NULL) return strdup(filename);
    
    size_t len;
    if (slash != NULL) {
        len = dot - slash - 1;
        char* result = (char*)malloc(len + 1);
        if (result == NULL) return NULL;
        strncpy(result, slash + 1, len);
        result[len] = '\0';
        return result;
    } else {
        len = dot - filename;
        char* result = (char*)malloc(len + 1);
        if (result == NULL) return NULL;
        strncpy(result, filename, len);
        result[len] = '\0';
        return result;
    }
}

int copy_file(const char* source, const char* destination) {
    if (source == NULL || destination == NULL) return 0;
    
    FILE* src = fopen(source, "rb");
    if (src == NULL) return 0;
    
    FILE* dst = fopen(destination, "wb");
    if (dst == NULL) {
        fclose(src);
        return 0;
    }
    
    char buffer[4096];
    size_t bytes;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }
    
    fclose(src);
    fclose(dst);
    return 1;
}

int delete_file(const char* filename) {
    if (filename == NULL) return 0;
    
    return remove(filename) == 0;
}

// ==================== Memory Management Utilities ====================

void* safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "Memory allocation failed for %zu bytes\n", size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void* safe_calloc(size_t num, size_t size) {
    void* ptr = calloc(num, size);
    if (ptr == NULL) {
        fprintf(stderr, "Memory allocation failed for %zu elements of size %zu\n", num, size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void* safe_realloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (new_ptr == NULL && size > 0) {
        fprintf(stderr, "Memory reallocation failed for %zu bytes\n", size);
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

void safe_free(void** ptr) {
    if (ptr != NULL && *ptr != NULL) {
        free(*ptr);
        *ptr = NULL;
    }
}

// ==================== Date/Time Utilities ====================

char* get_current_timestamp() {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    char* timestamp = (char*)malloc(20);
    if (timestamp == NULL) return NULL;
    
    strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", tm_info);
    return timestamp;
}

char* get_current_date() {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    char* date = (char*)malloc(11);
    if (date == NULL) return NULL;
    
    strftime(date, 11, "%Y-%m-%d", tm_info);
    return date;
}

char* format_timestamp(time_t timestamp) {
    struct tm* tm_info = localtime(&timestamp);
    
    char* formatted = (char*)malloc(20);
    if (formatted == NULL) return NULL;
    
    strftime(formatted, 20, "%Y-%m-%d %H:%M:%S", tm_info);
    return formatted;
}

time_t parse_timestamp(const char* timestamp_str) {
    if (timestamp_str == NULL) return 0;
    
    struct tm tm = {0};
    if (strptime(timestamp_str, "%Y-%m-%d %H:%M:%S", &tm) != NULL) {
        return mktime(&tm);
    }
    
    return 0;
}

// ==================== Input/Output Utilities ====================

void clear_screen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

void print_header(const char* title) {
    printf("\n%s%s", COLOR_BG_BLUE, COLOR_WHITE);
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                     %-40s ║\n", title);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("%s", COLOR_RESET);
}

void print_success(const char* message) {
    printf("%s✓ %s%s\n", COLOR_GREEN, message, COLOR_RESET);
}

void print_error(const char* message) {
    printf("%s✗ %s%s\n", COLOR_RED, message, COLOR_RESET);
}

void print_warning(const char* message) {
    printf("%s⚠ %s%s\n", COLOR_YELLOW, message, COLOR_RESET);
}

void print_info(const char* message) {
    printf("%sℹ %s%s\n", COLOR_CYAN, message, COLOR_RESET);
}

void print_table_header(const char** headers, int num_columns, const int* column_widths) {
    printf("%s", COLOR_BOLD);
    
    // Print top border
    printf("┌");
    for (int i = 0; i < num_columns; i++) {
        for (int j = 0; j < column_widths[i] + 2; j++) {
            printf("─");
        }
        if (i < num_columns - 1) printf("┬");
    }
    printf("┐\n");
    
    // Print headers
    printf("│");
    for (int i = 0; i < num_columns; i++) {
        printf(" %-*s │", column_widths[i], headers[i]);
    }
    printf("\n");
    
    // Print separator
    printf("├");
    for (int i = 0; i < num_columns; i++) {
        for (int j = 0; j < column_widths[i] + 2; j++) {
            printf("─");
        }
        if (i < num_columns - 1) printf("┼");
    }
    printf("┤\n");
    
    printf("%s", COLOR_RESET);
}

void print_table_row(const char** values, int num_columns, const int* column_widths) {
    printf("│");
    for (int i = 0; i < num_columns; i++) {
        if (values[i] != NULL) {
            printf(" %-*s │", column_widths[i], values[i]);
        } else {
            printf(" %-*s │", column_widths[i], "NULL");
        }
    }
    printf("\n");
}

void print_table_footer(int num_columns, const int* column_widths) {
    printf("└");
    for (int i = 0; i < num_columns; i++) {
        for (int j = 0; j < column_widths[i] + 2; j++) {
            printf("─");
        }
        if (i < num_columns - 1) printf("┴");
    }
    printf("┘\n");
}

void print_progress_bar(int percentage, int width) {
    int filled = (percentage * width) / 100;
    
    printf("%s[", COLOR_BLUE);
    for (int i = 0; i < width; i++) {
        if (i < filled) {
            printf("█");
        } else {
            printf("░");
        }
    }
    printf("] %d%%%s\n", percentage, COLOR_RESET);
}

// ==================== Data Type Utilities ====================

int is_integer(const char* str) {
    if (str == NULL || *str == '\0') return 0;
    
    char* endptr;
    strtol(str, &endptr, 10);
    
    return *endptr == '\0';
}

int is_float(const char* str) {
    if (str == NULL || *str == '\0') return 0;
    
    char* endptr;
    strtod(str, &endptr);
    
    return *endptr == '\0';
}

int is_boolean(const char* str) {
    if (str == NULL) return 0;
    
    char* lower = strdup(str);
    to_lower_case(lower);
    
    int result = (strcmp(lower, "true") == 0 || 
                  strcmp(lower, "false") == 0 ||
                  strcmp(lower, "1") == 0 || 
                  strcmp(lower, "0") == 0 ||
                  strcmp(lower, "yes") == 0 || 
                  strcmp(lower, "no") == 0);
    
    free(lower);
    return result;
}

int is_date(const char* str) {
    if (str == NULL || strlen(str) != 10) return 0;
    
    // Check format: YYYY-MM-DD
    if (str[4] != '-' || str[7] != '-') return 0;
    
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (!isdigit(str[i])) return 0;
    }
    
    return 1;
}

int string_to_int(const char* str, int* result) {
    if (str == NULL || result == NULL) return 0;
    
    char* endptr;
    long value = strtol(str, &endptr, 10);
    
    if (*endptr != '\0') return 0;
    
    *result = (int)value;
    return 1;
}

int string_to_double(const char* str, double* result) {
    if (str == NULL || result == NULL) return 0;
    
    char* endptr;
    double value = strtod(str, &endptr);
    
    if (*endptr != '\0') return 0;
    
    *result = value;
    return 1;
}

int string_to_bool(const char* str) {
    if (str == NULL) return 0;
    
    char* lower = strdup(str);
    to_lower_case(lower);
    
    int result;
    if (strcmp(lower, "true") == 0 || strcmp(lower, "1") == 0 || strcmp(lower, "yes") == 0) {
        result = 1;
    } else {
        result = 0;
    }
    
    free(lower);
    return result;
}

// ==================== Validation Utilities ====================

int validate_table_name(const char* name) {
    if (name == NULL || strlen(name) == 0) return 0;
    
    // Check length
    if (strlen(name) > MAX_TABLE_NAME_LEN) return 0;
    
    // Check first character
    if (!isalpha(name[0]) && name[0] != '_') return 0;
    
    // Check remaining characters
    for (size_t i = 1; i < strlen(name); i++) {
        if (!isalnum(name[i]) && name[i] != '_') {
            return 0;
        }
    }
    
    // Check reserved keywords
    const char* reserved_keywords[] = {
        "SELECT", "INSERT", "UPDATE", "DELETE", "CREATE", "DROP", 
        "ALTER", "TABLE", "INDEX", "DATABASE", "FROM", "WHERE", 
        "AND", "OR", "NOT", "NULL", "TRUE", "FALSE", "INT", 
        "FLOAT", "STRING", "BOOL", "DATE", "PRIMARY", "KEY", 
        "FOREIGN", "REFERENCES", "UNIQUE", "CHECK", "DEFAULT"
    };
    
    int num_keywords = sizeof(reserved_keywords) / sizeof(reserved_keywords[0]);
    char* upper_name = strdup(name);
    to_upper_case(upper_name);
    
    for (int i = 0; i < num_keywords; i++) {
        if (strcmp(upper_name, reserved_keywords[i]) == 0) {
            free(upper_name);
            return 0;
        }
    }
    
    free(upper_name);
    return 1;
}

int validate_column_name(const char* name) {
    return validate_table_name(name); // Same rules for now
}

int validate_database_name(const char* name) {
    return validate_table_name(name); // Same rules for now
}

int validate_sql_identifier(const char* identifier) {
    return validate_table_name(identifier);
}

// ==================== Hash and Comparison Utilities ====================

unsigned int hash_string(const char* str) {
    if (str == NULL) return 0;
    
    unsigned int hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    return hash;
}

int compare_strings(const void* a, const void* b) {
    if (a == NULL && b == NULL) return 0;
    if (a == NULL) return -1;
    if (b == NULL) return 1;
    
    return strcmp((const char*)a, (const char*)b);
}

int compare_ints(const void* a, const void* b) {
    int int_a = *(const int*)a;
    int int_b = *(const int*)b;
    
    if (int_a < int_b) return -1;
    if (int_a > int_b) return 1;
    return 0;
}

int compare_doubles(const void* a, const void* b) {
    double double_a = *(const double*)a;
    double double_b = *(const double*)b;
    
    if (double_a < double_b) return -1;
    if (double_a > double_b) return 1;
    return 0;
}

// ==================== Configuration Utilities ====================

Config* load_config(const char* filename) {
    if (filename == NULL || !file_exists(filename)) {
        return create_default_config();
    }
    
    Config* config = (Config*)malloc(sizeof(Config));
    if (config == NULL) return NULL;
    
    // Initialize with defaults
    strcpy(config->default_database_path, "./databases");
    config->auto_save = 1;
    config->auto_backup = 1;
    config->max_history_size = 100;
    config->enable_colors = 1;
    config->log_level = LOG_LEVEL_INFO;
    config->max_cache_size = 1000;
    
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        free(config);
        return create_default_config();
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        char* trimmed = trim_whitespace(line);
        
        if (strlen(trimmed) == 0 || trimmed[0] == '#') {
            continue;
        }
        
        char key[64], value[128];
        if (sscanf(trimmed, "%63[^=]=%127[^\n]", key, value) == 2) {
            char* trimmed_key = trim_whitespace(key);
            char* trimmed_value = trim_whitespace(value);
            
            if (strcmp(trimmed_key, "default_database_path") == 0) {
                strncpy(config->default_database_path, trimmed_value, sizeof(config->default_database_path) - 1);
            } else if (strcmp(trimmed_key, "auto_save") == 0) {
                config->auto_save = atoi(trimmed_value);
            } else if (strcmp(trimmed_key, "auto_backup") == 0) {
                config->auto_backup = atoi(trimmed_value);
            } else if (strcmp(trimmed_key, "max_history_size") == 0) {
                config->max_history_size = atoi(trimmed_value);
            } else if (strcmp(trimmed_key, "enable_colors") == 0) {
                config->enable_colors = atoi(trimmed_value);
            } else if (strcmp(trimmed_key, "log_level") == 0) {
                config->log_level = atoi(trimmed_value);
            } else if (strcmp(trimmed_key, "max_cache_size") == 0) {
                config->max_cache_size = atoi(trimmed_value);
            }
        }
    }
    
    fclose(file);
    return config;
}

Config* create_default_config() {
    Config* config = (Config*)malloc(sizeof(Config));
    if (config == NULL) return NULL;
    
    strcpy(config->default_database_path, "./databases");
    config->auto_save = 1;
    config->auto_backup = 1;
    config->max_history_size = 100;
    config->enable_colors = 1;
    config->log_level = LOG_LEVEL_INFO;
    config->max_cache_size = 1000;
    
    return config;
}

int save_config(const char* filename, Config* config) {
    if (filename == NULL || config == NULL) return 0;
    
    FILE* file = fopen(filename, "w");
    if (file == NULL) return 0;
    
    fprintf(file, "# Database Configuration File\n\n");
    fprintf(file, "default_database_path = %s\n", config->default_database_path);
    fprintf(file, "auto_save = %d\n", config->auto_save);
    fprintf(file, "auto_backup = %d\n", config->auto_backup);
    fprintf(file, "max_history_size = %d\n", config->max_history_size);
    fprintf(file, "enable_colors = %d\n", config->enable_colors);
    fprintf(file, "log_level = %d\n", config->log_level);
    fprintf(file, "max_cache_size = %d\n", config->max_cache_size);
    
    fclose(file);
    return 1;
}

void free_config(Config* config) {
    if (config != NULL) {
        free(config);
    }
}

// ==================== Logging Utilities ====================

Logger* create_logger(const char* filename, int level) {
    Logger* logger = (Logger*)malloc(sizeof(Logger));
    if (logger == NULL) return NULL;
    
    logger->level = level;
    logger->file = fopen(filename, "a");
    if (logger->file == NULL) {
        free(logger);
        return NULL;
    }
    
    return logger;
}

void log_message(Logger* logger, int level, const char* message, ...) {
    if (logger == NULL || level < logger->level) return;
    
    char* level_str;
    const char* color_code;
    
    switch (level) {
        case LOG_LEVEL_DEBUG:
            level_str = "DEBUG";
            color_code = COLOR_BLUE;
            break;
        case LOG_LEVEL_INFO:
            level_str = "INFO";
            color_code = COLOR_GREEN;
            break;
        case LOG_LEVEL_WARNING:
            level_str = "WARNING";
            color_code = COLOR_YELLOW;
            break;
        case LOG_LEVEL_ERROR:
            level_str = "ERROR";
            color_code = COLOR_RED;
            break;
        default:
            level_str = "UNKNOWN";
            color_code = COLOR_RESET;
    }
    
    char* timestamp = get_current_timestamp();
    
    va_list args;
    va_start(args, message);
    
    // Log to file
    fprintf(logger->file, "[%s] [%s] ", timestamp, level_str);
    vfprintf(logger->file, message, args);
    fprintf(logger->file, "\n");
    fflush(logger->file);
    
    // Log to console if colors are enabled
    if (level >= LOG_LEVEL_INFO) {
        printf("%s[%s] [%s] ", color_code, timestamp, level_str);
        vprintf(message, args);
        printf("%s\n", COLOR_RESET);
    }
    
    va_end(args);
    free(timestamp);
}

void close_logger(Logger* logger) {
    if (logger != NULL) {
        if (logger->file != NULL) {
            fclose(logger->file);
        }
        free(logger);
    }
}

// ==================== Backup Utilities ====================

int create_backup(const char* source_dir, const char* backup_dir) {
    if (source_dir == NULL || backup_dir == NULL) return 0;
    
    // Create backup directory if it doesn't exist
    if (!directory_exists(backup_dir)) {
        if (!create_directory(backup_dir)) {
            return 0;
        }
    }
    
    // Create timestamp for backup
    char* timestamp = get_current_timestamp();
    char* safe_timestamp = string_replace(timestamp, ":", "-");
    char backup_path[512];
    snprintf(backup_path, sizeof(backup_path), "%s/backup_%s", backup_dir, safe_timestamp);
    
    // Copy directory recursively
    if (!copy_directory(source_dir, backup_path)) {
        free(safe_timestamp);
        free(timestamp);
        return 0;
    }
    
    free(safe_timestamp);
    free(timestamp);
    
    // Compress backup
    char command[1024];
    snprintf(command, sizeof(command), "tar -czf %s.tar.gz %s", backup_path, backup_path);
    
    int result = system(command);
    
    // Remove uncompressed backup
    char rm_command[512];
    snprintf(rm_command, sizeof(rm_command), "rm -rf %s", backup_path);
    system(rm_command);
    
    return result == 0;
}

int copy_directory(const char* source, const char* destination) {
    if (source == NULL || destination == NULL) return 0;
    
    // Create destination directory
    if (!create_directory(destination)) {
        return 0;
    }
    
    DIR* dir = opendir(source);
    if (dir == NULL) return 0;
    
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char source_path[512];
        char dest_path[512];
        
        snprintf(source_path, sizeof(source_path), "%s/%s", source, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", destination, entry->d_name);
        
        struct stat stat_buf;
        stat(source_path, &stat_buf);
        
        if (S_ISDIR(stat_buf.st_mode)) {
            // Recursively copy directory
            if (!copy_directory(source_path, dest_path)) {
                closedir(dir);
                return 0;
            }
        } else {
            // Copy file
            if (!copy_file(source_path, dest_path)) {
                closedir(dir);
                return 0;
            }
        }
    }
    
    closedir(dir);
    return 1;
}

// ==================== Miscellaneous Utilities ====================

void sleep_ms(int milliseconds) {
    #ifdef _WIN32
        Sleep(milliseconds);
    #else
        usleep(milliseconds * 1000);
    #endif
}

int get_terminal_width() {
    #ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    #else
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        return w.ws_col;
    #endif
}

int get_terminal_height() {
    #ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    #else
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        return w.ws_row;
    #endif
}

char* read_line(FILE* stream) {
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    
    read = getline(&line, &len, stream);
    
    if (read == -1) {
        free(line);
        return NULL;
    }
    
    // Remove newline
    line[strcspn(line, "\n")] = 0;
    
    return line;
}

char* read_multiline_input() {
    printf("Enter SQL command (end with ';' on a line by itself):\n");
    printf("%sSQL> %s", COLOR_GREEN, COLOR_RESET);
    
    char* input = NULL;
    char* line = NULL;
    size_t total_len = 0;
    
    while ((line = read_line(stdin)) != NULL) {
        // Check for termination
        if (strcmp(trim_whitespace(line), ";") == 0) {
            free(line);
            break;
        }
        
        // Append line
        size_t line_len = strlen(line);
        char* new_input = (char*)realloc(input, total_len + line_len + 2);
        if (new_input == NULL) {
            free(input);
            free(line);
            return NULL;
        }
        
        input = new_input;
        
        if (total_len > 0) {
            input[total_len] = ' ';
            total_len++;
        }
        
        strcpy(input + total_len, line);
        total_len += line_len;
        
        free(line);
        printf("%s... %s", COLOR_GREEN, COLOR_RESET);
    }
    
    return input;
}

// ==================== Data Structure Utilities ====================

DynamicArray* create_dynamic_array(size_t element_size) {
    DynamicArray* array = (DynamicArray*)malloc(sizeof(DynamicArray));
    if (array == NULL) return NULL;
    
    array->data = malloc(INITIAL_CAPACITY * element_size);
    if (array->data == NULL) {
        free(array);
        return NULL;
    }
    
    array->element_size = element_size;
    array->size = 0;
    array->capacity = INITIAL_CAPACITY;
    
    return array;
}

void dynamic_array_push(DynamicArray* array, void* element) {
    if (array == NULL || element == NULL) return;
    
    if (array->size >= array->capacity) {
        array->capacity *= 2;
        array->data = realloc(array->data, array->capacity * array->element_size);
    }
    
    memcpy((char*)array->data + array->size * array->element_size, element, array->element_size);
    array->size++;
}

void* dynamic_array_get(DynamicArray* array, size_t index) {
    if (array == NULL || index >= array->size) return NULL;
    
    return (char*)array->data + index * array->element_size;
}

void free_dynamic_array(DynamicArray* array) {
    if (array != NULL) {
        if (array->data != NULL) {
            free(array->data);
        }
        free(array);
    }
}

// ==================== Serialization Utilities ====================

char* serialize_int(int value) {
    char* buffer = (char*)malloc(16);
    if (buffer == NULL) return NULL;
    
    snprintf(buffer, 16, "%d", value);
    return buffer;
}

char* serialize_double(double value) {
    char* buffer = (char*)malloc(32);
    if (buffer == NULL) return NULL;
    
    snprintf(buffer, 32, "%.10f", value);
    return buffer;
}

char* serialize_bool(int value) {
    return strdup(value ? "true" : "false");
}

int deserialize_int(const char* str) {
    if (str == NULL) return 0;
    return atoi(str);
}

double deserialize_double(const char* str) {
    if (str == NULL) return 0.0;
    return atof(str);
}

int deserialize_bool(const char* str) {
    if (str == NULL) return 0;
    
    if (strcmp(str, "true") == 0 || strcmp(str, "1") == 0) {
        return 1;
    }
    return 0;
}

// ==================== Performance Utilities ====================

PerformanceTimer* start_timer() {
    PerformanceTimer* timer = (PerformanceTimer*)malloc(sizeof(PerformanceTimer));
    if (timer == NULL) return NULL;
    
    #ifdef _WIN32
        QueryPerformanceCounter(&timer->start);
    #else
        clock_gettime(CLOCK_MONOTONIC, &timer->start);
    #endif
    
    return timer;
}

double stop_timer(PerformanceTimer* timer) {
    if (timer == NULL) return 0.0;
    
    #ifdef _WIN32
        LARGE_INTEGER end, frequency;
        QueryPerformanceCounter(&end);
        QueryPerformanceFrequency(&frequency);
        
        double elapsed = (double)(end.QuadPart - timer->start.QuadPart) / frequency.QuadPart;
    #else
        struct timespec end;
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        double elapsed = (end.tv_sec - timer->start.tv_sec) + 
                        (end.tv_nsec - timer->start.tv_nsec) / 1e9;
    #endif
    
    free(timer);
    return elapsed;
}

void print_performance_stats(const char* operation, double time_seconds, long bytes_processed) {
    printf("%sPerformance Stats:%s\n", COLOR_CYAN, COLOR_RESET);
    printf("  Operation: %s\n", operation);
    printf("  Time: %.6f seconds\n", time_seconds);
    
    if (bytes_processed > 0) {
        double mb_processed = bytes_processed / (1024.0 * 1024.0);
        printf("  Data: %.2f MB\n", mb_processed);
        printf("  Throughput: %.2f MB/s\n", mb_processed / time_seconds);
    }
    
    printf("\n");
}