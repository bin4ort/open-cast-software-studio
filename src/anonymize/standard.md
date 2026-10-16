# OCS Studio — Anonymize Module Standard

<!-- SPDX-License-Identifier: GPL-3.0-only -->

## Purpose

Detect and redact PII (emails, phones, URLs, names) from transcript text.

## Module Root

`src/anonymize/`

## Source Files

- `redactor.h` / `redactor.cpp` — Text anonymization with synthetic name replacement

## Dependencies

- `common.h` (Segment struct)
- Standard library only (regex, string, vector)

## Public API

| Function | Namespace | Purpose |
|----------|-----------|---------|
| `synthetic_name(original)` | `ocs::redact` | Deterministic name placeholder |
| `redact_text(text, opts, names_found)` | `ocs::redact` | Full text redaction |
| `flag_segments(segments, opts)` | `ocs::redact` | Flag segments with PII |

## Test File

`src/anonymize/test/test.sh`

## License

GPL-3.0-only
