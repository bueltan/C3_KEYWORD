#pragma once

#include <Arduino.h>

#include "keyword_config.h"

struct AudioDetectorStats
{
    uint32_t events;
    uint32_t windowsReady;
    uint32_t skipped;
    float backgroundNoise;
    float highThreshold;
    float lowThreshold;
    bool calibrated;
};

void audioDetectorBegin();

// Clears continuity-dependent state; fixed thresholds remain unchanged.
// In automatic mode, the background-noise calibration is
// preserved once completed. If continuity is lost during calibration, the
// 500 ms bootstrap is restarted.
void audioDetectorResetStream();

// Feeds one PCM frame into RMS/VAD.
// Returns true only when a complete 1-second candidate window has been
// copied to outWindow and is ready for keywordModelRun().
bool audioDetectorProcess(
    const int16_t *samples,
    size_t sampleCount,
    bool discontinuity,
    int16_t *outWindow,
    size_t outWindowSamples);

AudioDetectorStats audioDetectorGetStats();
