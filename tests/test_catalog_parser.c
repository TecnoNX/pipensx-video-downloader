/* test_catalog_parser.c - Test standalone catalog parser */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "src/app/movie_catalog.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  TEST %2d: %-60s", tests_run, name); \
} while(0)

#define PASS() do { \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define FAIL(msg) do { \
    tests_failed++; \
    printf("FAIL: %s\n", msg); \
} while(0)

static void test_create_destroy(void) {
    TEST("catalog_create / catalog_destroy");
    MovieCatalog *catalog = catalog_create();
    assert(catalog != NULL);
    assert(catalog->count == 0);
    catalog_destroy(catalog);
    PASS();
}

static void test_load_movies_json(void) {
    TEST("catalog_load_from_file (movies.json)");
    MovieCatalog *catalog = catalog_create();
    assert(catalog != NULL);
    
    char error[512];
    int result = catalog_load_from_file(catalog, 
        "resources/catalog/movies.json", error, sizeof(error));
    assert(result == 1);
    assert(catalog->count == 15);
    
    /* Verify first entry */
    assert(strncmp(catalog->entries[0].title, "The Matrix", 10) == 0);
    assert(catalog->entries[0].year == 1999);
    assert(catalog->entries[0].rating > 0);
    assert(catalog->entries[0].size_gb > 0);
    
    /* Verify entry with special chars in title */
    int found_special = 0;
    for (size_t i = 0; i < catalog->count; i++) {
        const char *t = catalog->entries[i].title;
        if (strstr(t, "[1080p") || strstr(t, "(1999)") || strstr(t, ":")) {
            found_special = 1;
            break;
        }
    }
    assert(found_special);
    
    catalog_destroy(catalog);
    PASS();
}

static void test_get_stats(void) {
    TEST("catalog_get_stats");
    MovieCatalog *catalog = catalog_create();
    catalog_load_from_file(catalog, "resources/catalog/movies.json", NULL, 0);
    
    CatalogStats stats = catalog_get_stats(catalog);
    assert(stats.total_entries == 15);
    assert(stats.movies > 0);
    assert(stats.series > 0);
    assert(stats.avg_rating > 0);
    
    catalog_destroy(catalog);
    PASS();
}

static void test_search_by_title(void) {
    TEST("catalog_search_by_title");
    MovieCatalog *catalog = catalog_create();
    catalog_load_from_file(catalog, "resources/catalog/movies.json", NULL, 0);
    
    /* Search for "Matrix" */
    MovieEntry *found = catalog_search_by_title(catalog, "Matrix");
    assert(found != NULL);
    assert(strstr(found->title, "Matrix") != NULL);
    
    /* Search for something that doesn't exist */
    found = catalog_search_by_title(catalog, "ZZZZnonexistent");
    assert(found == NULL);
    
    catalog_destroy(catalog);
    PASS();
}

static void test_search_by_genre(void) {
    TEST("catalog_search_by_genre");
    MovieCatalog *catalog = catalog_create();
    catalog_load_from_file(catalog, "resources/catalog/movies.json", NULL, 0);
    
    MovieEntry *found = catalog_search_by_genre(catalog, "Action");
    assert(found != NULL);
    
    catalog_destroy(catalog);
    PASS();
}

static void test_full_search(void) {
    TEST("catalog_search (full)");
    MovieCatalog *catalog = catalog_create();
    catalog_load_from_file(catalog, "resources/catalog/movies.json", NULL, 0);
    
    int count;
    MatchResult *results = catalog_search(catalog, "Action", &count);
    assert(results != NULL);
    assert(count > 0);
    /* Action appears in genre of multiple entries */
    free(results);
    
    /* Search for something not in any field */
    results = catalog_search(catalog, "ZZZZnonexistent", &count);
    assert(results == NULL);
    assert(count == 0);
    
    catalog_destroy(catalog);
    PASS();
}

static void test_sort(void) {
    TEST("catalog_sort (SORT_BY_RATING)");
    MovieCatalog *catalog = catalog_create();
    catalog_load_from_file(catalog, "resources/catalog/movies.json", NULL, 0);
    
    int result = catalog_sort(catalog, SORT_BY_RATING);
    assert(result == 1);
    
    /* First entry should have highest rating */
    assert(catalog->entries[0].rating >= catalog->entries[1].rating);
    
    catalog_destroy(catalog);
    PASS();
}

static void test_entry_fields(void) {
    TEST("All entry fields populated");
    MovieCatalog *catalog = catalog_create();
    catalog_load_from_file(catalog, "resources/catalog/movies.json", NULL, 0);
    
    for (size_t i = 0; i < catalog->count; i++) {
        MovieEntry *e = &catalog->entries[i];
        assert(e->title[0] != '\0');
        assert(e->magnet[0] != '\0');
        assert(e->genre[0] != '\0');
        assert(e->description[0] != '\0');
        assert(e->info_hash[0] != '\0');
        assert(e->year > 0);
        assert(e->rating > 0);
        assert(e->size_gb > 0);
    }
    
    catalog_destroy(catalog);
    PASS();
}

static void test_info_hash(void) {
    TEST("Info hashes are valid (40 hex chars)");
    MovieCatalog *catalog = catalog_create();
    catalog_load_from_file(catalog, "resources/catalog/movies.json", NULL, 0);
    
    for (size_t i = 0; i < catalog->count; i++) {
        assert(strlen(catalog->entries[i].info_hash) == 40);
        for (int j = 0; j < 40; j++) {
            char c = catalog->entries[i].info_hash[j];
            assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
        }
    }
    
    catalog_destroy(catalog);
    PASS();
}

static void test_get_by_hash(void) {
    TEST("catalog_get_by_hash");
    MovieCatalog *catalog = catalog_create();
    catalog_load_from_file(catalog, "resources/catalog/movies.json", NULL, 0);
    
    const char *first_hash = catalog->entries[0].info_hash;
    MovieEntry *found = catalog_get_by_hash(catalog, first_hash);
    assert(found != NULL);
    assert(strcmp(found->info_hash, first_hash) == 0);
    
    found = catalog_get_by_hash(catalog, "0000000000000000000000000000000000000000");
    assert(found == NULL);
    
    catalog_destroy(catalog);
    PASS();
}

static void test_random_entry(void) {
    TEST("catalog_get_random_entry");
    MovieCatalog *catalog = catalog_create();
    catalog_load_from_file(catalog, "resources/catalog/movies.json", NULL, 0);
    
    MovieEntry *r = catalog_get_random_entry(catalog);
    assert(r != NULL);
    assert(r->title[0] != '\0');
    
    catalog_destroy(catalog);
    PASS();
}

int main(void) {
    printf("\n=== Movie Catalog Parser Tests ===\n\n");
    
    test_create_destroy();
    test_load_movies_json();
    test_get_stats();
    test_search_by_title();
    test_search_by_genre();
    test_full_search();
    test_sort();
    test_entry_fields();
    test_info_hash();
    test_get_by_hash();
    test_random_entry();
    
    printf("\n=== Results ===\n");
    printf("Run: %d | Passed: %d | Failed: %d\n\n", tests_run, tests_passed, tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}
