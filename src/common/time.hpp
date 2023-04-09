#pragma once

#include <chrono>

namespace ws {

using TimePoint = std::chrono::high_resolution_clock::time_point;
using Duration = std::chrono::duration<double>;

inline TimePoint now() {
  return std::chrono::high_resolution_clock::now();
}

inline double elapsed_time(const TimePoint& t0, const TimePoint& t1) {
  return Duration(t1 - t0).count();
}

}