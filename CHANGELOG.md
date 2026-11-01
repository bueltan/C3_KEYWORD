# Changelog

## 2026-10-31 — referencia de guarda CORE

- Actualizar referencia de guarda posterior de CORE a 150 ms. Sin cambios
  de firmware/modelo KEYWORD; el cambio de bloqueo PCM pertenece a CORE.

## 2026-10-31 — PIUU v1 y standby por voz

- Reemplazar Wall-E v12 por PIUU proyecto 1153075 deploy v1 con sus seis clases;
  conservar detector/PCM/MFCC y optimizaciones locales, aplicando parche sin fuzz al
  SDK nuevo y registrando hashes/respaldo y revisión de diferencias upstream.
- Mapear piuu/0.90 a WAKE y piuu_stop/0.90 a STANDBY 0x07; dejar luz/linterna
  sin acciones. Comprobar etiquetas configuradas al arrancar y aceptar las seis
  etiquetas revisadas en el instalador. No añadir arbitraje ni debounce.
- Documentar actualización coordinada con CORE y ensayo acústico pendiente.
- Validar en copia aislada build esp32-c3-uart, reglas, VAD versionado y workflow;
  comparar 40 ventanas MFCC idénticas bit a bit contra el SDK original. Sin Upload.

## 2026-10-28

- Require minimum voice activity duration (MIN_ACTIVITY_MS = 150) in VAD to discard brief clicks and impulsive noise without model inference.
- Use fixed VAD thresholds 1000/800 for live microphone capture.
- Analyze long activity in overlapping one-second windows and inspect its tail instead of discarding the event.
- Preserve existing PCM buffers, model and confidence threshold; extend VAD regression coverage.

## 2026-10-27

- Default live-audio VAD to fixed RMS thresholds (1000/600); retain optional automatic calibration.
- Preserve onset/quiet timing, model confidence and stream-reset thresholds.
