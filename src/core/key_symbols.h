#pragma once

#include <cstdint>
#include <xkbcommon/xkbcommon-keysyms.h>

namespace KeySymbol {
[[nodiscard]] inline bool isEnter(std::uint32_t sym) noexcept {
  return sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter;
}

[[nodiscard]] inline bool isBackspace(std::uint32_t sym) noexcept {
  return sym == XKB_KEY_BackSpace;
}

[[nodiscard]] inline bool isDelete(std::uint32_t sym) noexcept {
  return sym == XKB_KEY_Delete;
}

[[nodiscard]] inline bool isLeft(std::uint32_t sym) noexcept {
  return sym == XKB_KEY_Left;
}

[[nodiscard]] inline bool isRight(std::uint32_t sym) noexcept {
  return sym == XKB_KEY_Right;
}

[[nodiscard]] inline bool isHome(std::uint32_t sym) noexcept {
  return sym == XKB_KEY_Home;
}

[[nodiscard]] inline bool isEnd(std::uint32_t sym) noexcept {
  return sym == XKB_KEY_End;
}
} // namespace KeySymbol
