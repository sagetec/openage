// stubs/log/log.h
// Stub: replaces openage's logging system with a no-op or simple printf.
#pragma once

#include <cstdio>
#include <string>

// Log macros used in pathfinding/ and util/
// In openage, log::log(INFO << "msg") writes to the logging subsystem.
// Here we simply print to stdout (or suppress entirely).

#define INFO  std::string("[INFO] ")
#define WARN  std::string("[WARN] ")
#define ERR   std::string("[ERR]  ")
#define DBG   std::string("[DBG]  ")

namespace openage::log {

inline void log(const std::string &msg) {
    // Uncomment to enable logging:
    // std::puts(msg.c_str());
    (void)msg;
}

} // namespace openage::log
