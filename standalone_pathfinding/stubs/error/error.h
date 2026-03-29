// stubs/error/error.h
// Standalone stub for openage's Error/MSG system.
//
// OpenAge uses the pattern:
//   throw Error{MSG(err) << "some text " << value};
//   throw Error{ERR << "text"};
//   ENSURE(cond, "text " << val << " more");
//
// We implement a lightweight MessageBuilder that supports operator<<
// so that all existing uses in the pathfinding sources compile unchanged.

#pragma once

#include <sstream>
#include <stdexcept>
#include <string>

namespace openage {

// ── MessageBuilder ────────────────────────────────────────────────────────────
// Accumulates a message via operator<< and converts to string on demand.
// Uses std::string internally (not ostringstream) so it is copyable/movable.
class MessageBuilder {
public:
    MessageBuilder() = default;

    explicit MessageBuilder(const char *prefix) : msg_(prefix) {}
    explicit MessageBuilder(const std::string &prefix) : msg_(prefix) {}

    // Copy and move support (needed by ENSURE and throw)
    MessageBuilder(const MessageBuilder &) = default;
    MessageBuilder(MessageBuilder &&) = default;
    MessageBuilder &operator=(const MessageBuilder &) = default;
    MessageBuilder &operator=(MessageBuilder &&) = default;

    // Stream any type using a temporary ostringstream
    template <typename T>
    MessageBuilder &operator<<(const T &value) {
        std::ostringstream ss;
        ss << value;
        msg_ += ss.str();
        return *this;
    }

    // Specialisation for string literals / char* (avoids ambiguity)
    MessageBuilder &operator<<(const char *s) {
        if (s) msg_ += s;
        return *this;
    }

    MessageBuilder &operator<<(const std::string &s) {
        msg_ += s;
        return *this;
    }

    std::string str() const { return msg_; }

private:
    std::string msg_;
};

// ── Error ─────────────────────────────────────────────────────────────────────
// Thrown by ENSURE() and explicit throw Error{MSG(err) << "..."} sites.
struct Error : public std::runtime_error {
    // From a MessageBuilder (the common case: throw Error{MSG(err) << "..."})
    explicit Error(const MessageBuilder &mb)
        : std::runtime_error(mb.str()) {}

    // From a plain string
    explicit Error(const std::string &msg)
        : std::runtime_error(msg) {}

    explicit Error(const char *msg)
        : std::runtime_error(msg ? msg : "") {}
};

} // namespace openage

// ── Macros ────────────────────────────────────────────────────────────────────

// MSG(level) — creates a MessageBuilder with a "[level] " prefix.
// Usage: throw Error{MSG(err) << "some message"};
#define MSG(level) ::openage::MessageBuilder("[" #level "] ")

// ERR — shorthand for MSG(err), used in some OpenAge files.
#define ERR ::openage::MessageBuilder("[error] ")

// ENSURE(cond, msg_expr) — assertion with error message.
// msg_expr can be:
//   - A string literal:        ENSURE(x, "bad value")
//   - A MessageBuilder chain:  ENSURE(x, MSG(err) << "bad " << val)
//   - A bare stream chain:     ENSURE(x, "bad " << val)
//
// For bare-stream chains, we build via a temp MessageBuilder.
#define ENSURE(cond, msg_expr) \
    do { \
        if (!(cond)) { \
            ::openage::MessageBuilder _ensure_builder_; \
            _ensure_builder_ << msg_expr; \
            throw ::openage::Error(_ensure_builder_); \
        } \
    } while (false)
