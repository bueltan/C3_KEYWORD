#include <Arduino.h>

#include "audio_detector.h"
#include "ears_link.h"
#include "event_mapper.h"
#include "keyword_config.h"
#include "keyword_model.h"

// 16000 int16 samples = 32 KB. Keep this out of loop() stack.
static int16_t inferenceWindow[KeywordConfig::MODEL_WINDOW_SAMPLES];

void setup()
{
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("============================================");
    Serial.println(" C3_KEYWORD v1");
    Serial.println(" PCM! -> RMS/VAD -> Edge Impulse -> EVT!");
    Serial.println("============================================");

    Serial.printf(
        "[BOOT] audio=%lu Hz mono PCM16 | frame=%lu ms | samples=%u | bytes=%u\n",
        (unsigned long)KeywordConfig::SAMPLE_RATE_HZ,
        (unsigned long)KeywordConfig::FRAME_MS,
        unsigned(KeywordConfig::FRAME_SAMPLES),
        unsigned(KeywordConfig::FRAME_BYTES));

    audioDetectorBegin();

    if (!keywordModelBegin())
    {
        Serial.println("[BOOT] ERROR inicializando keyword model");
        return;
    }

    if (!earsLinkBegin())
    {
        Serial.println("[BOOT] ERROR inicializando EARS link");
        return;
    }

    Serial.println("[BOOT] listo");
}

void loop()
{
    AudioFrame frame{};

    if (!earsLinkReceive(frame, 100))
        return;

    const bool windowReady = audioDetectorProcess(
        frame.samples,
        KeywordConfig::FRAME_SAMPLES,
        frame.discontinuity,
        inferenceWindow,
        KeywordConfig::MODEL_WINDOW_SAMPLES);

    if (!windowReady)
        return;

    const KeywordResult result = keywordModelRun(
        inferenceWindow,
        KeywordConfig::MODEL_WINDOW_SAMPLES);

    if (!result.ok)
        return;

    const EventMappingResult mapped = eventMapperMap(result);

    if (!mapped.knownLabel)
    {
        Serial.printf(
            "[MAP] label=%s no genera evento\n",
            result.label != nullptr ? result.label : "<null>");

        return;
    }

    if (!mapped.emit)
    {
        Serial.printf(
            "[MAP] label=%s confidence=%.5f < threshold=%.2f\n",
            result.label,
            result.confidence,
            mapped.threshold);

        return;
    }

    const bool sent = earsLinkSendEvent(mapped.event);

    Serial.printf(
        "[EVENT] %s | label=%s | confidence=%.5f | tx=%s | queue=%u | heap=%u\n",
        eventMapperEventName(mapped.event),
        result.label,
        result.confidence,
        sent ? "OK" : "ERROR",
        unsigned(earsLinkQueueDepth()),
        ESP.getFreeHeap());
}
