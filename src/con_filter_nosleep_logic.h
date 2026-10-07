#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

namespace con_filter_nosleep_logic {

constexpr int kMinActionMsLower = 0;
constexpr int kMinActionMsUpper = 10000;

inline int ClampMinActionMs(int value) {
    return std::clamp(value, kMinActionMsLower, kMinActionMsUpper);
}

inline bool ActionDue(std::uint64_t now, std::uint64_t lastActionAt, int minimumMs) {
    const auto minMs = static_cast<std::uint64_t>(ClampMinActionMs(minimumMs));
    return lastActionAt == 0 || now >= lastActionAt + minMs;
}

inline std::uint64_t NotBefore(std::uint64_t now, std::uint64_t lastActionAt, int minimumMs) {
    if (lastActionAt == 0) return now;
    const auto boundary = lastActionAt + static_cast<std::uint64_t>(ClampMinActionMs(minimumMs));
    return std::max(now, boundary);
}

enum class SemanticProbeFailure {
    Retry,
    NotFound,
    Ambiguous,
};

inline SemanticProbeFailure ClassifySemanticFailure(const std::wstring& detail) {
    if (detail.find(L"AMBIGUOUS") != std::wstring::npos) return SemanticProbeFailure::Ambiguous;
    if (detail.find(L"NOT_FOUND") != std::wstring::npos) return SemanticProbeFailure::NotFound;
    return SemanticProbeFailure::Retry;
}

} // namespace con_filter_nosleep_logic
