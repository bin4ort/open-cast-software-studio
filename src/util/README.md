# OCS Studio — Utility Module

<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 Open Cast Software Studio contributors -->

## Overview

The `util` module provides core utility functions shared across all OCS Studio modules: JSON parsing/escaping, shell quoting, and common helpers.

## Components

| File | Description |
|------|-------------|
| `json.h/cpp` | JSON parser/encoder with proper UTF-8/UTF-16 surrogate pair handling |

## JSON Module

The JSON module handles encoding/decoding with full Unicode support:

```cpp
namespace ocs::json {

// Encode a C++ string to a JSON string value (with escaping)
std::string encode(const std::string& input);

// Decode a JSON string value to a C++ string
std::string decode(const std::string& input);

// Escape control characters (0x00-0x1F) as \\uXXXX
// Also handles quotes, backslashes, and common escapes (\\t, \\n, \\r)
std::string escape(const std::string& input);

}  // namespace ocs::json
```

## UTF-8 / UTF-16 Handling

- Surrogate pairs (U+10000+) are properly encoded as `\u{high}\u{low}`
- Orphan surrogates are detected and escaped individually
- Boundary conditions handled (i_ + 5 < s_.size() vs <=)

## Control Character Escaping

All control characters (0x00-0x1F) are escaped as `\uXXXX`:
- `\t` → `\u0009`
- `\n` → `\u000A`
- `\r` → `\u000D`
- Null bytes → `\u0000`
- Device control codes → `\uXXXX`

## Testing

```bash
bash src/util/test/test.sh
```

## Dependencies

- Standard library only (string, vector, iostream for error output)

## License

GPL-3.0-only. See [../../LICENSE](../../LICENSE) for details.
