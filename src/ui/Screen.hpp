#pragma once

#include "ScreenModel.hpp"

namespace ui {

/*!
 * Abstract presentation surface.
 *
 * Implementations either render the model locally or forward it to a remote
 * consumer. Both must be free of application state.
 */
class Screen {
public:
    virtual ~Screen() = default;

    virtual void present(const ScreenModel &model) = 0;
};

}
