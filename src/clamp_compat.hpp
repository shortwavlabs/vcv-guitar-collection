#pragma once

namespace swv {
namespace compat {

template <typename T>
inline T clamp(T value, T low, T high) {
    return value < low ? low : (high < value ? high : value);
}

} // namespace compat
} // namespace swv