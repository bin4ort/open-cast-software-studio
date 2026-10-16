# OCS Studio — Processing Module

<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 Open Cast Software Studio contributors -->

## Overview

The `processing` module orchestrates multi-step audio processing workflows (batch jobs). It manages job state, progress tracking, and sequential execution of DSP, transcription, anonymization, and TTS steps.

## Components

| File | Description |
|------|-------------|
| `job.h/cpp` | Batch job definition, state machine, and progress reporting |

## Job Types

| Job Type | Description |
|----------|-------------|
| `batch` | Apply voice transformation to an input file |
| `hybrid` | Transcribe → rewrite → shift → re-voice pipeline |
| `redact` | Extract transcript and redact PII |
| `preview` | Generate voice preview from example text |

## API

```cpp
namespace ocs::processing {

enum class JobType { Batch, Hybrid, Redact, Preview };
enum class JobStatus { Idle, Running, Paused, Completed, Failed };

struct JobConfig {
    JobType type;
    std::string input_path;
    std::string output_path;
    dsp::ShiftParams voice_params;
    std::string gender_target;
    bool enable_transcription;
    bool enable_redaction;
};

class Job {
public:
    Job(const JobConfig& config);
    void start();
    void pause();
    void resume();
    void cancel();

    JobStatus status() const;
    double progress() const;  // 0.0 – 1.0
    std::string error_message() const;
    void set_callback(std::function<void(double, std::string)> cb);
};

}  // namespace ocs::processing
```

## Dependencies

- `dsp::` — Voice transformation
- `transcribe::` — Whisper.cpp transcription
- `anonymize::` — PII redaction
- `llm::` — AI-powered text rewriting (optional)
- `vocoder::` — TTS re-voicing (optional)

## Progress Callback

Jobs call the callback with `(progress, message)` at each step:
- `0.0` → `0.25` — Pre-processing (file load, validation)
- `0.25` → `0.75` — Core processing (DSP, transcription, rewriting)
- `0.75` → `1.0` — Post-processing (file write, cleanup)

## Error Handling

- Invalid input files: `JobStatus::Failed` with descriptive message
- Missing dependencies (whisper, TTS): Step skipped with warning
- Backend unavailability: Falls back to offline mode where possible

## Testing

```bash
bash src/processing/test/test.sh
```

## License

GPL-3.0-only. See [../../LICENSE](../../LICENSE) for details.
