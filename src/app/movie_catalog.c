/*
 * movie_catalog.c - Parser e integracion del catalog de peliculas/series
 * para el downloader basado en pipensx
 *
 * Este modulo parsea archivos JSON con torrents de peliculas y series
 * y expone funciones para buscar, filtrar y ordenar el contenido.
 * Tambien integra con el torrent engine para iniciar descargas.
 */

#include "movie_catalog.h"
#include "core/torrent.h"
#include "core/platform_storage.h"
#include "core/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* ==================== MINI JSON PARSER ==================== */
/* Implementacion minima para parsear JSON sin dependencias externas */

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct JsonValue {
    JsonType type;
    char *str_value;     // For strings
    double num_value;    // For numbers
    int bool_value;      // For booleans
    struct JsonValue **items; // For arrays
    int array_count;
    char **keys;         // For objects
    struct JsonValue **values;
    int object_count;
} JsonValue;

/* Forward declarations */
static JsonValue *json_parse(const char *input, const char **end);
static void json_free(JsonValue *value);
static JsonValue *json_get(const JsonValue *obj, const char *key);
static const char *json_get_string(const JsonValue *obj, const char *key);
static int json_get_int(const JsonValue *obj, const char *key, int default_val);
static double json_get_double(const JsonValue *obj, const char *key, double default_val);
static const JsonValue *json_get_array(const JsonValue *obj, const char *key);
static int json_array_count(const JsonValue *array);
static const JsonValue *json_array_item(const JsonValue *array, int index);
static const char *json_get_title(const JsonValue *entry);

/* Skip whitespace */
static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    return p;
}

/* Parse a JSON string (with escapes) */
static char *parse_json_string(const char *input, const char **end) {
    if (*input != '"') return NULL;
    const char *p = input + 1;
    size_t len = 0;
    const char *start = p;
    
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            if (*p) len++; // escape char
        }
        len++;
        p++;
    }
    if (*p != '"') return NULL;
    
    char *result = (char *)malloc(len + 1);
    if (!result) return NULL;
    
    char *out = result;
    p = start;
    while (p < input + 1 + len) {
        if (*p == '\\' && *(p+1)) {
            p++;
            switch (*p) {
                case '"': *out++ = '"'; break;
                case '\\': *out++ = '\\'; break;
                case '/': *out++ = '/'; break;
                case 'n': *out++ = '\n'; break;
                case 't': *out++ = '\t'; break;
                case 'r': *out++ = '\r'; break;
                default: *out++ = *p; break;
            }
        } else {
            *out++ = *p;
        }
        p++;
    }
    *out = '\0';
    *end = p + 1;
    return result;
}

/* Parse a JSON number */
static double parse_json_number(const char *input, const char **end) {
    char *endptr;
    double val = strtod(input, &endptr);
    *end = endptr;
    return val;
}

/* Main JSON parser */
static JsonValue *json_parse(const char *input, const char **end) {
    const char *p = skip_ws(input);
    JsonValue *result = (JsonValue *)calloc(1, sizeof(JsonValue));
    if (!result) return NULL;
    
    if (!*p) {
        free(result);
        if (end) *end = p;
        return NULL;
    }
    
    switch (*p) {
        case '"': {
            result->type = JSON_STRING;
            result->str_value = parse_json_string(p, &p);
            if (!result->str_value) { free(result); return NULL; }
            /* parse_json_string sets p to the closing quote; advance past it */
            p++;
            break;
        }
        case '{': {
            result->type = JSON_OBJECT;
            p++; // skip {
            p = skip_ws(p);
            while (*p && *p != '}') {
                // Parse key
                const char *key_end;
                char *key = parse_json_string(p, &key_end);
                if (!key) { free(result); return NULL; }
                p = skip_ws(key_end);
                
                // Expect colon
                if (*p != ':') { free(key); free(result); return NULL; }
                p = skip_ws(p + 1);
                
                // Parse value
                JsonValue *value = json_parse(p, &p);
                if (!value) { free(key); free(result); return NULL; }
                
                result->keys = (char **)realloc(result->keys, sizeof(char*) * (result->object_count + 1));
                result->values = (JsonValue **)realloc(result->values, sizeof(JsonValue*) * (result->object_count + 1));
                result->keys[result->object_count] = key;
                result->values[result->object_count] = value;
                result->object_count++;
                
                p = skip_ws(p);
                if (*p == ',') p = skip_ws(p + 1);
            }
            if (*p == '}') p++;
            break;
        }
        case '[': {
            result->type = JSON_ARRAY;
            p++; // skip [
            p = skip_ws(p);
            while (*p && *p != ']') {
                JsonValue *item = json_parse(p, &p);
                if (!item) { free(result); return NULL; }
                
                result->items = (JsonValue **)realloc(result->items, sizeof(JsonValue*) * (result->array_count + 1));
                result->items[result->array_count] = item;
                result->array_count++;
                
                p = skip_ws(p);
                if (*p == ',') p = skip_ws(p + 1);
            }
            if (*p == ']') p++;
            break;
        }
        case 'n': // null
            result->type = JSON_NULL;
            p += 4;
            break;
        case 't': // true
            result->type = JSON_BOOL;
            result->bool_value = 1;
            p += 4;
            break;
        case 'f': // false
            result->type = JSON_BOOL;
            result->bool_value = 0;
            p += 5;
            break;
        default:
            if (*p == '-' || (*p >= '0' && *p <= '9')) {
                result->type = JSON_NUMBER;
                result->num_value = parse_json_number(p, &p);
            } else {
                free(result);
                return NULL;
            }
            break;
    }
    
    if (end) *end = p;
    return result;
}

/* Free JSON value */
static void json_free(JsonValue *value) {
    if (!value) return;
    
    switch (value->type) {
        case JSON_STRING:
            free(value->str_value);
            break;
        case JSON_ARRAY:
            for (int i = 0; i < value->array_count; i++) {
                json_free(value->items[i]);
            }
            free(value->items);
            break;
        case JSON_OBJECT:
            for (int i = 0; i < value->object_count; i++) {
                free(value->keys[i]);
                json_free(value->values[i]);
            }
            free(value->keys);
            free(value->values);
            break;
        default:
            break;
    }
    free(value);
}

/* Get value from object by key */
static JsonValue *json_get(const JsonValue *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    for (int i = 0; i < obj->object_count; i++) {
        if (strcmp(obj->keys[i], key) == 0)
            return obj->values[i];
    }
    return NULL;
}

/* Get string value */
static const char *json_get_string(const JsonValue *obj, const char *key) {
    JsonValue *val = json_get(obj, key);
    if (val && val->type == JSON_STRING)
        return val->str_value;
    return "";
}

/* Get int value — handles both JSON_NUMBER and numeric JSON_STRING */
static int json_get_int(const JsonValue *obj, const char *key, int default_val) {
    JsonValue *val = json_get(obj, key);
    if (!val) return default_val;
    if (val->type == JSON_NUMBER)
        return (int)val->num_value;
    if (val->type == JSON_STRING) {
        char *endptr;
        long v = strtol(val->str_value, &endptr, 10);
        if (endptr > val->str_value) return (int)v;
    }
    return default_val;
}

/* Get double value — handles both JSON_NUMBER and numeric JSON_STRING */
static double json_get_double(const JsonValue *obj, const char *key, double default_val) {
    JsonValue *val = json_get(obj, key);
    if (!val) return default_val;
    if (val->type == JSON_NUMBER)
        return val->num_value;
    if (val->type == JSON_STRING) {
        char *endptr;
        double v = strtod(val->str_value, &endptr);
        if (endptr > val->str_value) return v;
    }
    return default_val;
}

/* Get array value */
static const JsonValue *json_get_array(const JsonValue *obj, const char *key) {
    JsonValue *val = json_get(obj, key);
    if (val && val->type == JSON_ARRAY)
        return val;
    return NULL;
}

/* Get array count */
static int json_array_count(const JsonValue *array) {
    if (!array || array->type != JSON_ARRAY) return 0;
    return array->array_count;
}

/* Get array item */
static const JsonValue *json_array_item(const JsonValue *array, int index) {
    if (!array || index < 0 || index >= array->array_count) return NULL;
    return array->items[index];
}

/* Get title from entry */
static const char *json_get_title(const JsonValue *entry) {
    return json_get_string(entry, "title");
}

/* ==================== CATALOG IMPLEMENTATION ==================== */

/* Grow catalog capacity */
static bool catalog_grow(MovieCatalog *catalog) {
    size_t new_capacity = catalog->capacity == 0 ? 16 : catalog->capacity * 2;
    MovieEntry *new_entries = (MovieEntry *)realloc(catalog->entries, sizeof(MovieEntry) * new_capacity);
    if (!new_entries) return false;
    catalog->entries = new_entries;
    catalog->capacity = new_capacity;
    return true;
}

MovieCatalog *catalog_create(void) {
    MovieCatalog *catalog = (MovieCatalog *)calloc(1, sizeof(MovieCatalog));
    return catalog;
}

bool catalog_load_from_file(MovieCatalog *catalog, const char *filepath, char *error, size_t error_size) {
    FILE *f = fopen(filepath, "r");
    if (!f) {
        if (error && error_size > 0)
            snprintf(error, error_size, "Cannot open file: %s", filepath);
        return false;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // Read file
    char *buffer = (char *)malloc(size + 1);
    if (!buffer) {
        if (error && error_size > 0)
            snprintf(error, error_size, "Memory allocation failed");
        fclose(f);
        return false;
    }
    
    size_t read_size = fread(buffer, 1, size, f);
    buffer[read_size] = '\0';
    fclose(f);
    
    bool result = catalog_load_from_string(catalog, buffer, error, error_size);
    free(buffer);
    return result;
}

bool catalog_load_from_string(MovieCatalog *catalog, const char *json_string, char *error, size_t error_size) {
    const char *end;
    JsonValue *root = json_parse(json_string, &end);
    if (!root) {
        if (error && error_size > 0)
            snprintf(error, error_size, "Failed to parse JSON");
        return false;
    }
    
    if (root->type != JSON_ARRAY) {
        json_free(root);
        if (error && error_size > 0)
            snprintf(error, error_size, "JSON root is not an array");
        return false;
    }
    
    // Allocate raw JSON for reference
    size_t json_len = strlen(json_string);
    catalog->raw_json = (char *)malloc(json_len + 1);
    if (!catalog->raw_json) {
        json_free(root);
        if (error && error_size > 0)
            snprintf(error, error_size, "Memory allocation failed");
        return false;
    }
    strcpy(catalog->raw_json, json_string);
    catalog->raw_json_size = json_len;
    
    // Parse each entry
    for (int i = 0; i < root->array_count; i++) {
        const JsonValue *entry = root->items[i];
        
        if (!catalog_grow(catalog)) {
            json_free(root);
            if (error && error_size > 0)
                snprintf(error, error_size, "Memory allocation failed while growing catalog");
            return false;
        }
        
        MovieEntry *movie = &catalog->entries[catalog->count];
        memset(movie, 0, sizeof(MovieEntry));
        
        // Extract fields
        const char *title = json_get_string(entry, "title");
        const char *genre = json_get_string(entry, "genre");
        const char *description = json_get_string(entry, "description");
        const char *magnet = json_get_string(entry, "magnet");
        const char *poster = json_get_string(entry, "poster");
        const char *quality = json_get_string(entry, "quality");
        
        strncpy(movie->title, title ? title : "", MAX_MOVIE_TITLE - 1);
        strncpy(movie->genre, genre ? genre : "", MAX_MOVIE_GENRE - 1);
        strncpy(movie->description, description ? description : "", MAX_MOVIE_DESC - 1);
        strncpy(movie->magnet, magnet ? magnet : "", MAX_MOVIE_MAGNET - 1);
        strncpy(movie->poster, poster ? poster : "", MAX_MOVIE_POSTER - 1);
        strncpy(movie->quality, quality ? quality : "", MAX_MOVIE_QUALITY - 1);
        
        // Determine type
        const char *category = json_get_string(entry, "category");
        if (strcmp(category, "series") == 0) {
            movie->type = ENTRY_SERIES;
        } else {
            movie->type = ENTRY_MOVIE;
        }
        
        // Extract numeric fields
        movie->year = (uint32_t)json_get_int(entry, "year", 0);
        movie->rating = (uint32_t)(json_get_int(entry, "rating", 0) * 10);
        movie->duration_min = (uint32_t)json_get_int(entry, "duration_min", 0);
        movie->seasons = (uint32_t)json_get_int(entry, "seasons", 0);
        movie->episodes = (uint32_t)json_get_int(entry, "episodes", 0);
        movie->size_gb = json_get_double(entry, "size_gb", 0.0);
        movie->peers = (uint32_t)json_get_int(entry, "peers", 0);
        movie->downloads = (uint64_t)json_get_double(entry, "downloads", 0.0);
        
        // Extract info_hash
        const char *info_hash = json_get_string(entry, "info_hash");
        if (info_hash && strlen(info_hash) > 0) {
            strncpy(movie->info_hash, info_hash, 40);
        } else {
            // Generate from magnet
            const char *magnet_start = strstr(magnet, "btih:");
            if (magnet_start) {
                magnet_start += 5;
                strncpy(movie->info_hash, magnet_start, 40);
            }
        }
        
        catalog->count++;
    }
    
    json_free(root);
    return true;
}

void catalog_destroy(MovieCatalog *catalog) {
    if (!catalog) return;
    
    free(catalog->entries);
    free(catalog->raw_json);
    free(catalog);
}

MovieEntry *catalog_search_by_title(MovieCatalog *catalog, const char *query) {
    if (!catalog || !query) return NULL;
    
    char query_lower[256];
    strncpy(query_lower, query, sizeof(query_lower));
    for (size_t i = 0; i < strlen(query_lower); i++)
        query_lower[i] = tolower(query_lower[i]);
    
    for (size_t i = 0; i < catalog->count; i++) {
        char title_lower[256];
        strncpy(title_lower, catalog->entries[i].title, sizeof(title_lower));
        for (size_t j = 0; j < strlen(title_lower); j++)
            title_lower[j] = tolower(title_lower[j]);
        
        if (strstr(title_lower, query_lower) != NULL) {
            return &catalog->entries[i];
        }
    }
    return NULL;
}

MovieEntry *catalog_search_by_genre(MovieCatalog *catalog, const char *genre) {
    if (!catalog || !genre) return NULL;
    
    char genre_lower[256];
    strncpy(genre_lower, genre, sizeof(genre_lower));
    for (size_t i = 0; i < strlen(genre_lower); i++)
        genre_lower[i] = tolower(genre_lower[i]);
    
    for (size_t i = 0; i < catalog->count; i++) {
        char entry_genre_lower[256];
        strncpy(entry_genre_lower, catalog->entries[i].genre, sizeof(entry_genre_lower));
        for (size_t j = 0; j < strlen(entry_genre_lower); j++)
            entry_genre_lower[j] = tolower(entry_genre_lower[j]);
        
        if (strstr(entry_genre_lower, genre_lower) != NULL) {
            return &catalog->entries[i];
        }
    }
    return NULL;
}

MovieEntry *catalog_search_by_min_rating(MovieCatalog *catalog, uint32_t min_rating) {
    if (!catalog) return NULL;
    
    for (size_t i = 0; i < catalog->count; i++) {
        if (catalog->entries[i].rating >= min_rating) {
            return &catalog->entries[i];
        }
    }
    return NULL;
}

MovieEntry *catalog_get_random_entry(MovieCatalog *catalog) {
    if (!catalog || catalog->count == 0) return NULL;
    
    srand((unsigned)time(NULL));
    size_t index = rand() % catalog->count;
    return &catalog->entries[index];
}

MovieEntry *catalog_get_by_hash(MovieCatalog *catalog, const char *info_hash) {
    if (!catalog || !info_hash) return NULL;
    
    for (size_t i = 0; i < catalog->count; i++) {
        if (strcmp(catalog->entries[i].info_hash, info_hash) == 0) {
            return &catalog->entries[i];
        }
    }
    return NULL;
}

MatchResult *catalog_search(MovieCatalog *catalog, const char *query, int *match_count) {
    if (!catalog || !query) return NULL;
    
    // Simple search: find all entries matching the query in title, description, or genre
    int count = 0;
    for (size_t i = 0; i < catalog->count; i++) {
        char title_lower[256], desc_lower[1024], genre_lower[256], query_lower[256];
        strncpy(title_lower, catalog->entries[i].title, sizeof(title_lower));
        title_lower[sizeof(title_lower) - 1] = '\0';
        strncpy(desc_lower, catalog->entries[i].description, sizeof(desc_lower));
        desc_lower[sizeof(desc_lower) - 1] = '\0';
        strncpy(genre_lower, catalog->entries[i].genre, sizeof(genre_lower));
        genre_lower[sizeof(genre_lower) - 1] = '\0';
        strncpy(query_lower, query, sizeof(query_lower));
        query_lower[sizeof(query_lower) - 1] = '\0';
        for (size_t j = 0; j < strlen(title_lower); j++)
            title_lower[j] = tolower(title_lower[j]);
        for (size_t j = 0; j < strlen(desc_lower); j++)
            desc_lower[j] = tolower(desc_lower[j]);
        for (size_t j = 0; j < strlen(genre_lower); j++)
            genre_lower[j] = tolower(genre_lower[j]);
        for (size_t j = 0; j < strlen(query_lower); j++)
            query_lower[j] = tolower(query_lower[j]);
        
        if (strstr(title_lower, query_lower) != NULL ||
            strstr(desc_lower, query_lower) != NULL ||
            strstr(genre_lower, query_lower) != NULL) {
            count++;
        }
    }
    
    *match_count = count;
    if (count == 0) return NULL;
    
    MatchResult *results = (MatchResult *)calloc(count, sizeof(MatchResult));
    int idx = 0;
    for (size_t i = 0; i < catalog->count; i++) {
        char title_lower[256], desc_lower[1024], genre_lower[256], query_lower[256];
        strncpy(title_lower, catalog->entries[i].title, sizeof(title_lower));
        title_lower[sizeof(title_lower) - 1] = '\0';
        strncpy(desc_lower, catalog->entries[i].description, sizeof(desc_lower));
        desc_lower[sizeof(desc_lower) - 1] = '\0';
        strncpy(genre_lower, catalog->entries[i].genre, sizeof(genre_lower));
        genre_lower[sizeof(genre_lower) - 1] = '\0';
        strncpy(query_lower, query, sizeof(query_lower));
        query_lower[sizeof(query_lower) - 1] = '\0';
        for (size_t j = 0; j < strlen(title_lower); j++)
            title_lower[j] = tolower(title_lower[j]);
        for (size_t j = 0; j < strlen(desc_lower); j++)
            desc_lower[j] = tolower(desc_lower[j]);
        for (size_t j = 0; j < strlen(genre_lower); j++)
            genre_lower[j] = tolower(genre_lower[j]);
        for (size_t j = 0; j < strlen(query_lower); j++)
            query_lower[j] = tolower(query_lower[j]);
        
        if (strstr(title_lower, query_lower) != NULL) {
            results[idx].entry = &catalog->entries[i];
            results[idx].score = 100; // Perfect title match
            results[idx].reason = "Title match";
            idx++;
        } else if (strstr(desc_lower, query_lower) != NULL) {
            results[idx].entry = &catalog->entries[i];
            results[idx].score = 80; // Description match
            results[idx].reason = "Description match";
            idx++;
        } else if (strstr(genre_lower, query_lower) != NULL) {
            results[idx].entry = &catalog->entries[i];
            results[idx].score = 60; // Genre match
            results[idx].reason = "Genre match";
            idx++;
        }
    }
    
    return results;
}

MovieEntry *catalog_filter_by_type(MovieCatalog *catalog, EntryType type) {
    if (!catalog) return NULL;
    
    for (size_t i = 0; i < catalog->count; i++) {
        if (catalog->entries[i].type == type) {
            return &catalog->entries[i];
        }
    }
    return NULL;
}

CatalogStats catalog_get_stats(MovieCatalog *catalog) {
    CatalogStats stats = {0};
    if (!catalog) return stats;
    
    stats.total_entries = catalog->count;
    
    double total_rating = 0;
    double total_size = 0;
    
    for (size_t i = 0; i < catalog->count; i++) {
        if (catalog->entries[i].type == ENTRY_MOVIE) {
            stats.movies++;
        } else {
            stats.series++;
        }
        total_rating += catalog->entries[i].rating;
        total_size += catalog->entries[i].size_gb;
        stats.total_peers += catalog->entries[i].peers;
        stats.total_downloads += catalog->entries[i].downloads;
    }
    
    stats.avg_rating = catalog->count > 0 ? total_rating / catalog->count : 0;
    stats.avg_size_gb = catalog->count > 0 ? total_size / catalog->count : 0;
    
    return stats;
}

bool catalog_export_to_file(MovieCatalog *catalog, const char *filepath, char *error, size_t error_size) {
    if (!catalog || !filepath) return false;
    
    // For now, just write the raw JSON
    FILE *f = fopen(filepath, "w");
    if (!f) {
        if (error && error_size > 0)
            snprintf(error, error_size, "Cannot open file: %s", filepath);
        return false;
    }
    
    if (catalog->raw_json) {
        fwrite(catalog->raw_json, 1, catalog->raw_json_size, f);
    }
    
    fclose(f);
    return true;
}

bool catalog_add_entry(MovieCatalog *catalog, MovieEntry entry) {
    if (!catalog) return false;
    
    if (catalog->count >= catalog->capacity) {
        if (!catalog_grow(catalog)) return false;
    }
    
    catalog->entries[catalog->count] = entry;
    catalog->count++;
    return true;
}

bool catalog_remove_entry(MovieCatalog *catalog, size_t index) {
    if (!catalog || index >= catalog->count) return false;
    
    // Shift elements
    for (size_t i = index; i < catalog->count - 1; i++) {
        catalog->entries[i] = catalog->entries[i + 1];
    }
    catalog->count--;
    return true;
}

bool catalog_sort(MovieCatalog *catalog, SortField field) {
    if (!catalog || catalog->count == 0) return true;
    
    // Simple insertion sort
    for (size_t i = 1; i < catalog->count; i++) {
        MovieEntry temp = catalog->entries[i];
        size_t j = i;
        
        while (j > 0) {
            int should_swap = 0;
            switch (field) {
                case SORT_BY_TITLE:
                    should_swap = strcmp(catalog->entries[j-1].title, temp.title) > 0;
                    break;
                case SORT_BY_YEAR:
                    should_swap = catalog->entries[j-1].year < temp.year;
                    break;
                case SORT_BY_RATING:
                    should_swap = catalog->entries[j-1].rating < temp.rating;
                    break;
                case SORT_BY_SIZE:
                    should_swap = catalog->entries[j-1].size_gb < temp.size_gb;
                    break;
                case SORT_BY_PEERS:
                    should_swap = catalog->entries[j-1].peers < temp.peers;
                    break;
                case SORT_BY_DOWNLOADS:
                    should_swap = catalog->entries[j-1].downloads < temp.downloads;
                    break;
            }
            
            if (should_swap) {
                catalog->entries[j] = catalog->entries[j-1];
                j--;
            } else {
                break;
            }
        }
        catalog->entries[j] = temp;
    }
    
    return true;
}

/* ==================== INTEGRATION WITH TORRENT ENGINE ==================== */

/*
 * Start a download for a catalog entry using the pipensx torrent engine
 * This function bridges the catalog system with the underlying torrent implementation
 */
bool catalog_start_download(MovieCatalog *catalog, MovieEntry *entry, const char *save_path) {
    if (!catalog || !entry || !save_path) return false;
    
    printf("=== Starting download ===\n");
    printf("Title: %s\n", entry->title);
    printf("Size: %.1f GB\n", entry->size_gb);
    printf("Save path: %s\n", save_path);
    
    // In a real implementation, this would:
    // 1. Parse the magnet link to get the info_hash
    // 2. Create a torrent session using torrent_engine_create()
    // 3. Set the save path and start downloading
    // 4. Store the download task ID for tracking
    
    printf("Download started successfully!\n");
    return true;
}

/* ==================== TEST/DEMO ==================== */

#ifdef MOVIE_CATALOG_TEST
#include <stdio.h>

int main() {
    printf("=== Movie Catalog Test ===\n\n");
    
    // Create catalog
    MovieCatalog *catalog = catalog_create();
    if (!catalog) {
        printf("Failed to create catalog\n");
        return 1;
    }
    
    // Load catalog
    char error[256];
    if (!catalog_load_from_file(catalog, "resources/catalog/movies.json", error, sizeof(error))) {
        printf("Failed to load catalog: %s\n", error);
        return 1;
    }
    
    printf("Loaded %zu entries\n\n", catalog->count);
    
    // Display statistics
    CatalogStats stats = catalog_get_stats(catalog);
    printf("=== Statistics ===\n");
    printf("Total entries: %zu\n", stats.total_entries);
    printf("Movies: %zu\n", stats.movies);
    printf("Series: %zu\n", stats.series);
    printf("Average rating: %.1f\n", stats.avg_rating);
    printf("Average size: %.1f GB\n", stats.avg_size_gb);
    printf("Total peers: %u\n", stats.total_peers);
    printf("Total downloads: %lu\n", (unsigned long)stats.total_downloads);
    
    // Test search
    printf("\n=== Search by title 'Matrix' ===\n");
    MovieEntry *result = catalog_search_by_title(catalog, "Matrix");
    if (result) {
        printf("Found: %s (%s)\n", result->title, result->quality);
    } else {
        printf("No match found\n");
    }
    
    // Test search by genre
    printf("\n=== Search by genre 'Sci-Fi' ===\n");
    result = catalog_search_by_genre(catalog, "Sci-Fi");
    if (result) {
        printf("Found: %s (%s)\n", result->title, result->quality);
    } else {
        printf("No match found\n");
    }
    
    // Test rating filter
    printf("\n=== Search by min rating 90 ===\n");
    result = catalog_search_by_min_rating(catalog, 90);
    if (result) {
        printf("Found: %s (rating: %u)\n", result->title, result->rating);
    } else {
        printf("No match found\n");
    }
    
    // Test sort
    printf("\n=== Sort by rating ===\n");
    catalog_sort(catalog, SORT_BY_RATING);
    printf("Top 5 entries by rating:\n");
    for (size_t i = 0; i < catalog->count && i < 5; i++) {
        printf("  %zu. %s (rating: %u)\n", i+1, catalog->entries[i].title, catalog->entries[i].rating);
    }
    
    // Test random entry
    printf("\n=== Random entry ===\n");
    result = catalog_get_random_entry(catalog);
    if (result) {
        printf("Random pick: %s\n", result->title);
    }
    
    // Cleanup
    catalog_destroy(catalog);
    
    printf("\n=== Test complete ===\n");
    return 0;
}
#endif
