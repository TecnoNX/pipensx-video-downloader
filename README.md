# PipensX Video Downloader

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Language: C](https://img.shields.io/badge/language-C11-blue.svg)](https://en.cppreference.com/w/c)

Download manager for videos and movies via BitTorrent, built on the **PipensX** BitTorrent core. Designed to stream and save content directly to SD card storage — originally developed for Nintendo Switch homebrew, with a portable CLI for Linux/PC.

---

## Table of Contents

- [Features](#features)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Build](#build)
  - [Linux / Portable CLI](#linux--portable-cli)
  - [Nintendo Switch](#nintendo-switch)
- [Usage](#usage)
- [API Overview](#api-overview)
- [Dependencies](#dependencies)
- [License](#license)
- [Credits](#credits)

---

## Features

- **BitTorrent Client** — Full implementation of the BitTorrent protocol v1 (BEP 3) and v2 (BEP 52)
- **μTP Support** — Low-latency transport via vendored libutp (LEDBAT congestion control)
- **DHT (Kademlia)** — Decentralized peer discovery via vendored jech's dht
- **MSE/PE Encryption** — Message Stream Encryption (BEP 3/4) with RC4 keystream and DH key exchange
- **Web Seeds (HTTP/HTTPS)** — Fallback download via web seeds (BEP 17/19)
- **Fast Resume** — Have-bitfield snapshotting for crash recovery without re-hash scan
- **Adaptive Peer Management** — Choking/unchoking, pipelining, request hedging, rate-based peer evaluation
- **Video Streaming** — Stream video content while downloading; seekable playback via HTTP server
- **SD Card Storage** — Direct write to SD card filesystem (fatfs on Switch, raw filesystem on PC)
- **Multi-Tracker Support** — Announce to up to 32 trackers per torrent
- **Debrid Provider Integration** — Real-Debrid and TorBox support (inherited from PipensX)

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                     │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────┐ │
│  │  Video Down- │  │  HTTP Server │  │  Stream       │ │
│  │  loader CLI  │  │  (for playback)│ │  Manager      │ │
│  └──────┬───────┘  └──────┬───────┘  └───────┬───────┘ │
│         │                 │                   │          │
│  ┌──────┴─────────────────┴───────────────────┴───────┐ │
│  │              Download Manager / App Service         │ │
│  │  Torrent lifecycle, metadata, debrid, catalog       │ │
│  └─────────────────────┬──────────────────────────────┘ │
├─────────────────────────┼───────────────────────────────┤
│                   Core Engine Layer                      │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────┐ │
│  │ Torrent  │ │  Peer    │ │  DHT     │ │  Tracker   │ │
│  │ Manager  │ │  Manager │ │  Engine  │ │  Client    │ │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘ └─────┬──────┘ │
│       │            │            │              │         │
│  ┌────┴────────────┴────┬───────┴──────────────┴──────┐ │
│  │          BitTorrent Protocol Core                  │ │
│  │  bencode · metainfo · piece · sha1/sha256 · mse   │ │
│  └──────────────────────┬─────────────────────────────┘ │
├─────────────────────────┼───────────────────────────────┤
│                  Network / Platform Layer                │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────┐ │
│  │  TCP/UDP │ │  libutp  │ │  libcurl │ │   Storage  │ │
│  │  Sockets │ │  (μTP)   │ │  (HTTP)  │ │   Backend  │ │
│  └──────────┘ └──────────┘ └──────────┘ └────────────┘ │
└─────────────────────────────────────────────────────────┘
```

### Core Modules

| Module | File(s) | Description |
|--------|---------|-------------|
| Torrent | `torrent.c/h` | Master controller: peer management, scheduling, choking, telemetry |
| Peer | `peer.c/h` | Per-peer state: handshake, messaging (bitfield/choke/request/piece), MSE crypto |
| DHT | `dht.c/h` + `dht_vendor/dht.c` | Kademlia DHT engine (jech), node management, peer discovery |
| Tracker | `tracker.c/h` | HTTP tracker announce (GET/POST), compact peer list parsing |
| Piece | `piece.c/h` | Piece scheduling, block-level hashing, buffer pooling, verify/write |
| MetaInfo | `metainfo.c/h` | .torrent file parser (bdecode), info-hash computation |
| Bencode | `bencode.c/h` | Read-only bencode parser (dicts, lists, strings, ints) |
| Net | `net.c/h` | TCP/UDP socket abstraction, poll-based event loop |
| MSE | `mse.c/h` | MSE/PE encryption: DH key exchange, RC4 keystream, handshake |
| SHA1/SHA256 | `sha1.c/h`, `sha256.c/h` | Hashing primitives for piece verification |
| Storage | `storage.c` | File write backend — SD card / FATFS / POSIX filesystem |
| libutp | `core/libutp/` | μTP: UDP-based low-latency transport (LEDBAT) |
| util | `util.c/h` | Logging, monotonic timers, random bytes |

---

## Project Structure

```
pipensx-video-downloader/
├── core/                        # BitTorrent protocol core
│   ├── torrent.c/h              # Torrent session controller
│   ├── peer.c/h                 # Peer connection & protocol
│   ├── dht.c/h                  # DHT engine wrapper
│   ├── dht_vendor/dht.c/h       # Vendored jech's Kademlia DHT
│   ├── tracker.c/h              # Tracker HTTP client
│   ├── metainfo.c/h             # .torrent parser
│   ├── bencode.c/h              # bencode parser
│   ├── piece.c/h                # Piece scheduling & hashing
│   ├── mse.c/h                  # MSE/PE encryption
│   ├── sha1.c/h                 # SHA-1 hashing
│   ├── sha256.c/h               # SHA-256 hashing
│   ├── net.c/h                  # Socket abstraction
│   ├── storage.c                # File storage backend
│   ├── util.c/h                 # Utilities (logging, timers)
│   └── libutp/                  # Vendored libutp (μTP)
├── src/                         # Application layer (TBD)
│                               # Video downloader CLI, HTTP stream server
├── tests/                       # Unit & integration tests (TBD)
├── scripts/                     # Build & utility scripts (TBD)
├── docs/                        # Documentation (TBD)
├── README.md
├── LICENSE
└── .gitignore
```

---

## Build

### Linux / Portable CLI

**Prerequisites:**
- GCC 11+ or Clang 14+ (C11)
- libcurl (headers + development)
- pthreads (usually built-in with GCC/Clang)

```bash
# Build portable CLI
make pc
```

A `Makefile.pc` will be provided once the `src/` layer is completed.

### Nintendo Switch

**Prerequisites:**
- devkitPro (switch toolchain)
- libnx SDK
- borealis UI library (for Switch UI)

```bash
# Build Switch NRO
make switch
```

---

## Usage

### Adding a Torrent

```c
// Parse a .torrent file
metainfo_t mi;
metainfo_parse(&mi, torrent_data, torrent_size);

// Create a torrent session
torrent_t *t = torrent_create(&mi, storage_config, NULL);

// Start downloading
torrent_start(t);

// Poll for progress (call periodically from main loop)
torrent_poll(t, 16);  // 16ms tick

// Check status
torrent_status_t status;
torrent_get_status(t, &status);
printf("Progress: %.1f%% | Peers: %d | Speed: %llu B/s\n",
       status.progress * 100.0, status.active_peers, status.download_rate);

// Cleanup
torrent_destroy(t);
metainfo_cleanup(&mi);
```

### Downloading via CLI (TBD)

```bash
# Download from magnet link
./pipensx-video-downloader --magnet "magnet:?xt=urn:btih:..." --output /media/sdcard/videos/

# Download from .torrent file
./pipensx-video-downloader --file movie.torrent --output /media/sdcard/videos/

# List available files in torrent (for selective download)
./pipensx-video-downloader --magnet "magnet:?xt=urn:btih:..." --list

# Stream mode (start HTTP server for playback)
./pipensx-video-downloader --magnet "magnet:?xt=urn:btih:..." --stream --port 8080
```

---

## API Overview

The core exposes a clean C API. No C++ dependencies, no external runtime.

### Core Types

```c
typedef struct torrent torrent_t;
typedef struct metainfo metainfo_t;
```

### Key Functions

```c
// Parse .torrent file
int metainfo_parse(metainfo_t *mi, const uint8_t *data, size_t len);
void metainfo_cleanup(metainfo_t *mi);

// Torrent lifecycle
torrent_t *torrent_create(const metainfo_t *mi, const storage_config_t *sc,
                          const torrent_config_t *tc);
void torrent_start(torrent_t *t);
void torrent_stop(torrent_t *t);
void torrent_destroy(torrent_t *t);

// Polling (call from main loop)
void torrent_poll(torrent_t *t, uint32_t elapsed_ms);

// Status
void torrent_get_status(const torrent_t *t, torrent_status_t *out);

// DHT engine (singleton)
dht_session_t *dht_attach(const uint8_t info_hash[20], uint16_t announce_port);
void dht_detach(dht_session_t *s);
```

---

## Dependencies

### Vendored (included in repo)

| Library | License | Purpose |
|---------|---------|---------|
| libutp | BSD-style | μTP low-latency transport (LEDBAT) |
| jech dht | MIT | Kademlia DHT engine |

### Required (external)

| Library | Purpose |
|---------|---------|
| libcurl | HTTP tracker announce, web seeds |
| pthreads | Threading (torrent engine, DHT, hash worker) |

### Optional (Switch build only)

| Library | Purpose |
|---------|---------|
| libnx | Nintendo Switch SDK |
| borealis | Switch UI framework |
| miniupnpc | NAT port mapping |

---

## License

This project is licensed under the **MIT License** — see [LICENSE](LICENSE) for details.

The vendored libutp and jech dht retain their original licenses.

---

## Credits

Developed by **TecnoNX** (https://github.com/TecnoNX)

Based on the **PipensX** BitTorrent core — a native Nintendo Switch homebrew BitTorrent download manager and streaming package installer.

- **BitTorrent protocol core** adapted from PipensX
- **DHT engine**: Juliusz Chroboczek (jech's dht, MIT)
- **μTP transport**: libutp (BitTorrent Inc., BSD-style)
- **UI framework**: borealis (Nintendo Switch homebrew)

---

*Built with ❤️ for the homebrew and open-source community.*
