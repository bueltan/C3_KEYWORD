#pragma once

#include <Arduino.h>

namespace KeywordConfig
{
    // ============================================================
    // Audio format received from C3_EARS
    // ============================================================

    static constexpr uint32_t SAMPLE_RATE_HZ = 16000;
    static constexpr uint32_t FRAME_MS = 20;

    static constexpr size_t FRAME_SAMPLES =
        (SAMPLE_RATE_HZ * FRAME_MS) / 1000; // 320

    static constexpr size_t FRAME_BYTES =
        FRAME_SAMPLES * sizeof(int16_t); // 640

    // ============================================================
    // Keyword model window
    // ============================================================

    static constexpr size_t MODEL_WINDOW_SAMPLES = 16000; // 1 s

    // Two seconds of rolling PCM. This is enough for the 1 s model
    // window plus pre/post audio while an event is being closed.
    static constexpr size_t AUDIO_RING_SAMPLES = 32000;

    // ============================================================
    // RMS / VAD
    // ============================================================

    // Live microphone default: fixed RMS thresholds, independent of startup noise.
    static constexpr bool FIXED_VAD_THRESHOLDS = true;
    static constexpr float FIXED_HIGH_THRESHOLD = 1000.0f;
    static constexpr float FIXED_LOW_THRESHOLD = 800.0f;
    static_assert(FIXED_LOW_THRESHOLD > 0 &&
        FIXED_LOW_THRESHOLD < FIXED_HIGH_THRESHOLD && FIXED_HIGH_THRESHOLD < 32768,
        "VAD thresholds must be ordered within PCM16 RMS range");

    // Optional legacy calibration when FIXED_VAD_THRESHOLDS is false.
    static constexpr uint32_t NOISE_CALIBRATION_MS = 500;

    static constexpr size_t NOISE_CALIBRATION_BLOCKS =
        (NOISE_CALIBRATION_MS + FRAME_MS - 1) / FRAME_MS; // 25

    static constexpr uint32_t ONSET_MS = 60;
    static constexpr size_t ONSET_BLOCKS =
        (ONSET_MS + FRAME_MS - 1) / FRAME_MS; // 3

    static constexpr uint32_t QUIET_MS = 260;
    static constexpr size_t QUIET_BLOCKS =
        (QUIET_MS + FRAME_MS - 1) / FRAME_MS; // 13

    static constexpr uint32_t PREBUFFER_MS = 200;
    static constexpr size_t PREBUFFER_SAMPLES =
        (SAMPLE_RATE_HZ * PREBUFFER_MS) / 1000;

    static constexpr uint32_t MIN_ACTIVITY_MS = 150;
    static constexpr size_t MIN_ACTIVITY_SAMPLES =
        (SAMPLE_RATE_HZ * MIN_ACTIVITY_MS) / 1000;

    static constexpr uint32_t MAX_ACTIVITY_MS = 960;
    static constexpr size_t MAX_ACTIVITY_SAMPLES =
        (SAMPLE_RATE_HZ * MAX_ACTIVITY_MS) / 1000;

    // Long events use the same 1 s model window with bounded overlap.
    static constexpr size_t LONG_WINDOW_STRIDE_SAMPLES = SAMPLE_RATE_HZ * 750 / 1000;

    static constexpr float HIGH_THRESHOLD_FACTOR = 3.0f;
    static constexpr float LOW_THRESHOLD_FACTOR = 1.8f;

    static constexpr float MIN_HIGH_THRESHOLD = 100.0f;
    static constexpr float MIN_LOW_THRESHOLD = 60.0f;

    // ============================================================
    // UART link with C3_EARS
    // ============================================================

    static constexpr uint32_t UART_BAUD = 921600;

    // Physical wiring:
    // EARS TX GPIO4  ---> KEYWORD RX GPIO5
    // EARS RX GPIO5  <--- KEYWORD TX GPIO4
    static constexpr int UART_RX_PIN = 5;
    static constexpr int UART_TX_PIN = 4;

    static constexpr size_t UART_RX_BUFFER_BYTES = 4096;
    static constexpr uint32_t UART_READ_TIMEOUT_MS = 100;

    // Audio continues arriving while Edge Impulse is running.
    // One second of queued audio gives ~50 frames of headroom.
    static constexpr uint32_t AUDIO_QUEUE_MS = 1000;
    static constexpr size_t AUDIO_QUEUE_FRAMES =
        (AUDIO_QUEUE_MS + FRAME_MS - 1) / FRAME_MS; // 50

    static constexpr uint32_t UART_TASK_STACK_BYTES = 4096;
    static constexpr UBaseType_t UART_TASK_PRIORITY = 2;
}

static_assert(KeywordConfig::FRAME_SAMPLES == 320,
              "C3_KEYWORD expects 320 PCM samples per 20 ms frame");

static_assert(KeywordConfig::FRAME_BYTES == 640,
              "C3_KEYWORD expects 640 PCM bytes per 20 ms frame");
