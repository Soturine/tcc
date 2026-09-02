#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

using String = std::string;

class NativeSerialStub {
 public:
  template <typename T>
  void print(const T&) {}

  template <typename T>
  void println(const T&) {}

  void println() {}

  template <typename... Args>
  void printf(const char*, Args...) {}
};

inline NativeSerialStub Serial;
