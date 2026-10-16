# OCS Studio — Anonymize Module

<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 Open Cast Software Studio contributors -->

## Overview

The `anonymize` module provides PII (Personally Identifiable Information) detection and redaction for audio transcripts. It supports regex-based and rule-based anonymization of emails, phone numbers, URLs, and personal names.

## Components

| File | Description |
|------|-------------|
| `redactor.h/cpp` | Text anonymization engine with synthetic name replacement |

## Features

- **Email redaction** — Detects and replaces email addresses
- **Phone number redaction** — Detects international and local formats
- **URL redaction** — Detects HTTP/HTTPS/mailto/ftp URLs
- **Name redaction** — Detects capitalized words (potential names) and replaces with `[REDACTED]` or synthetic placeholders
- **Synthetic names** — Optional deterministic name substitution (same input → same output)

## API

```cpp
namespace ocs::redact {

struct RedactOptions {
    bool redact_emails = true;
    bool redact_phones = true;
    bool redact_urls = true;
    bool redact_names = true;
    std::string replacement = "[REDACTED]";
    bool use_synthetic_names = false;
};

std::string synthetic_name(const std::string& original);
std::string redact_text(const std::string& text, const RedactOptions& opts,
                        std::vector<std::string>& names_found);
size_t flag_segments(std::vector<Segment>& segments, const RedactOptions& opts);

}  // namespace ocs::redact
```

## Synthetic Name Algorithm

When `use_synthetic_names` is enabled, `synthetic_name()` produces a deterministic placeholder that:
- Maps the same input name to the same output consistently
- Preserves gender hints (if detectable from the original name)
- Is plausible but clearly synthetic (e.g., "Alice" → "Alisa")

## Testing

```bash
bash src/anonymize/test/test.sh
```

## Dependencies

- `common.h` (Segment struct definition)
- Standard library only (regex, string, vector)

## License

GPL-3.0-only. See [../../LICENSE](../../LICENSE) for details.
