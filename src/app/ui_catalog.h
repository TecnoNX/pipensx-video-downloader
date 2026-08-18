#pragma once

/*
 * ui_catalog.h - Interfaz de usuario para catalog de peliculas/series
 * Usando borealis para Nintendo Switch
 *
 * Esta interfaz proporciona:
 * - Grid view para navegar peliculas/series
 * - Vista de detalles con info completa
 * - Panel de descargas activas
 * - Busqueda integrada
 * - Filtros por genero, rating, tipo
 */

#include "movie_catalog.h"
#include <switch.h>
#include <borealis/borealis.hpp>
#include <borealis/application.hpp>

// Screen types
typedef enum {
    SCREEN_MAIN,      // Catalog grid view
    SCREEN_DETAILS,   // Movie/series details
    SCREEN_DOWNLOADS, // Active downloads panel
    SCREEN_SEARCH,    // Search interface
    SCREEN_SETTINGS   // Settings
} ScreenType;

// UI configuration
typedef struct {
    int items_per_row;
    int max_visible_rows;
    bool show_posters;
    bool show_info_on_hover;
    uint32_t auto_refresh_interval; // ms
} UiConfig;

// Main UI class
class CatalogUI : public brls::Application {
public:
    CatalogUI(MovieCatalog *catalog, const char *save_path);
    ~CatalogUI() override;

    // Initialize the UI
    bool init();

    // Register screens
    void registerScreens();

    // Update UI (call every frame)
    void update();

    // Show catalog grid
    void showCatalogGrid();

    // Show movie/series details
    void showMovieDetails(MovieEntry *entry);

    // Show downloads panel
    void showDownloadsPanel();

    // Show search interface
    void showSearchInterface();

    // Show settings
    void showSettings();

    // Navigate back
    void navigateBack();

    // Start download for entry
    void startDownload(MovieEntry *entry);

    // Get current catalog
    MovieCatalog *getCatalog() const { return catalog_; }

private:
    MovieCatalog *catalog_;
    const char *save_path_;
    UiConfig config_;
    brls::View *current_view_;
    
    // Helper functions
    brls::List *createCatalogGrid();
    brls::Frame *createMovieCard(MovieEntry *entry);
    brls::Frame *createDownloadItem(const char *title, const char *progress, const char *status);
    void populateDownloadList(brls::List *list);
    void displayLoadingOverlay(const char *message);
    void hideLoadingOverlay();
    void displayError(const char *message);
};
