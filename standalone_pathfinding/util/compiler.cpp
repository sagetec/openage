// util/compiler.cpp
// Standalone replacement — strips the symbol-name / demangling functions
// that are only used by OpenAge's crash reporter and log system.
//
// The pathfinding library only calls ENSURE() (via misc.h) and
// the LIKELY/UNLIKELY branch hints (macros in compiler.h).
// Neither of those touch demangle() or symbol_name().
//
// If you later need demangling support, copy the original file from
// libopenage/util/compiler.cpp and add util/strings.h (Python-generated).

#include "compiler.h"

#include <string>

namespace openage {
namespace util {

std::string demangle(const char *symbol) {
    // Minimal stub — just echo the mangled name.
    // In the standalone build we never call this from pathfinding code.
    return symbol ? std::string(symbol) : std::string("<null>");
}

std::string addr_to_string(const void *addr) {
    // Minimal stub — format pointer as hex without printf dependencies.
    char buf[32];
    std::snprintf(buf, sizeof(buf), "[%p]", addr);
    return buf;
}

std::string symbol_name(const void * /*addr*/,
                        bool /*require_exact_addr*/,
                        bool /*no_pure_addrs*/) {
    return "<symbol_name unavailable in standalone build>";
}

bool is_symbol(const void * /*addr*/) {
    return false;
}

} // namespace util
} // namespace openage
