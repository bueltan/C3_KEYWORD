#pragma once

#include <Arduino.h>

#include "ears_protocol.h"
#include "keyword_config.h"

struct AudioFrame
{
    uint32_t sequence;
    bool discontinuity;
    int16_t samples[KeywordConfig::FRAME_SAMPLES];
};

// Initializes the full-duplex UART link and starts the independent RX task.
bool earsLinkBegin();

// Receives one already validated PCM frame from the internal queue.
// timeoutMs == 0 makes the call non-blocking.
bool earsLinkReceive(AudioFrame &frame, uint32_t timeoutMs = 100);

// Sends an EVT! frame back to C3_EARS.
bool earsLinkSendEvent(
    EarsProtocol::AudioEvent event,
    const void *payload = nullptr,
    uint16_t payloadBytes = 0);

uint32_t earsLinkDroppedFrames();
uint32_t earsLinkReceivedFrames();
size_t earsLinkQueueDepth();
