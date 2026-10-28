# Changelog

## 2026-10-28

- Require minimum voice activity duration (MIN_ACTIVITY_MS = 150) in VAD to discard brief clicks and impulsive noise without model inference.
- Use fixed VAD thresholds 1000/800 for live microphone capture.
- Analyze long activity in overlapping one-second windows and inspect its tail instead of discarding the event.
- Preserve existing PCM buffers, model and confidence threshold; extend VAD regression coverage.

## 2026-10-27

- Default live-audio VAD to fixed RMS thresholds (1000/600); retain optional automatic calibration.
- Preserve onset/quiet timing, model confidence and stream-reset thresholds.
