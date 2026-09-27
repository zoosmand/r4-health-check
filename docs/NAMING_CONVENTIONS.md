# Naming conventions

This document defines the naming and documentation style for the project-owned
Arduino C++ sources in `src/` and the host tests in `test/`.

Existing names are changed only in dedicated refactoring work, because
renaming an API can affect several modules.

## General rules

- Use English names that describe purpose rather than implementation detail.
- Spell out words unless an abbreviation is established in the hardware or
  protocol documentation.
- Keep hardware and protocol names in their canonical form: `DNS`, `HTTP`,
  `HTTPS`, `JSON`, `RA4M1`, `RTC`, `SNI`, `TLS`, `UNO R4`, `WDT`, `Wi-Fi`.
  Inside identifiers, treat them as words: `HttpLineReader`, `parseHttpStatusCode`.
- Include units in names when the type alone does not make them clear, for
  example `intervalMs`, `timeoutMs`, or `rssiDbm`.
- Use one term consistently for one concept. Prefer `begin`, `update`, `read`,
  `write`, `get`, `set`, `is`, `has`, `queue`, `save`, and `restore`.

## Files and modules

- One module is one class or one group of free functions.
- Name the files after the module in `PascalCase`, with a header and an
  implementation of the same base name: `HealthChecker.h`,
  `HealthChecker.cpp`. This matches Arduino library and sketch conventions.
- The sketch entry point is `UNO_R4_Health_Checker.ino`. Keep it limited to
  object construction, `setup()`, and `loop()`.
- Keep modules that do not need Arduino headers free of them (for example
  `TextParsing`), so they can be tested on the host.
- Header guards use the upper-case file name: `HEALTH_CHECKER_H`.

## Classes and functions

- Classes and structures use `PascalCase`: `AlarmController`, `ServiceState`.
- Member functions and free functions use `camelCase`: `queueService()`,
  `parseHttpStatusCode()`.
- Arduino lifecycle methods keep the Arduino names: `begin()` initializes
  hardware or state and starts nothing periodic; `update()` performs one
  non-blocking or bounded step of periodic work and is called from `loop()`.
- Boolean predicates begin with `is`, `has`, or `can`: `isConnected()`.
- A `get`-style accessor does not transfer ownership of returned storage
  unless its documentation says so.
- Functions and variables with file scope go in an unnamed namespace
  instead of using `static`.

## Types and enumerators

- Use `enum class` with a `PascalCase` type name. Enumerators use upper-case
  `SNAKE_CASE`: `LineReadResult::TIMED_OUT`.
- Structure members and function parameters use `camelCase`.
- Do not introduce `_t` type names. POSIX reserves many names with that suffix.

## Variables and constants

- Local variables and parameters use `camelCase`.
- Private data members use `camelCase` with a leading underscore:
  `_alarmController`. In C++ this is allowed for class members. Never use an
  underscore followed by an upper-case letter, or a leading underscore at
  global or namespace scope; those names are reserved.
- Compile-time constants use upper-case `SNAKE_CASE` and `constexpr`:
  `WATCHDOG_TIMEOUT_MS`. Prefer `constexpr` to `#define`.
- Macros are reserved for configuration that must be overridable by the
  preprocessor (for example `SECRET_API_TOKEN`). They use upper-case
  `SNAKE_CASE` and parenthesize every parameter and the complete expression.
- Variables shared with an interrupt are `volatile` and are no wider than one
  machine word, or are protected explicitly.

## Documentation

Document every public class, function, and structure where it is declared,
usually in the header. Document a private function in the header or
immediately above its definition, not in both places.

Function documentation uses this form:

```cpp
/**
  * @brief Read one CRLF- or LF-terminated line with a bounded size and time.
  * @param client (Client&) Connected client to read from.
  * @param buffer (char*) Non-null output buffer; always NUL-terminated.
  * @param capacity (size_t) Size of buffer in bytes; at least 1.
  * @param timeoutMs (unsigned long) Maximum time to wait for the line.
  * @retval (LineReadResult) Completion status.
  */
```

Use `@param` only for real parameters. Use `@retval` only for functions that
return a value. State units, ownership, lifetime, valid ranges, nullability,
blocking time, and interrupt restrictions when they matter. Obvious accessors
may use a one-line comment or none.

Structure documentation lists the purpose and meaning of every member:

```cpp
/**
  * @brief Static description of one monitored HTTPS resource.
  * @param id (const char*) Unique, URL-safe identifier used by the API.
  * @param intervalMs (unsigned long) Time between checks while healthy.
  */
```

## Compatibility notes

Adapters around imported APIs keep the upstream names of the things they wrap.
Arduino core and library symbols (`WiFi`, `WiFiSSLClient`, `FspTimer`, `WDT`),
FSP/CMSIS symbols (`R_SYSTEM`, `NVIC_SystemReset`), and linker sections
(`.noinit`) keep their required spelling.

---

&copy; 2026, Askug Ltd., Dmitry Slobodchikov
