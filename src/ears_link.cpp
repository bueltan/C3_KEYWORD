#include "ears_link.h"

#include <algorithm>
#include <cstring>

namespace
{
    HardwareSerial earsSerial(1);

    QueueHandle_t audioQueue = nullptr;
    TaskHandle_t rxTaskHandle = nullptr;

    volatile uint32_t droppedFrames = 0;
    volatile uint32_t receivedFrames = 0;

    bool readExact(uint8_t *destination, size_t length, uint32_t timeoutMs)
    {
        if (destination == nullptr || length == 0)
            return false;

        size_t received = 0;
        const uint32_t start = millis();

        while (received < length)
        {
            const int available = earsSerial.available();

            if (available > 0)
            {
                const size_t remaining = length - received;
                const size_t toRead = std::min(
                    remaining,
                    static_cast<size_t>(available));

                const size_t n = earsSerial.read(
                    destination + received,
                    toRead);

                received += n;
                continue;
            }

            if ((uint32_t)(millis() - start) >= timeoutMs)
                return false;

            vTaskDelay(pdMS_TO_TICKS(1));
        }

        return true;
    }

    void findPcmMagic()
    {
        size_t matched = 0;

        for (;;)
        {
            if (earsSerial.available() <= 0)
            {
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }

            const uint8_t b = static_cast<uint8_t>(earsSerial.read());

            if (b == EarsProtocol::PCM_MAGIC[matched])
            {
                ++matched;

                if (matched == sizeof(EarsProtocol::PCM_MAGIC))
                    return;
            }
            else
            {
                matched =
                    (b == EarsProtocol::PCM_MAGIC[0])
                        ? 1
                        : 0;
            }
        }
    }

    void uartRxTask(void *)
    {
        bool haveExpectedSequence = false;
        uint32_t expectedSequence = 0;
        bool pendingDiscontinuity = false;

        Serial.println("[LINK] UART RX task iniciado");

        for (;;)
        {
            findPcmMagic();

            EarsProtocol::PcmHeaderTail header{};

            if (!readExact(
                    reinterpret_cast<uint8_t *>(&header),
                    sizeof(header),
                    KeywordConfig::UART_READ_TIMEOUT_MS))
            {
                Serial.println("[LINK] timeout leyendo header PCM!");
                pendingDiscontinuity = true;
                continue;
            }

            if (header.payloadBytes != KeywordConfig::FRAME_BYTES)
            {
                Serial.printf(
                    "[LINK] payload PCM invalido seq=%lu len=%u\n",
                    (unsigned long)header.sequence,
                    (unsigned)header.payloadBytes);

                // Do not blindly drain the UART. We simply resume searching
                // for the next real PCM! magic in the byte stream.
                pendingDiscontinuity = true;
                haveExpectedSequence = false;
                continue;
            }

            AudioFrame frame{};
            frame.sequence = header.sequence;
            frame.discontinuity = pendingDiscontinuity;

            if (haveExpectedSequence && header.sequence != expectedSequence)
            {
                frame.discontinuity = true;

                Serial.printf(
                    "[LINK] discontinuidad seq esperado=%lu recibido=%lu\n",
                    (unsigned long)expectedSequence,
                    (unsigned long)header.sequence);
            }

            if (!readExact(
                    reinterpret_cast<uint8_t *>(frame.samples),
                    KeywordConfig::FRAME_BYTES,
                    KeywordConfig::UART_READ_TIMEOUT_MS))
            {
                Serial.printf(
                    "[LINK] timeout leyendo PCM seq=%lu\n",
                    (unsigned long)header.sequence);

                pendingDiscontinuity = true;
                haveExpectedSequence = false;
                continue;
            }

            expectedSequence = header.sequence + 1;
            haveExpectedSequence = true;

            if (xQueueSend(audioQueue, &frame, 0) != pdTRUE)
            {
                ++droppedFrames;
                pendingDiscontinuity = true;

                Serial.printf(
                    "[LINK] cola llena; frame descartado seq=%lu\n",
                    (unsigned long)header.sequence);
            }
            else
            {
                ++receivedFrames;
                pendingDiscontinuity = false;

                if ((receivedFrames % 50) == 0)
                {
                    Serial.printf(
                        "[LINK] frames=%lu queue=%u dropped=%lu heap=%u\n",
                        (unsigned long)receivedFrames,
                        unsigned(uxQueueMessagesWaiting(audioQueue)),
                        (unsigned long)droppedFrames,
                        ESP.getFreeHeap());
                }
            }
        }
    }
}

bool earsLinkBegin()
{
    if (audioQueue != nullptr)
        return true;

    audioQueue = xQueueCreate(
        KeywordConfig::AUDIO_QUEUE_FRAMES,
        sizeof(AudioFrame));

    if (audioQueue == nullptr)
    {
        Serial.println("[LINK] ERROR creando cola de audio");
        return false;
    }

    earsSerial.setRxBufferSize(KeywordConfig::UART_RX_BUFFER_BYTES);

    earsSerial.begin(
        KeywordConfig::UART_BAUD,
        SERIAL_8N1,
        KeywordConfig::UART_RX_PIN,
        KeywordConfig::UART_TX_PIN);

    Serial.printf(
        "[LINK] UART EARS <-> KEYWORD RX=%d TX=%d @ %lu baud\n",
        KeywordConfig::UART_RX_PIN,
        KeywordConfig::UART_TX_PIN,
        (unsigned long)KeywordConfig::UART_BAUD);

    if (xTaskCreate(
            uartRxTask,
            "ears_uart_rx",
            KeywordConfig::UART_TASK_STACK_BYTES,
            nullptr,
            KeywordConfig::UART_TASK_PRIORITY,
            &rxTaskHandle) != pdPASS)
    {
        Serial.println("[LINK] ERROR creando tarea UART RX");
        vQueueDelete(audioQueue);
        audioQueue = nullptr;
        return false;
    }

    return true;
}

bool earsLinkReceive(AudioFrame &frame, uint32_t timeoutMs)
{
    if (audioQueue == nullptr)
        return false;

    const TickType_t waitTicks =
        timeoutMs == 0
            ? 0
            : pdMS_TO_TICKS(timeoutMs);

    return xQueueReceive(
               audioQueue,
               &frame,
               waitTicks) == pdTRUE;
}

bool earsLinkSendEvent(
    EarsProtocol::AudioEvent event,
    const void *payload,
    uint16_t payloadBytes)
{
    if (event == EarsProtocol::AudioEvent::NONE)
        return false;

    if (payloadBytes > EarsProtocol::EVENT_MAX_PAYLOAD_BYTES)
        return false;

    if (payloadBytes > 0 && payload == nullptr)
        return false;

    EarsProtocol::EventHeader header{};

    memcpy(
        header.magic,
        EarsProtocol::EVENT_MAGIC,
        sizeof(header.magic));

    header.version = EarsProtocol::EVENT_PROTOCOL_VERSION;
    header.event = static_cast<uint8_t>(event);
    header.payloadBytes = payloadBytes;

    const size_t headerWritten = earsSerial.write(
        reinterpret_cast<const uint8_t *>(&header),
        sizeof(header));

    if (headerWritten != sizeof(header))
        return false;

    if (payloadBytes > 0)
    {
        const size_t payloadWritten = earsSerial.write(
            reinterpret_cast<const uint8_t *>(payload),
            payloadBytes);

        if (payloadWritten != payloadBytes)
            return false;
    }

    return true;
}

uint32_t earsLinkDroppedFrames()
{
    return droppedFrames;
}

uint32_t earsLinkReceivedFrames()
{
    return receivedFrames;
}

size_t earsLinkQueueDepth()
{
    if (audioQueue == nullptr)
        return 0;

    return uxQueueMessagesWaiting(audioQueue);
}
