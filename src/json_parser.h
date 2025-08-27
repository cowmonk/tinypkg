/*
 * TinyPkg - JSON Parser Header
 * Package definition parsing from JSON files
 */

#ifndef TINYPKG_JSON_PARSER_H
#define TINYPKG_JSON_PARSER_H

#include <jansson.h>

// JSON parsing functions
package_t *json_parser_load_package(const char *package_name);
package_t *json_parser_load_package_file(const char *json_file);
int json_parser_save_package(const package_t *pkg, const char *json_file);

// JSON validation
int json_parser_validate_package_file(const char *json_file);
int json_parser_validate_package_json(json_t *root);

// JSON utilities
json_t *json_parser_load_file(const char *filename);
int json_parser_save_file(json_t *root, const char *filename);
char *json_parser_get_string(json_t *obj, const char *key, const char *default_value);
int json_parser_get_int(json_t *obj, const char *key, int default_value);
json_t *json_parser_get_array(json_t *obj, const char *key);

// String array helpers
char **json_parser_array_to_strings(json_t *array, int *count);
json_t *json_parser_strings_to_array(char **strings, int count);
void json_parser_free_string_array(char **strings, int count);

#endif /* TINYPKG_JSON_PARSER_H */
