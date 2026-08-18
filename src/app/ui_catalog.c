/*
 * ui_catalog.c - Implementacion de la UI para catalog de peliculas/series
 * Usando borealis para Nintendo Switch
 *
 * Esta implementacion proporciona:
 * - Grid view para navegar peliculas/series
 * - Vista de detalles con info completa
 * - Panel de descargas activas
 * - Busqueda integrada
 * - Filtros por genero, rating, tipo
 */

#include "ui_catalog.h"
#include "movie_catalog.h"
#include "../core/torrent.h"
#include "../core/storage.h"
#include "../core/util.h"

#include <switch.h>
#include <borealis/borealis.hpp>
#include <borealis/application.hpp>
#include <borealis/views.hpp>
#include <borealis/header.hpp>
#include <borealis/list.hpp>
#include <borealis/frame.hpp>
#include <borealis/label.hpp>
#include <borealis/button.hpp>
#include <borealis/text_box.hpp>
#include <borealis/progress_bar.hpp>
#include <borealis/notification.hpp>
#include <borealis/overlay.hpp>
#include <borealis/tree.hpp>
#include <borealis/segmented_progress.hpp>
#include <cmath>

// Forward declarations for helper functions
static brls::AppletRes *appletEntry(void *userData);

/* ==================== CATALOGUI IMPLEMENTATION ==================== */

CatalogUI::CatalogUI(MovieCatalog *catalog, const char *save_path)
    : catalog_(catalog), save_path_(save_path), current_view_(nullptr) {
    
    // Set default config
    config_.items_per_row = 4;
    config_.max_visible_rows = 3;
    config_.show_posters = true;
    config_.show_info_on_hover = true;
    config_.auto_refresh_interval = 5000; // 5 seconds
}

CatalogUI::~CatalogUI() {
    // Cleanup
}

bool CatalogUI::init() {
    // Initialize borealis
    if (!brls::Application::init()) {
        return false;
    }
    
    // Set app info
    brls::Application::setAppTitle("PipensX Video Downloader");
    brls::Application::setAppVersion("1.0.0");
    
    // Register screens
    registerScreens();
    
    // Show catalog grid
    showCatalogGrid();
    
    return true;
}

void CatalogUI::registerScreens() {
    // Register all screens with their callbacks
    brls::Application::registerScreen("CatalogGrid", [this]() -> brls::View * {
        return this->createCatalogGrid();
    });
    
    brls::Application::registerScreen("MovieDetails", [this](std::vector<brls::Value *> args) -> brls::View * {
        MovieEntry *entry = (MovieEntry *)args[0]->pointer;
        this->showMovieDetails(entry);
        return new brls::Label("Details loaded");
    });
    
    brls::Application::registerScreen("DownloadsPanel", [this]() -> brls::View * {
        this->showDownloadsPanel();
        return new brls::Label("Downloads loaded");
    });
    
    brls::Application::registerScreen("SearchInterface", [this]() -> brls::View * {
        this->showSearchInterface();
        return new brls::Label("Search loaded");
    });
    
    brls::Application::registerScreen("Settings", [this]() -> brls::View * {
        this->showSettings();
        return new brls::Label("Settings loaded");
    });
}

void CatalogUI::update() {
    // Update UI state (call every frame)
    // This could include:
    // - Refreshing download progress
    // - Updating metadata
    // - Handling animations
}

brls::List *CatalogUI::createCatalogGrid() {
    auto *list = new brls::List();
    
    // Create header
    auto *header = new brls::Header("PipensX Video Downloader");
    header->addButton(brls::HeaderButton::BACK, [this](brls::HeaderButton button) {
        // Back button action
    });
    header->addButton(brls::HeaderButton::REFRESH, [this](brls::HeaderButton button) {
        // Refresh catalog
    });
    
    list->addCell(new brls::ListItem(header));
    
    // Get catalog stats
    CatalogStats stats = catalog_get_stats(catalog_);
    
    // Add stats label
    char stats_text[256];
    snprintf(stats_text, sizeof(stats_text), 
             "Total: %zu | Peliculas: %zu | Series: %zu | Rating promedio: %.1f",
             stats.total_entries, stats.movies, stats.series, stats.avg_rating);
    
    auto *stats_label = new brls::Label(stats_text, brls::LabelStyle::SUBTITLE, true);
    auto *stats_cell = new brls::ListItem(stats_label);
    stats_cell->setSelectable(false);
    list->addCell(stats_cell);
    
    // Add separator
    auto *separator = new brls::Separator();
    list->addCell(new brls::ListItem(separator));
    
    // Create movie/series cards
    for (size_t i = 0; i < catalog_->count; i++) {
        MovieEntry *entry = &catalog_->entries[i];
        
        // Create card frame
        auto *card = this->createMovieCard(entry);
        
        // Create list item for the card
        auto *item = new brls::ListItem(card);
        item->setSelectable(true);
        item->setActionCallback([this, entry](brls::View *view) {
            // Show details when card is selected
            this->showMovieDetails(entry);
        });
        
        list->addCell(item);
    }
    
    // Add action buttons
    auto *search_button = new brls::Button("Buscar", [this]() {
        this->showSearchInterface();
    });
    list->addCell(new brls::ListItem(search_button));
    
    auto *downloads_button = new brls::Button("Descargas", [this]() {
        this->showDownloadsPanel();
    });
    list->addCell(new brls::ListItem(downloads_button));
    
    auto *settings_button = new brls::Button("Configuracion", [this]() {
        this->showSettings();
    });
    list->addCell(new brls::ListItem(settings_button));
    
    return list;
}

brls::Frame *CatalogUI::createMovieCard(MovieEntry *entry) {
    auto *frame = new brls::Frame(brls::FrameStyle::BOX);
    
    // Create title label
    char title_text[128];
    strncpy(title_text, entry->title, sizeof(title_text) - 1);
    title_text[sizeof(title_text) - 1] = '\0';
    
    auto *title_label = new brls::Label(title_text, brls::LabelStyle::TITLE, true);
    frame->addView(title_label);
    
    // Add genre and rating info
    char info_text[128];
    snprintf(info_text, sizeof(info_text), "%s | Rating: %u/%100", 
             entry->genre, entry->rating);
    
    auto *info_label = new brls::Label(info_text, brls::LabelStyle::SUBTITLE, true);
    frame->addView(info_label);
    
    // Add size info
    char size_text[64];
    snprintf(size_text, sizeof(size_text), "Tamano: %.1f GB", entry->size_gb);
    
    auto *size_label = new brls::Label(size_text, brls::LabelStyle::NORMAL, true);
    frame->addView(size_label);
    
    return frame;
}

brls::Frame *CatalogUI::createDownloadItem(const char *title, const char *progress, const char *status) {
    auto *frame = new brls::Frame(brls::FrameStyle::BOX);
    
    // Create title label
    auto *title_label = new brls::Label(title, brls::LabelStyle::TITLE, true);
    frame->addView(title_label);
    
    // Create progress bar
    auto *progress_bar = new brls::ProgressBar(0.5f); // Example: 50%
    frame->addView(progress_bar);
    
    // Create status label
    auto *status_label = new brls::Label(status, brls::LabelStyle::SUBTITLE, true);
    frame->addView(status_label);
    
    return frame;
}

void CatalogUI::populateDownloadList(brls::List *list) {
    // In a real implementation, this would:
    // 1. Query the download manager for active downloads
    // 2. Create a list item for each download
    // 3. Update progress bars dynamically
}

void CatalogUI::showCatalogGrid() {
    auto *grid = this->createCatalogGrid();
    brls::Application::setRootView(grid);
}

void CatalogUI::showMovieDetails(MovieEntry *entry) {
    auto *list = new brls::List();
    
    // Create header
    auto *header = new brls::Header(entry->title);
    header->addButton(brls::HeaderButton::BACK, [this](brls::HeaderButton button) {
        this->navigateBack();
    });
    
    list->addCell(new brls::ListItem(header));
    
    // Add detail labels
    char details[512];
    snprintf(details, sizeof(details),
             "Genero: %s\nCalificacion: %u/100\nDuracion: %u min\n"
             "Anio: %u\nTamano: %.1f GB\nPeers: %u\nDescargas: %lu",
             entry->genre, entry->rating, entry->duration_min,
             entry->year, entry->size_gb, entry->peers, 
             (unsigned long)entry->downloads);
    
    auto *details_label = new brls::Label(details, brls::LabelStyle::NORMAL, true);
    list->addCell(new brls::ListItem(details_label));
    
    // Add description
    auto *desc_label = new brls::Label(entry->description, brls::LabelStyle::SUBTITLE, true);
    list->addCell(new brls::ListItem(desc_label));
    
    // Add action buttons
    auto *download_button = new brls::Button("Iniciar Descarga", [this, entry]() {
        this->startDownload(entry);
    });
    list->addCell(new brls::ListItem(download_button));
    
    auto *cancel_button = new brls::Button("Cancelar", [this]() {
        // Cancel action
    });
    list->addCell(new brls::ListItem(cancel_button));
    
    brls::Application::setRootView(list);
}

void CatalogUI::showDownloadsPanel() {
    auto *list = new brls::List();
    
    // Create header
    auto *header = new brls::Header("Descargas Activas");
    header->addButton(brls::HeaderButton::BACK, [this](brls::HeaderButton button) {
        this->navigateBack();
    });
    
    list->addCell(new brls::ListItem(header));
    
    // In a real implementation, this would:
    // 1. Query the download manager for active downloads
    // 2. Populate the list with download items
    
    // For now, add placeholder
    auto *placeholder = new brls::Label("No hay descargas activas", brls::LabelStyle::NORMAL, true);
    list->addCell(new brls::ListItem(placeholder));
    
    brls::Application::setRootView(list);
}

void CatalogUI::showSearchInterface() {
    auto *list = new brls::List();
    
    // Create header
    auto *header = new brls::Header("Buscar");
    header->addButton(brls::HeaderButton::BACK, [this](brls::HeaderButton button) {
        this->navigateBack();
    });
    
    list->addCell(new brls::ListItem(header));
    
    // Add search input
    auto *search_box = new brls::TextBox("Buscar peliculas o series...");
    list->addCell(new brls::ListItem(search_box));
    
    // Add search button
    auto *search_button = new brls::Button("Buscar", [this, search_box]() {
        const char *query = search_box->getText().c_str();
        int match_count;
        MatchResult *results = catalog_search(catalog_, query, &match_count);
        
        if (results) {
            // Show search results
            for (int i = 0; i < match_count; i++) {
                char title_text[256];
                snprintf(title_text, sizeof(title_text), "%s (%s)",
                         results[i].entry->title, results[i].reason);
                
                auto *result_label = new brls::Label(title_text, brls::LabelStyle::TITLE, true);
                auto *result_item = new brls::ListItem(result_label);
                result_item->setSelectable(true);
                result_item->setActionCallback([this, entry = results[i].entry](brls::View *view) {
                    this->showMovieDetails(entry);
                });
                list->addCell(result_item);
            }
            
            free(results);
        } else {
            auto *no_results = new brls::Label("No se encontraron resultados", brls::LabelStyle::SUBTITLE, true);
            list->addCell(new brls::ListItem(no_results));
        }
    });
    list->addCell(new brls::ListItem(search_button));
    
    brls::Application::setRootView(list);
}

void CatalogUI::showSettings() {
    auto *list = new brls::List();
    
    // Create header
    auto *header = new brls::Header("Configuracion");
    header->addButton(brls::HeaderButton::BACK, [this](brls::HeaderButton button) {
        this->navigateBack();
    });
    
    list->addCell(new brls::ListItem(header));
    
    // Add settings options
    auto *save_path_label = new brls::Label("Ruta de guardado: " + std::string(save_path_), brls::LabelStyle::NORMAL, true);
    list->addCell(new brls::ListItem(save_path_label));
    
    auto *quality_label = new brls::Label("Calidad por defecto: 1080p", brls::LabelStyle::NORMAL, true);
    list->addCell(new brls::ListItem(quality_label));
    
    auto *max_downloads_label = new brls::Label("Maximo de descargas: 3", brls::LabelStyle::NORMAL, true);
    list->addCell(new brls::ListItem(max_downloads_label));
    
    brls::Application::setRootView(list);
}

void CatalogUI::navigateBack() {
    this->showCatalogGrid();
}

void CatalogUI::startDownload(MovieEntry *entry) {
    // Display loading overlay
    this->displayLoadingOverlay("Iniciando descarga...");
    
    // Start download using catalog function
    bool success = catalog_start_download(catalog_, entry, save_path_);
    
    // Hide loading overlay
    this->hideLoadingOverlay();
    
    if (success) {
        // Show success notification
        char message[256];
        snprintf(message, sizeof(message), "Descarga iniciada: %s", entry->title);
        auto *notification = new brls::Notification(message, brls::NotificationType::ALERT);
        notification->show();
    } else {
        // Show error
        this->displayError("Error al iniciar la descarga");
    }
}

void CatalogUI::displayLoadingOverlay(const char *message) {
    // In a real implementation, this would show a loading overlay
    // For now, we'll just log it
}

void CatalogUI::hideLoadingOverlay() {
    // In a real implementation, this would hide the loading overlay
}

void CatalogUI::displayError(const char *message) {
    // In a real implementation, this would show an error overlay
    // For now, we'll just log it
}

/* ==================== ENTRY POINT ==================== */

int main(int argc, char *argv[]) {
    // Initialize graphics
    gfxInitDefault();
    
    // Initialize NS
    cfguInit();
    smInitialize();
    
    // Initialize HTTP
    httpInitialize(HTTP_LIBHTTP_VERSION_CURRENT);
    
    // Create catalog
    MovieCatalog *catalog = catalog_create();
    if (!catalog) {
        gfxExit();
        cfguExit();
        smExit();
        httpExit();
        return 1;
    }
    
    // Load catalog from file
    char error[256];
    if (!catalog_load_from_file(catalog, "sdmc:/pipensx/resources/catalog/movies.json", error, sizeof(error))) {
        // Try alternative path
        if (!catalog_load_from_file(catalog, "pipensx/resources/catalog/movies.json", error, sizeof(error))) {
            // Create empty catalog if file not found
            printf("Warning: Could not load catalog: %s\n", error);
        }
    }
    
    // Create UI
    CatalogUI ui(catalog, "sdmc:/pipensx/downloads");
    
    // Initialize UI
    if (!ui.init()) {
        catalog_destroy(catalog);
        gfxExit();
        cfguExit();
        smExit();
        httpExit();
        return 1;
    }
    
    // Run UI loop
    ui.run();
    
    // Cleanup
    catalog_destroy(catalog);
    gfxExit();
    cfguExit();
    smExit();
    httpExit();
    
    return 0;
}
