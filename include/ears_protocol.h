#pragma once

#include <Arduino.h>

namespace EarsProtocol
{
    // ============================================================
    // EARS -> KEYWORD audio protocol
    // ============================================================

    static constexpr uint8_t PCM_MAGIC[4] = {'P', 'C', 'M', '!'};

    struct __attribute__((packed)) PcmHeader
    {
        uint8_t magic[4];
        uint32_t sequence;
        uint16_t payloadBytes;
    };

    struct __attribute__((packed)) PcmHeaderTail
    {
        uint32_t sequence;
        uint16_t payloadBytes;
    };

    // ============================================================
    // KEYWORD -> EARS event protocol
    // ============================================================

    static constexpr uint8_t EVENT_MAGIC[4] = {'E', 'V', 'T', '!'};

    // IMPORTANT:
    // These values must stay identical to control_protocol.h in C3_EARS.
    // This is the mapping used by the current EARS design.
    enum class AudioEvent : uint8_t
    {
        NONE        = 0x00,
        WAKE_WORD   = 0x01,
        LIGHTS_WORD = 0x02,
        STOP_WORD   = 0x03,
        MUTE_WORD   = 0x04
    };
    static constexpr uint8_t EVENT_PROTOCOL_VERSION = 1;
    static constexpr size_t EVENT_MAX_PAYLOAD_BYTES = 32;

    struct __attribute__((packed)) EventHeader
    {
        uint8_t magic[4];
        uint8_t version;
        uint8_t event;
        uint16_t payloadBytes;
    };
}

static_assert(sizeof(EarsProtocol::PcmHeader) == 10,
              "PCM! header must be exactly 10 bytes");

static_assert(sizeof(EarsProtocol::PcmHeaderTail) == 6,
              "PCM! header tail must be exactly 6 bytes");

static_assert(sizeof(EarsProtocol::EventHeader) == 8,
              "EVT! header must be exactly 8 bytes");
