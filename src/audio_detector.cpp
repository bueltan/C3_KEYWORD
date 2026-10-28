#include "audio_detector.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    int16_t audioRing[KeywordConfig::AUDIO_RING_SAMPLES];

    size_t ringValid = 0;
    uint64_t streamSamples = 0;

    bool pending = false;
    uint64_t pendingStart = 0;
    uint32_t pendingEventNumber = 0;

    bool active = false;
    bool longActivity = false;
    uint64_t lastLongStart = 0;
    size_t eventSize = 0;
    size_t lastActive = 0;
    uint64_t eventStart = 0;
    uint64_t eventVoiceStart = 0;

    size_t onset = 0;
    size_t quiet = 0;

    float calibration[KeywordConfig::NOISE_CALIBRATION_BLOCKS];
    size_t calibrationCount = 0;
    bool calibrated = false;

    float backgroundNoise = 0.0f;
    float highThreshold = KeywordConfig::MIN_HIGH_THRESHOLD;
    float lowThreshold = KeywordConfig::MIN_LOW_THRESHOLD;

    uint32_t events = 0;
    uint32_t windowsReady = 0;
    uint32_t skipped = 0;

    float calculateRms(const int16_t *samples)
    {
        double sum = 0.0;

        for (size_t i = 0; i < KeywordConfig::FRAME_SAMPLES; ++i)
        {
            const double x = samples[i];
            sum += x * x;
        }

        return sqrt(sum / KeywordConfig::FRAME_SAMPLES);
    }

    void clearTemporalState()
    {
        ringValid = 0;
        streamSamples = 0;

        pending = false;
        pendingStart = 0;
        pendingEventNumber = 0;

        active = false;
        longActivity = false;lastLongStart=0;
        eventSize = 0;
        lastActive = 0;
        eventStart = 0;
        eventVoiceStart = 0;

        onset = 0;
        quiet = 0;
    }

    bool copyPendingWindow(
        int16_t *outWindow,
        size_t outWindowSamples)
    {
        if (!pending)
            return false;

        if (outWindow == nullptr ||
            outWindowSamples < KeywordConfig::MODEL_WINDOW_SAMPLES)
        {
            ++skipped;
            pending = false;
            Serial.println("[VAD] OMITIDO: buffer de salida invalido");
            return false;
        }

        if (streamSamples <
            pendingStart + KeywordConfig::MODEL_WINDOW_SAMPLES)
        {
            return false;
        }

        const uint64_t oldest = streamSamples - ringValid;

        if (pendingStart < oldest)
        {
            ++skipped;
            pending = false;

            Serial.println(
                "[VAD] OMITIDO: ventana fuera del historial disponible");

            return false;
        }

        for (size_t i = 0;
             i < KeywordConfig::MODEL_WINDOW_SAMPLES;
             ++i)
        {
            outWindow[i] = audioRing[
                (pendingStart + i) % KeywordConfig::AUDIO_RING_SAMPLES];
        }

        Serial.printf(
            "[VAD] VENTANA evento=%lu inicio=%.3f s fin=%.3f s\n",
            (unsigned long)pendingEventNumber,
            pendingStart / double(KeywordConfig::SAMPLE_RATE_HZ),
            (pendingStart + KeywordConfig::MODEL_WINDOW_SAMPLES) /
                double(KeywordConfig::SAMPLE_RATE_HZ));

        pending = false;
        ++windowsReady;
        return true;
    }

    void finishCalibration()
    {
        std::sort(
            calibration,
            calibration + KeywordConfig::NOISE_CALIBRATION_BLOCKS);

        size_t percentileIndex =
            (KeywordConfig::NOISE_CALIBRATION_BLOCKS * 90) / 100;

        if (percentileIndex >= KeywordConfig::NOISE_CALIBRATION_BLOCKS)
            percentileIndex = KeywordConfig::NOISE_CALIBRATION_BLOCKS - 1;

        backgroundNoise = calibration[percentileIndex];

        highThreshold = std::max(
            KeywordConfig::MIN_HIGH_THRESHOLD,
            backgroundNoise * KeywordConfig::HIGH_THRESHOLD_FACTOR);

        lowThreshold = std::max(
            KeywordConfig::MIN_LOW_THRESHOLD,
            backgroundNoise * KeywordConfig::LOW_THRESHOLD_FACTOR);

        calibrated = true;

        Serial.printf(
            "[VAD] CALIBRADO %u ms | background p90=%.2f | high=%.2f | low=%.2f\n",
            unsigned(KeywordConfig::NOISE_CALIBRATION_MS),
            backgroundNoise,
            highThreshold,
            lowThreshold);
    }

    bool closeEvent(
        int16_t *outWindow,
        size_t outWindowSamples,
        bool allowImmediateCopy)
    {
        ++events;

        const uint64_t voiceStart = eventVoiceStart;
        const uint64_t voiceEnd = eventStart + lastActive;

        if (voiceEnd <= voiceStart)
        {
            ++skipped;
            Serial.println("[VAD] OMITIDO: actividad invalida");
            return false;
        }

        const uint64_t activitySamples = voiceEnd - voiceStart;

        Serial.printf(
            "[VAD] EVENTO %lu | inicio voz=%.3f s | fin voz=%.3f s | actividad=%.1f ms\n",
            (unsigned long)events,
            voiceStart / double(KeywordConfig::SAMPLE_RATE_HZ),
            voiceEnd / double(KeywordConfig::SAMPLE_RATE_HZ),
            activitySamples * 1000.0 /
                KeywordConfig::SAMPLE_RATE_HZ);

        if (longActivity || activitySamples > KeywordConfig::MAX_ACTIVITY_SAMPLES) {
            // Inspect the tail too, without copying zeros or changing model size.
            const uint64_t tail=voiceEnd>KeywordConfig::MODEL_WINDOW_SAMPLES ?
                voiceEnd-KeywordConfig::MODEL_WINDOW_SAMPLES : 0;
            if(pending || (longActivity && tail<=lastLongStart)) return false;
            pendingStart=tail;pendingEventNumber=events;pending=true;
            return allowImmediateCopy ? copyPendingWindow(outWindow,outWindowSamples) : false;
        }

        if (activitySamples < KeywordConfig::MIN_ACTIVITY_SAMPLES)
        {
            ++skipped;
            Serial.printf(
                "[VAD] OMITIDO: actividad muy corta (%.1f ms < %u ms)\n",
                activitySamples * 1000.0 / KeywordConfig::SAMPLE_RATE_HZ,
                unsigned(KeywordConfig::MIN_ACTIVITY_MS));
            return false;
        }

        const int64_t centeredStart =
            (int64_t(voiceStart) +
             int64_t(voiceEnd) -
             int64_t(KeywordConfig::MODEL_WINDOW_SAMPLES)) /
            2;

        if (centeredStart < 0 || pending)
        {
            ++skipped;

            Serial.println(
                "[VAD] OMITIDO: historial inicial insuficiente o ventana pendiente");

            return false;
        }

        const uint64_t candidateStart =
            static_cast<uint64_t>(centeredStart);

        const uint64_t oldest = streamSamples - ringValid;

        if (candidateStart < oldest)
        {
            ++skipped;

            Serial.println(
                "[VAD] OMITIDO: inicio de ventana ya no disponible en ring");

            return false;
        }

        pendingStart = candidateStart;
        pendingEventNumber = events;
        pending = true;

        if (!allowImmediateCopy)
            return false;

        return copyPendingWindow(outWindow, outWindowSamples);
    }
}

void audioDetectorBegin()
{
    memset(audioRing, 0, sizeof(audioRing));
    memset(calibration, 0, sizeof(calibration));

    clearTemporalState();

    calibrationCount = 0;
    calibrated = KeywordConfig::FIXED_VAD_THRESHOLDS;
    backgroundNoise = 0.0f; // Not measured in fixed mode.
    highThreshold = KeywordConfig::FIXED_VAD_THRESHOLDS ?
        KeywordConfig::FIXED_HIGH_THRESHOLD : KeywordConfig::MIN_HIGH_THRESHOLD;
    lowThreshold = KeywordConfig::FIXED_VAD_THRESHOLDS ?
        KeywordConfig::FIXED_LOW_THRESHOLD : KeywordConfig::MIN_LOW_THRESHOLD;

    events = 0;
    windowsReady = 0;
    skipped = 0;

    Serial.printf(
        "[VAD] init | frame=%u ms | onset=%u ms | quiet=%u ms | calibration=%u ms\n",
        unsigned(KeywordConfig::FRAME_MS),
        unsigned(KeywordConfig::ONSET_BLOCKS * KeywordConfig::FRAME_MS),
        unsigned(KeywordConfig::QUIET_BLOCKS * KeywordConfig::FRAME_MS),
        unsigned(KeywordConfig::FIXED_VAD_THRESHOLDS ? 0 : KeywordConfig::NOISE_CALIBRATION_MS));
    Serial.printf("[VAD] mode=%s high=%.2f low=%.2f\n",
        KeywordConfig::FIXED_VAD_THRESHOLDS ? "fixed" : "automatic",
        highThreshold,lowThreshold);
    Serial.println("[VAD] long_activity=rolling window_ms=1000 stride_ms=750");
}

void audioDetectorResetStream()
{
    if (active || pending)
        ++skipped;

    clearTemporalState();

    if (!calibrated)
    {
        calibrationCount = 0;
        memset(calibration, 0, sizeof(calibration));
    }

    Serial.println("[VAD] estado temporal reiniciado por discontinuidad");
}

bool audioDetectorProcess(
    const int16_t *samples,
    size_t sampleCount,
    bool discontinuity,
    int16_t *outWindow,
    size_t outWindowSamples)
{
    if (samples == nullptr ||
        sampleCount != KeywordConfig::FRAME_SAMPLES)
    {
        return false;
    }

    if (discontinuity)
        audioDetectorResetStream();

    const uint64_t frameStart = streamSamples;

    for (size_t i = 0; i < KeywordConfig::FRAME_SAMPLES; ++i)
    {
        audioRing[
            (frameStart + i) % KeywordConfig::AUDIO_RING_SAMPLES] =
            samples[i];
    }

    streamSamples += KeywordConfig::FRAME_SAMPLES;

    ringValid = std::min(
        KeywordConfig::AUDIO_RING_SAMPLES,
        ringValid + KeywordConfig::FRAME_SAMPLES);

    // A pending candidate may become complete as soon as this new frame
    // arrives. We remember that result but still process this same 20 ms
    // frame through RMS/VAD so the detector does not create a temporal hole.
    bool windowReady = copyPendingWindow(outWindow, outWindowSamples);

    const float energy = calculateRms(samples);

    // ------------------------------------------------------------
    // Initial background-noise calibration (500 ms in v1)
    // ------------------------------------------------------------

    if (!calibrated)
    {
        calibration[calibrationCount++] = energy;

        if (calibrationCount >= KeywordConfig::NOISE_CALIBRATION_BLOCKS)
            finishCalibration();

        return windowReady;
    }

    // ------------------------------------------------------------
    // RMS / VAD activity detection
    // ------------------------------------------------------------

    if (!active)
    {
        onset =
            energy > highThreshold
                ? onset + 1
                : 0;

        if (onset >= KeywordConfig::ONSET_BLOCKS)
        {
            active = true;
            longActivity = false;lastLongStart=0;
            quiet = 0;

            // The first high-energy block is approximately ONSET_BLOCKS
            // behind the current frame end.
            const uint64_t onsetSpan =
                KeywordConfig::ONSET_BLOCKS * KeywordConfig::FRAME_SAMPLES;

            eventVoiceStart =
                streamSamples >= onsetSpan
                    ? streamSamples - onsetSpan
                    : 0;

            eventStart =
                eventVoiceStart >= KeywordConfig::PREBUFFER_SAMPLES
                    ? eventVoiceStart - KeywordConfig::PREBUFFER_SAMPLES
                    : 0;

            eventSize = static_cast<size_t>(
                streamSamples - eventStart);

            lastActive = eventSize;

            Serial.printf(
                "[VAD] actividad iniciada RMS=%.2f high=%.2f\n",
                energy,
                highThreshold);
        }

        return windowReady;
    }

    // Already inside an activity event.
    eventSize += KeywordConfig::FRAME_SAMPLES;

    if (energy > lowThreshold)
    {
        quiet = 0;
        lastActive = eventSize;
    }
    else
    {
        ++quiet;
    }

    if (quiet < KeywordConfig::QUIET_BLOCKS) {
        if(!pending && !windowReady && eventStart+lastActive-eventVoiceStart>KeywordConfig::MAX_ACTIVITY_SAMPLES) {
            const uint64_t start=longActivity ? lastLongStart+KeywordConfig::LONG_WINDOW_STRIDE_SAMPLES : eventStart;
            if(streamSamples>=start+KeywordConfig::MODEL_WINDOW_SAMPLES) {
                pendingStart=start;pendingEventNumber=events+1;pending=true;
                lastLongStart=start;longActivity=true;
                Serial.println("[VAD] ventana parcial de actividad larga");
                windowReady=copyPendingWindow(outWindow,outWindowSamples);
            }
        }
        return windowReady;
    }

    // If a previous pending window was completed by this same frame, keep
    // that window intact for the caller. A newly closed event is scheduled
    // as pending and can be copied on the next frame.
    const bool newWindowReady = closeEvent(
        outWindow,
        outWindowSamples,
        !windowReady);

    active = false;
    longActivity = false;lastLongStart=0;
    onset = 0;
    quiet = 0;
    eventSize = 0;
    lastActive = 0;

    return windowReady || newWindowReady;
}

AudioDetectorStats audioDetectorGetStats()
{
    AudioDetectorStats stats{};
    stats.events = events;
    stats.windowsReady = windowsReady;
    stats.skipped = skipped;
    stats.backgroundNoise = backgroundNoise;
    stats.highThreshold = highThreshold;
    stats.lowThreshold = lowThreshold;
    stats.calibrated = calibrated;
    return stats;
}
