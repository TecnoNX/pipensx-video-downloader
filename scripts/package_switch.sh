#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# pipensx-video-downloader — SD card packaging script
#
# Produces the directory layout:
#   sdcard/switch/pipensx-video-downloader/
#       ├── pipensx-video-downloader.nro   (built separately)
#       ├── resources/
#       │   └── catalog/movies.json
#       └── README.txt
#
# Prerequisites for .nro:
#   - devkitPRO + elf2nro + nacp-tool (install via sudo pacman)
#   - Or use the Makefile.switch target which calls this script
#
# Usage:
#   ./scripts/package_switch.sh          # Full package
#   ./scripts/package_switch.sh icon     # Generate placeholder icon
#   ./scripts/package_switch.sh nacp     # Generate NACP metadata
#   ./scripts/package_switch.sh sdcard   # Assemble SD card structure
#   ./scripts/package_switch.sh all      # Everything (default)
# ============================================================

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build-switch}"
SDCARD="${ROOT}/sdcard"
APP_NAME="pipensx-video-downloader"
APP_TITLE="PipensX Video Downloader"
APP_AUTHOR="TecnoNX"
APP_VERSION="1.0.0"

mkdir -p "${SDCARD}/switch/${APP_NAME}"

# ---------- Icon ----------
generate_icon() {
    echo "[icon] Generating placeholder icon..."
    # Icon dimensions: 144x80 PNG (Switch homebrew standard)
    # If ImageMagick is available, generate a simple icon
    if command -v convert &>/dev/null; then
        convert -size 144x80 \
            xc:"#1a1a2e" \
            -gravity center \
            -pointsize 28 \
            -fill "#e94560" \
            -annotate 0 "PXVD" \
            "${ROOT}/resources/icon.png"
        echo "  -> ${ROOT}/resources/icon.png"
    else
        echo "  [WARN] ImageMagick not found. Create resources/icon.png manually (144x80 PNG)."
        echo "         A placeholder icon will be provided by the icon subcommand."
    fi
}

# ---------- NACP ----------
generate_nacp() {
    echo "[nacp] Generating NACP metadata..."
    local nacp_file="${ROOT}/resources/pipensx-video-downloader.nacp"
    cat > "${nacp_file}" <<EOF
name: ${APP_TITLE}
author: ${APP_AUTHOR}
version: ${APP_VERSION}
EOF
    echo "  -> ${nacp_file}"
}

# ---------- SD card structure ----------
assemble_sdcard() {
    echo "[sdcard] Assembling SD card structure..."
    local dest="${SDCARD}/switch/${APP_NAME}"
    
    # Copy NRO if it exists
    local nro="${BUILD_DIR}/${APP_NAME}.nro"
    if [[ -f "${nro}" ]]; then
        cp "${nro}" "${dest}/"
        echo "  NRO: ${nro} -> ${dest}/"
    elif [[ -f "${ROOT}/pipensx-video-downloader.nro" ]]; then
        cp "${ROOT}/pipensx-video-downloader.nro" "${dest}/"
        echo "  NRO: ${ROOT}/pipensx-video-downloader.nro -> ${dest}/"
    else
        echo "  [WARN] No .nro found. Build with 'make switch' first."
    fi
    
    # Copy resources
    mkdir -p "${dest}/resources/catalog"
    if [[ -f "${ROOT}/resources/catalog/movies.json" ]]; then
        cp "${ROOT}/resources/catalog/movies.json" "${dest}/resources/catalog/"
        echo "  Catalog: movies.json"
    fi
    
    # Copy README for SD card
    cat > "${dest}/README.txt" <<EOF
PipensX Video Downloader v${APP_VERSION}
=========================================
Homebrew BitTorrent client for Nintendo Switch

CONTENTS:
- pipensx-video-downloader.nro — Main application
- resources/catalog/movies.json — Pre-loaded movie/series catalog

USAGE:
1. Copy this entire folder to your SD card
2. Launch via homebrew launcher (hekate, oild, etc.)
3. Browse the catalog, select a movie/series
4. Start download — content saved to SD card

REQUIRES:
- Custom firmware (AtmoSphere recommended)
- Homebrew launcher

CREDITS:
- Core BitTorrent engine by TecnoNX
- UI: Borealis framework
- DHT: jech's Kademlia
- Transport: libutp (μTP)

LICENSE: MIT
=========================================
EOF
    echo "  README.txt"
    
    echo ""
    echo "SD card structure ready at: ${dest}/"
    find "${dest}" -type f | sort
}

# ---------- Main ----------
case "${1:-all}" in
    icon)
        generate_icon
        ;;
    nacp)
        generate_nacp
        ;;
    sdcard)
        assemble_sdcard
        ;;
    all)
        generate_icon
        generate_nacp
        assemble_sdcard
        ;;
    clean)
        rm -rf "${SDCARD}"
        echo "Cleaned sdcard/ directory"
        ;;
    help)
        echo "Usage: $0 [icon|nacp|sdcard|all|clean|help]"
        ;;
    *)
        echo "Unknown command: $1"
        echo "Use '$0 help' for usage"
        exit 1
        ;;
esac
