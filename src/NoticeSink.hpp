#pragma once

#include "ui/Ui.hpp"

/*!
 * Lightweight non-owning callback for emitting status bar / host notices.
 */
struct NoticeSink {
    using Callback = void (*)(void *ctx, const char *message, ui::Tone tone);

    Callback callback = nullptr;
    void *ctx = nullptr;

    void emit(const char *message, ui::Tone tone) const
    {
        if (callback != nullptr && message != nullptr) {
            callback(ctx, message, tone);
        }
    }
};
