#pragma once

#include <Arduino.h>

struct KeywordResult
{
    bool ok;
    const char *label;
    float confidence;
    float inferenceMs;
    int errorCode;
};

// Validates the compiled Edge Impulse model configuration and prints
// basic model information.
bool keywordModelBegin();

// Runs one inference over a complete 1-second PCM16 window.
KeywordResult keywordModelRun(
    const int16_t *samples,
    size_t sampleCount);
