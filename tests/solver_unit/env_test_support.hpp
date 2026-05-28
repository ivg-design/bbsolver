#pragma once

// Portable environment-variable save / set / unset / restore for the
// solver unit tests. POSIX builds use the standard `setenv` /
// `unsetenv` / `getenv` triple; MSVC builds use `_putenv_s` (with the
// empty-value idiom for unset) and `_dupenv_s` (to read env vars
// without tripping C4996 deprecation warnings on `std::getenv`).
//
// Test-only header — not consumed by any solver source. Multiple unit
// tests previously carried bespoke `ScopedEnv` / `EnvVarGuard` classes;
// W5 consolidates them here so the Windows CI build (PFF/W2) does not
// fail on `setenv`/`unsetenv` (which are POSIX-only) on MSVC.
//
// The two consumer shapes both collapse to one class:
//
//   ScopedEnv guard(name);          // capture + immediately unset
//   guard.Set("value");             // mutate within the test
//   guard.Clear();                  // explicit unset
//                                   // ~guard restores the captured value
//
//   ScopedEnv guard(name, "value"); // capture + immediately set
//   ScopedEnv guard(name, nullptr); // capture + immediately unset (== name-only ctor)
//                                   // ~guard restores the captured value

#include <cstddef>
#include <cstdlib>
#if __has_include(<_stdlib.h>)
#include <_stdlib.h>
#else
#include <stdlib.h>
#endif
#include <string>
#include <utility>

namespace bbsolver {
namespace test_support {

// Read an environment variable portably. Returns {had_value, value}.
// On MSVC, uses _dupenv_s to avoid the C4996 deprecation warning that
// std::getenv triggers under modern Visual Studio.
inline std::pair<bool, std::string> GetEnvOpt(const char* name) {
#if defined(_WIN32)
  char* buffer = nullptr;
  std::size_t length = 0;
  if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr) {
    if (buffer != nullptr) {
      std::free(buffer);
    }
    return {false, std::string{}};
  }
  std::string out(buffer);
  std::free(buffer);
  return {true, std::move(out)};
#else
  const char* current = std::getenv(name);
  if (current == nullptr) {
    return {false, std::string{}};
  }
  return {true, std::string{current}};
#endif
}

// Set an environment variable to `value`. `value` must be a
// null-terminated string. Overwrites any existing binding.
inline void SetEnv(const char* name, const char* value) {
#if defined(_WIN32)
  // _putenv_s overwrites by default; success returns 0.
  (void)_putenv_s(name, value);
#else
  // setenv with overwrite=1.
  (void)::setenv(name, value, 1);
#endif
}

// Unset an environment variable. On MSVC, passing an empty string to
// `_putenv_s` removes the binding (per the Microsoft docs).
inline void UnsetEnv(const char* name) {
#if defined(_WIN32)
  (void)_putenv_s(name, "");
#else
  (void)::unsetenv(name);
#endif
}

// Scope guard: capture the variable's current value on construction,
// let the test mutate it freely via Set/Clear, then restore the
// captured value (or unset, if originally unset) on destruction.
//
// Constructors:
//   ScopedEnv(name)              -> capture + unset
//   ScopedEnv(name, "value")     -> capture + set to "value"
//   ScopedEnv(name, nullptr)     -> capture + unset (same as 1-arg form)
class ScopedEnv {
 public:
  explicit ScopedEnv(std::string name)
: name_(std::move(name)), had_value_(false) {
    auto current = GetEnvOpt(name_.c_str());
    if (current.first) {
      had_value_ = true;
      saved_ = std::move(current.second);
    }
    UnsetEnv(name_.c_str());
  }

  ScopedEnv(std::string name, const char* value)
: ScopedEnv(std::move(name)) {
    if (value != nullptr) {
      Set(value);
    }
  }

  ScopedEnv(const ScopedEnv&) = delete;
  ScopedEnv& operator=(const ScopedEnv&) = delete;
  ScopedEnv(ScopedEnv&&) = delete;
  ScopedEnv& operator=(ScopedEnv&&) = delete;

  ~ScopedEnv() {
    if (had_value_) {
      SetEnv(name_.c_str(), saved_.c_str());
    } else {
      UnsetEnv(name_.c_str());
    }
  }

  void Set(const char* value) { SetEnv(name_.c_str(), value); }
  void Clear() { UnsetEnv(name_.c_str()); }

  const std::string& name() const { return name_; }

 private:
  std::string name_;
  bool had_value_;
  std::string saved_;
};

}  // namespace test_support
}  // namespace bbsolver
