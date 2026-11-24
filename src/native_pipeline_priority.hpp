// Stable priority ordering for finite, explicitly invoked pipeline batches.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>
namespace scarlett::pipeline {
enum class Priority : std::uint8_t { Background, Normal, FrameCritical };
constexpr bool valid_priority(Priority p) noexcept {
    return p==Priority::Background || p==Priority::Normal || p==Priority::FrameCritical;
}
constexpr Priority promote_priority(Priority a,Priority b) noexcept {return a<b?b:a;}
inline std::vector<std::size_t> priority_order(const std::vector<Priority>& priorities){
    std::vector<std::size_t> order(priorities.size());
    std::iota(order.begin(),order.end(),std::size_t{0});
    std::stable_sort(order.begin(),order.end(),[&](std::size_t a,std::size_t b){return priorities[a]>priorities[b];});
    return order;
}
}
