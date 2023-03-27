#ifndef SACONFIG_H
#define SACONFIG_H

#include "llvm/Support/FileSystem.h"

#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <fstream>

//
// Configurations for compilation.
//
//#define SOUND_MODE 1
#define MLTA_FOR_INDIRECT_CALL

// Skip functions with more blocks to avoid scalability issues
#define MAX_BLOCKS_SUPPORT 500

#endif
