// stubs/error/error.h
// Stub: replaces openage's Error/MSG system with a simple assert.
// Include this when building without the openage error/log subsystem.
#pragma once

#include <cassert>
#include <stdexcept>
#include <string>

namespace openage {

// Minimal Error class stub
struct Error : public std::runtime_error {
    explicit Error(const std::string &msg) : std::runtime_error(msg) {}
};

// MSG macro stub — just returns the string
#define MSG(level) std::string("[" #level "] ")

} // namespace openage

// ENSURE macro: assert in debug, throw in release
// Matches the semantics used in util/misc.h and util/fixed_point.h
#ifdef NDEBUG
#   define ENSURE(cond, msg) \
        do { if (!(cond)) throw openage::Error(msg); } while (false)
#else
#   define ENSURE(cond, msg) \
        do { assert((cond) && (msg)); } while (false)
#endif
