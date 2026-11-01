#include "event_mapper.h"
#include "keyword_commands.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>

Logger Serial;

static KeywordResult result(const char *label, float confidence, bool ok = true)
{
    KeywordResult value{};
    value.ok = ok;
    value.label = label;
    value.confidence = confidence;
    return value;
}

int main()
{
    // Threshold equality must accept; the next representable lower score rejects.
    for (const auto &rule : KeywordCommands::RULES)
    {
        const auto accepted = eventMapperMap(result(rule.label, rule.minConfidence));
        assert(accepted.knownLabel && accepted.emit && accepted.event == rule.event);
        assert(accepted.threshold == rule.minConfidence);
        const auto rejected = eventMapperMap(result(rule.label,
            std::nextafter(rule.minConfidence, 0.0f)));
        assert(rejected.knownLabel && !rejected.emit);
        assert(rejected.event == EarsProtocol::AudioEvent::NONE);
        assert(rejected.threshold == rule.minConfidence);
        assert(eventMapperMap(result(rule.label, 1.0f)).emit);
        assert(!eventMapperMap(result(rule.label, 0.0f)).emit);
        assert(!eventMapperMap(result(rule.label, 1.0f, false)).emit);
    }
    // Noise, unmapped labels and failed inference must never become commands.
    for (const char *label : {"noise", "unknown", "not_trained", "walle", "PIUU", "PIUU_STOP", "linterna", "luz", ""})
    {
        const auto mapped = eventMapperMap(result(label, 1.0f));
        assert(!mapped.knownLabel && !mapped.emit);
    }
    assert(!eventMapperMap(result(nullptr, 1.0f)).emit);
    assert(eventMapperMap(result("piuu", 0.90f)).event == EarsProtocol::AudioEvent::WAKE_WORD);
    assert(eventMapperMap(result("piuu_stop", 0.90f)).event == EarsProtocol::AudioEvent::STANDBY_WORD);
    assert(static_cast<unsigned>(EarsProtocol::AudioEvent::STANDBY_WORD) == 0x07);
    eventMapperPrintConfig();
    puts("PASS: per-keyword confidence, exact boundary, rejected labels and inference errors");
}
