#include <cstddef>

#include "timeutil.h"

namespace timeutil {

auto make_time_point(std::size_t hours, std::size_t minutes) -> TimePoint
{
    return hours * 60 + minutes;
}

auto time_point_hm(const TimePoint &p, std::size_t &hours, std::size_t &minutes)
    -> void
{
    hours   = p / 60;
    minutes = p % 60;
}

auto TimeInterval::contains(TimePoint p) const -> bool
{
    return p >= begin && p <= end;
}

} // namespace timeutil
