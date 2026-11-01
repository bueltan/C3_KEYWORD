#include "event_mapper.h"

#include <cstring>

#include "keyword_commands.h"

void eventMapperPrintConfig()
{
    for (const auto &entry : KeywordCommands::RULES)
    {
        Serial.printf("[MAP] config label=%s event=%s threshold=%.3f\n",
            entry.label, eventMapperEventName(entry.event), entry.minConfidence);
    }
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

    for (const auto &entry : KeywordCommands::RULES)
    {
        if (strcmp(result.label, entry.label) != 0)
            continue;

        mapped.knownLabel = true;
        mapped.threshold = entry.minConfidence;

        if (result.confidence >= entry.minConfidence)
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

        case EarsProtocol::AudioEvent::STANDBY_WORD:
            return "STANDBY_WORD";

        case EarsProtocol::AudioEvent::MUTE_WORD:
            return "MUTE_WORD";

        case EarsProtocol::AudioEvent::NONE:
        default:
            return "NONE";
    }
}
