#include "ScreenModel.hpp"

#include <cstring>

namespace ui {

void copyText(char *destination, size_t destinationSize, const char *source)
{
    if (destinationSize == 0) {
        return;
    }
    if (source == nullptr) {
        destination[0] = '\0';
        return;
    }

    size_t i = 0;
    while (i + 1 < destinationSize && source[i] != '\0') {
        destination[i] = source[i];
        i++;
    }
    destination[i] = '\0';
}

void ScreenModel::clear()
{
    count_ = 0;
}

bool ScreenModel::add(const ComponentView &view)
{
    if (count_ >= MaxComponents) {
        return false;
    }
    components_[count_++] = view;
    return true;
}

size_t ScreenModel::count() const
{
    return count_;
}

const ComponentView &ScreenModel::at(size_t index) const
{
    return components_[index];
}

void ScreenModel::setFullRedraw(bool fullRedraw)
{
    fullRedraw_ = fullRedraw;
}

bool ScreenModel::fullRedraw() const
{
    return fullRedraw_;
}

namespace {

bool sameView(const ComponentView &a, const ComponentView &b)
{
    return a.kind == b.kind &&
           a.bounds.x == b.bounds.x &&
           a.bounds.y == b.bounds.y &&
           a.bounds.w == b.bounds.w &&
           a.bounds.h == b.bounds.h &&
           a.color == b.color &&
           a.statusColor == b.statusColor &&
           a.scale == b.scale &&
           a.focused == b.focused &&
           a.selected == b.selected &&
           a.focusable == b.focusable &&
           std::strcmp(a.primary, b.primary) == 0 &&
           std::strcmp(a.secondary, b.secondary) == 0;
}

}

bool ScreenModel::sameContent(const ScreenModel &other) const
{
    if (count_ != other.count_) {
        return false;
    }

    for (size_t i = 0; i < count_; i++) {
        if (!sameView(components_[i], other.components_[i])) {
            return false;
        }
    }
    return true;
}

}
