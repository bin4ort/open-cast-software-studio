# OCS Studio — Utility Module Standard

<!-- SPDX-License-Identifier: GPL-3.0-only -->

## Purpose

Core utilities: JSON parsing/encoding with full Unicode support.

## Module Root

`src/util/`

## Source Files

- `json.h` / `json.cpp` — JSON encoder/decoder, UTF-8/UTF-16 surrogate handling, control char escaping

## Dependencies

- Standard library only (string, vector, iostream)

## Public API

| Function | Namespace | Purpose |
|----------|-----------|---------|
| `encode(input)` | `ocs::json` | C++ string → JSON string |
| `decode(input)` | `ocs::json` | JSON string → C++ string |
| `escape(input)` | `ocs::json` | Escape control chars as \uXXXX |

## UTF-8 Handling

- Surrogate pairs encoded as `\u{high}\u{low}`
- Orphan surrogates escaped individually
- Boundary condition: `i_ + 5 <= s_.size()`

## Test File

`src/util/test/test.sh`

## License

GPL-3.0-only
