# C3_KEYWORD v1

First modular version of the keyword-spotting firmware.

## Responsibilities

```text
C3_EARS
   |
   | PCM!  PCM16 mono 16 kHz / 20 ms
   v
C3_KEYWORD
   |
   |-- ears_link       UART + framing + sequence + queue
   |-- audio_detector  RMS/VAD + 500 ms initial noise calibration
   |-- keyword_model   Edge Impulse inference
   |-- event_mapper    model label -> stable system event
   |
   | EVT!
   v
C3_EARS
```

## Files

```text
include/
├── keyword_config.h
├── ears_protocol.h
├── ears_link.h
├── audio_detector.h
├── keyword_model.h
└── event_mapper.h

src/
├── main.cpp
├── ears_link.cpp
├── audio_detector.cpp
├── keyword_model.cpp
└── event_mapper.cpp
```

## Important: keep the Edge Impulse export

This package does **not** include the generated Edge Impulse SDK/model files.
Keep the model directories already copied from `wall-e-inferencia`, including
the files that provide:

```cpp
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
```

The current compiled model is expected to be:

- 16 kHz
- 16000 samples / 1 second

Compilation fails intentionally if those assumptions no longer match the
retrained model. When the future model changes its input window, update the
architecture deliberately instead of silently accepting a mismatch.

## UART

```text
EARS TX GPIO4  ---> KEYWORD RX GPIO5
EARS RX GPIO5  <--- KEYWORD TX GPIO4
baud: 921600
```

### PCM! EARS -> KEYWORD

```text
4 bytes   "PCM!"
4 bytes   sequence u32
2 bytes   payloadBytes u16
640 bytes PCM16 mono
```

### EVT! KEYWORD -> EARS

```text
4 bytes   "EVT!"
1 byte    version
1 byte    event
2 bytes   payloadBytes
N bytes   payload (0..32)
```

v1 sends events without payload.

## Model labels vs system events

Only `event_mapper.cpp` knows model label names.

Current mapping:

```text
"walle" >= 0.80 -> WAKE_WORD
```

When the model is retrained, edit the mapping table there. Do not put model
labels into `ears_link` or `audio_detector`.

## Noise calibration

v1 intentionally keeps calibration simple:

```text
500 ms = 25 frames
RMS p90 -> backgroundNoise
high = max(100, backgroundNoise * 3.0)
low  = max( 60, backgroundNoise * 1.8)
```

It is a one-shot bootstrap. Dynamic background-noise adaptation is left for a
later version.

## Sequence/discontinuity

`ears_link` is the single owner of PCM sequence validation. A source gap,
UART parse problem, or queue overflow marks the next delivered `AudioFrame`
as discontinuous. `audio_detector` then clears only its continuity-dependent
state.

This removes the old duplicated sequence checking from both capture and
`loop()`.

## EVT! IDs

This v1 uses:

```text
NONE        = 0
WAKE_WORD   = 1
LIGHTS_WORD = 2
STOP_WORD   = 3
MUTE_WORD   = 4
version     = 1
```

These values **must match** `control_protocol.h` in C3_EARS. If your current
EARS header uses different numeric values, change only `include/ears_protocol.h`
before testing event transmission.


## VAD para INMP441 — 2026-10-27

El modo predeterminado usa umbrales RMS fijos: inicio >1000, mantenimiento >800.
Se configuran con `FIXED_VAD_THRESHOLDS`, `FIXED_HIGH_THRESHOLD` y
`FIXED_LOW_THRESHOLD` en `include/keyword_config.h`. No estima ruido al iniciar;
el log indica `mode=fixed high=1000.00 low=800.00`, `calibration=0 ms`.
Conserva inicio de 60 ms, cierre de 260 ms y confianza de WALLE 0,80.
El modo automático anterior sigue disponible poniendo `FIXED_VAD_THRESHOLDS=false`.

La captura de silencio `20261027T123658.212995Z` mostró media absoluta estable
190–499 y picos 1060–1639 tras estabilizarse; esas métricas no equivalen a RMS.
Los valores fijos son provisionales y requieren validar silencio y voz en hardware.
El transitorio inicial ya no altera los umbrales, pero puede generar actividad
VAD: este cambio no añade un período de descarte de audio al arrancar.

## Actividades largas — 2026-10-28

El firmware utiliza VAD fijo 1000/800 (RMS). Confirmar ambos valores en el log
de arranque: la captura anterior tenía 1000/400 y no coincidía con el código.
Las actividades cortas conservan su ventana original centrada. Al superar
960 ms, las actividades largas producen ventanas de PCM original de un segundo
con avance de 750 ms, sin esperar a que termine el evento. Al cerrar, se analiza
también el último segundo si agrega cobertura. Ya no se descarta el evento
entero por exceder 960 ms o dos segundos.

Se reutilizan el anillo de dos segundos y el buffer de inferencia existentes;
no se agregan tareas ni buffers de audio. Modelo v12, MFCC, confianza 0.80 y
ganancia EARS permanecen iguales. Con inferencias de ~370 ms, una actividad
continua implica aproximadamente 49% de tiempo de inferencia, más el resto del
procesamiento; el cierre puede añadir una inferencia de cola. Vigilar `queue`,
`dropped` y tiempos reales. Las ventanas superpuestas pueden detectar la misma
palabra más de una vez; no se añadió un bloqueo de reactivación. El ruido largo
antes omitido ahora se clasifica, por lo que hay que evaluar falsas activaciones.

Prueba: diez “WALLE” separados por tres segundos, después palabras negativas y
un tramo de ruido/silencio. Confirmar `long_activity=rolling` en el arranque.
Esto mejora cobertura temporal; no garantiza que el modelo reconozca todas las
ventanas. El usuario carga solamente KEYWORD (`esp32-c3-uart`).
