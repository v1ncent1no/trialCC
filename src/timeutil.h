#pragma once

namespace timeutil {

using TimePoint = std::size_t;

auto make_time_point(std::size_t hours, std::size_t minutes) -> TimePoint;
auto time_point_hm(const TimePoint &p, std::size_t &hours, std::size_t &minutes)
    -> void;

struct TimeInterval {
    TimePoint begin;
    TimePoint end;

    [[nodiscard]] auto contains(TimePoint p) const -> bool;
};

} // namespace timeutil
