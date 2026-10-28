#pragma once

#include "ears_protocol.h"
#include "keyword_model.h"

struct EventMappingResult
{
    bool knownLabel;
    bool emit;
    EarsProtocol::AudioEvent event;
    float threshold;
};

// Converts a model label + confidence into a stable system event.
// Model labels stay private to this layer; EARS only sees AudioEvent IDs.
EventMappingResult eventMapperMap(const KeywordResult &result);

const char *eventMapperEventName(EarsProtocol::AudioEvent event);
