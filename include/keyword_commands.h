#pragma once

#include "ears_protocol.h"

// One rule per trained keyword. Labels must match the Edge Impulse export
// exactly (PIUU v1 uses lowercase "piuu" and "piuu_stop").
// Edit each minConfidence independently, then rebuild/upload KEYWORD.
namespace KeywordCommands
{
    struct Rule
    {
        const char *label;
        EarsProtocol::AudioEvent event;
        float minConfidence;
    };

    static constexpr Rule RULES[] =
    {
        {"piuu", EarsProtocol::AudioEvent::WAKE_WORD, 0.90f},
        {"piuu_stop", EarsProtocol::AudioEvent::STANDBY_WORD, 0.90f},
        // Add future trained labels here with their own thresholds, e.g.:
        // {"stop", EarsProtocol::AudioEvent::STOP_WORD, 0.80f},
        // {"lights", EarsProtocol::AudioEvent::LIGHTS_WORD, 0.70f},
    };

    static constexpr size_t RULE_COUNT = sizeof(RULES) / sizeof(RULES[0]);

    constexpr bool validRules(size_t index)
    {
        return index == RULE_COUNT ||
            (RULES[index].label != nullptr && RULES[index].label[0] != '\0' &&
             RULES[index].event != EarsProtocol::AudioEvent::NONE &&
             RULES[index].minConfidence > 0.0f && RULES[index].minConfidence <= 1.0f &&
             validRules(index + 1));
    }

    static_assert(validRules(0), "Keyword rules need a label, an event and confidence in (0, 1]");
}
