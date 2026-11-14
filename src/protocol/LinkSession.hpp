#pragma once

#include <cstddef>
#include <cstdint>

#include "ByteStream.hpp"
#include "Messages.hpp"
#include "UartTransport.hpp"
#include "../ui/ScreenModel.hpp"

namespace proto {

/*!
 * Owns a UART link and speaks the frame protocol over it.
 *
 * The session is transport and framing only: it knows nothing about widgets or
 * key semantics. Presentation and input translation live in the adapters built
 * on top of it.
 */
class LinkSession {
public:
    static constexpr size_t KeyQueueSize = 8;

    /*!
     * Maximum widgets buffered per Screen frame.
     *
     * Derived from the screen model rather than set independently: the two once
     * disagreed (12 here, 24 there), so every widget past the twelfth - the
     * input box and all four buttons on the chat page - was silently dropped and
     * the remote screen appeared to have no controls.
     */
    static constexpr size_t MaxScreenComponents = ui::ScreenModel::MaxComponents;
    static constexpr uint8_t ProtocolVersion = 1;

    explicit LinkSession(ByteStream &stream);

    void init();

    /*!
     * Drains the UART and dispatches every complete frame.
     */
    void poll();

    bool sendHello(uint16_t width, uint16_t height);
    bool sendNotice(const NoticeMessage &notice);

    /*!
     * Starts collecting a Screen frame.
     *
     * The component count is only known once every widget has been described,
     * so components are buffered here and emitted as one frame by
     * finishScreen().
     */
    void beginScreen(uint16_t width, uint16_t height, bool fullRedraw);

    /*!
     * Buffers one widget.
     *
     * \return false when the frame is already full. Callers should surface this:
     *         a dropped widget means the remote screen is missing controls.
     */
    bool addComponent(const ComponentMessage &component);

    /*!
     * Emits the buffered Screen frame.
     *
     * \return false when the frame does not fit in a single payload or when no
     *         screen was open.
     */
    bool finishScreen();

    /*!
     * Pops the oldest received key.
     */
    bool nextKey(KeyMessage &key);

    /*!
     * True when the most recent Screen frame dropped a widget because it
     * exceeded MaxScreenComponents.
     */
    bool screenOverflowed() const;

    /*!
     * True when the host has sent at least one valid frame since boot.
     */
    bool hostSeen() const;

    /*!
     * Consumes a pending full-redraw request.
     *
     * A host that attaches mid-session has no widgets yet, so the first frame it
     * receives must be a complete screen rather than an incremental update.
     */
    bool consumeResyncRequest();

private:
    static void onFrame(MessageType type, const uint8_t *payload, size_t length, void *context);
    void handleKeyFrame(const uint8_t *payload, size_t length);
    bool emit(MessageType type, const uint8_t *payload, size_t length);

    ByteStream &stream_;
    FrameWriter writer_;
    FrameReader reader_;

    uint8_t writeBuffer_[MaxFrame]{};
    uint8_t readBuffer_[MaxPayload]{};
    uint8_t payloadBuffer_[MaxPayload]{};

    KeyMessage keyQueue_[KeyQueueSize]{};
    size_t keyHead_ = 0;
    size_t keyCount_ = 0;

    ComponentMessage screenComponents_[MaxScreenComponents]{};
    uint16_t screenWidth_ = 0;
    uint16_t screenHeight_ = 0;
    bool screenFullRedraw_ = false;
    size_t screenCount_ = 0;
    bool screenOpen_ = false;

    /*!
     * True once a widget had to be dropped because the buffer was full.
     *
     * Surfaced so the app can report a layout that outgrew the protocol budget
     * instead of silently showing the host a screen with missing controls.
     */
    bool screenOverflowed_ = false;

    bool hostSeen_ = false;
    bool resyncRequested_ = false;
};

/*!
 * The screen buffer must be able to hold a complete screen model. If it cannot,
 * widgets are silently dropped from the end of a frame and the remote display
 * loses controls, which is hard to spot at runtime.
 */
static_assert(LinkSession::MaxScreenComponents >= ui::ScreenModel::MaxComponents,
              "LinkSession::MaxScreenComponents must cover ui::ScreenModel::MaxComponents");

}
