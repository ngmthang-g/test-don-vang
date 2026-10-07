#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gold_history {

inline constexpr std::int64_t kRetentionSeconds = 3LL * 24LL * 60LL * 60LL;
inline constexpr std::int64_t kAutoPruneIntervalSeconds = 10LL * 60LL;

struct Sample {
    std::int64_t unixSeconds = 0;
    std::wstring identity{};
    std::wstring characterName{};
    std::int64_t boundMoneyRaw = 0;
};

class Store {
public:
    bool Open(const std::filesystem::path& path, std::int64_t nowUnix, std::wstring& error);
    bool Append(const Sample& sample, std::int64_t nowUnix, std::wstring& error);
    bool Prune(std::int64_t nowUnix, std::wstring& error);

    std::optional<Sample> Latest(const std::wstring& identity) const;
    std::optional<Sample> Nearest(const std::wstring& identity,
                                  std::int64_t targetUnix,
                                  std::int64_t maxDistanceSeconds) const;
    bool HasContinuousCoverage(const std::wstring& identity,
                               std::int64_t startUnix,
                               std::int64_t endUnix,
                               std::int64_t endpointToleranceSeconds,
                               std::int64_t maxGapSeconds) const;

    const std::vector<Sample>& Samples() const { return samples_; }
    const std::filesystem::path& Path() const { return path_; }

private:
    bool Rewrite(std::wstring& error);
    bool AppendLine(const Sample& sample, std::wstring& error);

    std::filesystem::path path_{};
    std::vector<Sample> samples_{};
    std::int64_t lastPruneUnix_ = 0;
};

std::int64_t UnixNow();

} // namespace gold_history
