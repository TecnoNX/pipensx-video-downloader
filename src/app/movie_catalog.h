#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum fields for a catalog entry
#define MAX_MOVIE_TITLE 256
#define MAX_MOVIE_GENRE 128
#define MAX_MOVIE_DESC 1024
#define MAX_MOVIE_MAGNET 512
#define MAX_MOVIE_POSTER 256
#define MAX_MOVIE_QUALITY 32

// Catalog entry types
typedef enum {
    ENTRY_MOVIE,
    ENTRY_SERIES
} EntryType;

// Catalog entry structure
typedef struct {
    EntryType type;
    char title[MAX_MOVIE_TITLE];
    char genre[MAX_MOVIE_GENRE];
    char description[MAX_MOVIE_DESC];
    char magnet[MAX_MOVIE_MAGNET];
    char poster[MAX_MOVIE_POSTER];
    char quality[MAX_MOVIE_QUALITY];
    uint32_t year;
    uint32_t rating;      // 0-100 (e.g., 87 = 8.7)
    uint32_t duration_min; // For movies (minutes)
    uint32_t seasons;      // For series (0 if movie)
    uint32_t episodes;     // For series (0 if movie)
    double size_gb;
    uint32_t peers;
    uint64_t downloads;
    char info_hash[41];   // 40 hex chars + null
} MovieEntry;

// Catalog structure
typedef struct {
    MovieEntry *entries;
    size_t count;
    size_t capacity;
    char *raw_json;       // Original JSON for reference
    size_t raw_json_size; // Original JSON size
} MovieCatalog;

// Catalog operations
typedef struct {
    MovieEntry *entry;   // The matched entry
    uint32_t score;      // Match quality (higher = better)
    const char *reason;  // Why it matched (for UI display)
} MatchResult;

// Initialize an empty catalog
MovieCatalog *catalog_create(void);

// Load catalog from JSON file
bool catalog_load_from_file(MovieCatalog *catalog, const char *filepath, char *error, size_t error_size);

// Load catalog from string
bool catalog_load_from_string(MovieCatalog *catalog, const char *json_string, char *error, size_t error_size);

// Free catalog
void catalog_destroy(MovieCatalog *catalog);

// Search catalog by title (partial match)
MovieEntry *catalog_search_by_title(MovieCatalog *catalog, const char *query);

// Search catalog by genre
MovieEntry *catalog_search_by_genre(MovieCatalog *catalog, const char *genre);

// Search catalog by rating (minimum)
MovieEntry *catalog_search_by_min_rating(MovieCatalog *catalog, uint32_t min_rating);

// Get random entry
MovieEntry *catalog_get_random_entry(MovieCatalog *catalog);

// Get entry by info_hash
MovieEntry *catalog_get_by_hash(MovieCatalog *catalog, const char *info_hash);

// Search catalog and return best matches
MatchResult *catalog_search(MovieCatalog *catalog, const char *query, int *match_count);

// Filter catalog by type (movie/series)
MovieEntry *catalog_filter_by_type(MovieCatalog *catalog, EntryType type);

// Get catalog statistics
typedef struct {
    size_t total_entries;
    size_t movies;
    size_t series;
    double avg_rating;
    double avg_size_gb;
    uint32_t total_peers;
    uint64_t total_downloads;
} CatalogStats;

CatalogStats catalog_get_stats(MovieCatalog *catalog);

// Export catalog to JSON file
bool catalog_export_to_file(MovieCatalog *catalog, const char *filepath, char *error, size_t error_size);

// Add entry to catalog
bool catalog_add_entry(MovieCatalog *catalog, MovieEntry entry);

// Remove entry by index
bool catalog_remove_entry(MovieCatalog *catalog, size_t index);

// Sort catalog by field
typedef enum {
    SORT_BY_TITLE,
    SORT_BY_YEAR,
    SORT_BY_RATING,
    SORT_BY_SIZE,
    SORT_BY_PEERS,
    SORT_BY_DOWNLOADS
} SortField;

bool catalog_sort(MovieCatalog *catalog, SortField field);

#ifdef __cplusplus
}
#endif
