#include "event_mapper.h"

#include <cstring>

namespace
{
    struct Mapping
    {
        const char *label;
        EarsProtocol::AudioEvent event;
        float threshold;
    };

    // This is intentionally the only place that knows the current model's
    // semantic labels. When the model is retrained, edit this table without
    // changing UART, VAD, or EARS.
    static constexpr Mapping mappings[] =
    {
        {"walle", EarsProtocol::AudioEvent::WAKE_WORD, 0.80f},
    };
}

EventMappingResult eventMapperMap(const KeywordResult &result)
{
    EventMappingResult mapped{};
    mapped.knownLabel = false;
    mapped.emit = false;
    mapped.event = EarsProtocol::AudioEvent::NONE;
    mapped.threshold = 0.0f;

    if (!result.ok || result.label == nullptr)
        return mapped;

    for (const Mapping &entry : mappings)
    {
        if (strcmp(result.label, entry.label) != 0)
            continue;

        mapped.knownLabel = true;
        mapped.threshold = entry.threshold;

        if (result.confidence >= entry.threshold)
        {
            mapped.emit = true;
            mapped.event = entry.event;
        }

        return mapped;
    }

    return mapped;
}

const char *eventMapperEventName(EarsProtocol::AudioEvent event)
{
    switch (event)
    {
        case EarsProtocol::AudioEvent::WAKE_WORD:
            return "WAKE_WORD";

        case EarsProtocol::AudioEvent::LIGHTS_WORD:
            return "LIGHTS_WORD";

        case EarsProtocol::AudioEvent::STOP_WORD:
            return "STOP_WORD";

        case EarsProtocol::AudioEvent::MUTE_WORD:
            return "MUTE_WORD";

        case EarsProtocol::AudioEvent::NONE:
        default:
            return "NONE";
    }
}
