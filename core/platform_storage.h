#pragma once
#include <stddef.h>
#include <stdint.h>
#include "metainfo.h"

// Storage types are already defined in platform/../platform/storage.h
// This file just provides the implementation for PC platform - no duplicate definitions

typedef struct storage storage_t;

// Include the platform storage header which has all the type definitions
// We just re-declare the functions here for the core to use
#include "../platform/storage.h"
