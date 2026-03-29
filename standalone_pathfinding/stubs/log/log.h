// stubs/log/log.h
// Standalone stub for openage's log system.
//
// OpenAge uses:
//   log::log(INFO << "message " << value);
//   log::log(DBG << "message");
//   log::log(WARN << "message");
//
// INFO/DBG/WARN/ERR return a MessageBuilder (from error/error.h).
// log::log() accepts a MessageBuilder and optionally prints it.
#pragma once

#include "error/error.h"  // MessageBuilder, ERR

// Log level macros — return a MessageBuilder that supports << chaining.
// ERR is already defined in error/error.h; we define the rest here.
#ifndef INFO
#   define INFO  ::openage::MessageBuilder("[INFO]  ")
#endif
#ifndef WARN
#   define WARN  ::openage::MessageBuilder("[WARN]  ")
#endif
#ifndef DBG
#   define DBG   ::openage::MessageBuilder("[DBG]   ")
#endif
#ifndef SPAM
#   define SPAM  ::openage::MessageBuilder("[SPAM]  ")
#endif


namespace openage {
namespace log {

// Accepts a MessageBuilder chain, e.g. log::log(INFO << "hello " << x)
inline void log(const ::openage::MessageBuilder &mb) {
    // Uncomment to enable logging to stdout:
    // std::puts(mb.str().c_str());
    (void)mb;
}

// Overload for plain strings (rare, but present in some files)
inline void log(const std::string &msg) {
    (void)msg;
}

} // namespace log
} // namespace openage
