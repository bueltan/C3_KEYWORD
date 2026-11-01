#include "keyword_model.h"

#include <cstring>

#include "keyword_config.h"
#include "keyword_commands.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

namespace
{
    const int16_t *currentWindow = nullptr;
    size_t currentWindowSamples = 0;

    int readWindow(size_t offset, size_t length, float *out)
    {
        if (currentWindow == nullptr || out == nullptr)
            return -1;

        if (offset > currentWindowSamples ||
            length > currentWindowSamples - offset)
        {
            return -1;
        }

        for (size_t i = 0; i < length; ++i)
            out[i] = currentWindow[offset + i];

        return 0;
    }
}

static_assert(
    EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE == KeywordConfig::MODEL_WINDOW_SAMPLES,
    "Edge Impulse model must use a 1-second / 16000-sample window");

static_assert(
    EI_CLASSIFIER_FREQUENCY == KeywordConfig::SAMPLE_RATE_HZ,
    "Edge Impulse model must use 16 kHz audio");

static_assert(
    EI_CLASSIFIER_LABEL_COUNT > 0,
    "Edge Impulse model must expose at least one label");

bool keywordModelBegin()
{
    Serial.printf(
        "[MODEL] Edge Impulse deploy version=%u | labels=%u | window=%u | fs=%u\n",
        unsigned(EI_CLASSIFIER_PROJECT_DEPLOY_VERSION),
        unsigned(EI_CLASSIFIER_LABEL_COUNT),
        unsigned(KeywordConfig::MODEL_WINDOW_SAMPLES),
        unsigned(KeywordConfig::SAMPLE_RATE_HZ));

    for (const auto &rule : KeywordCommands::RULES)
    {
        bool found = false;
        for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; ++i)
            if (strcmp(rule.label, ei_classifier_inferencing_categories[i]) == 0)
                found = true;
        if (!found)
        {
            Serial.printf("[MODEL] ERROR missing configured label=%s\n", rule.label);
            return false;
        }
    }

    return true;
}

KeywordResult keywordModelRun(
    const int16_t *samples,
    size_t sampleCount)
{
    KeywordResult output{};
    output.ok = false;
    output.label = nullptr;
    output.confidence = 0.0f;
    output.inferenceMs = 0.0f;
    output.errorCode = 0;

    if (samples == nullptr ||
        sampleCount != KeywordConfig::MODEL_WINDOW_SAMPLES)
    {
        output.errorCode = -1;
        return output;
    }

    currentWindow = samples;
    currentWindowSamples = sampleCount;

    signal_t signal{};
    signal.total_length = sampleCount;
    signal.get_data = readWindow;

    ei_impulse_result_t result{};

    const uint32_t beginUs = micros();

    const EI_IMPULSE_ERROR err = run_classifier(
        &signal,
        &result,
        false);

    output.inferenceMs =
        (micros() - beginUs) / 1000.0f;

    currentWindow = nullptr;
    currentWindowSamples = 0;

    if (err != EI_IMPULSE_OK)
    {
        output.errorCode = static_cast<int>(err);

        Serial.printf(
            "[MODEL] ERROR run_classifier=%d\n",
            output.errorCode);

        return output;
    }

    size_t best = 0;

    Serial.print("[MODEL] ");

    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; ++i)
    {
        Serial.printf(
            "%s=%.5f ",
            result.classification[i].label,
            result.classification[i].value);

        if (result.classification[i].value >
            result.classification[best].value)
        {
            best = i;
        }
    }

    Serial.println();

    output.ok = true;
    output.label = result.classification[best].label;
    output.confidence = result.classification[best].value;
    output.errorCode = 0;

    Serial.printf(
        "[MODEL] best=%s confidence=%.5f inference=%.2f ms\n",
        output.label,
        output.confidence,
        output.inferenceMs);

    return output;
}
