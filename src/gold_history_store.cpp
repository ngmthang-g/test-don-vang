#include "gold_history_store.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace gold_history {
namespace {

constexpr const char* kHeader = "TL_GOLD_HISTORY_V1";

std::string EncodeWide(const std::wstring& value) {
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill('0');
    bool first = true;
    for (wchar_t ch : value) {
        if (!first) out << '.';
        first = false;
        using UnsignedWide = std::make_unsigned_t<wchar_t>;
        const auto v = static_cast<std::uint64_t>(static_cast<UnsignedWide>(ch));
        out << std::setw(static_cast<int>(sizeof(wchar_t) * 2)) << v;
    }
    return out.str();
}

bool DecodeWide(const std::string& encoded, std::wstring& value) {
    value.clear();
    if (encoded.empty()) return true;
    std::size_t pos = 0;
    const std::size_t width = sizeof(wchar_t) * 2;
    while (pos < encoded.size()) {
        const std::size_t dot = encoded.find('.', pos);
        const std::size_t end = dot == std::string::npos ? encoded.size() : dot;
        if (end - pos != width) return false;
        std::uint64_t raw = 0;
        std::istringstream in(encoded.substr(pos, width));
        in >> std::hex >> raw;
        if (!in || !in.eof() || raw > static_cast<std::uint64_t>(std::numeric_limits<std::make_unsigned_t<wchar_t>>::max()))
            return false;
        value.push_back(static_cast<wchar_t>(raw));
        if (dot == std::string::npos) break;
        pos = dot + 1;
    }
    return true;
}

bool ParseInt64(const std::string& text, std::int64_t& value) {
    try {
        std::size_t used = 0;
        const long long parsed = std::stoll(text, &used, 10);
        if (used != text.size()) return false;
        value = static_cast<std::int64_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> SplitTabs(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t tab = line.find('\t', start);
        fields.push_back(line.substr(start, tab == std::string::npos ? std::string::npos : tab - start));
        if (tab == std::string::npos) break;
        start = tab + 1;
    }
    return fields;
}

bool ParseSample(const std::string& line, Sample& sample) {
    const auto fields = SplitTabs(line);
    if (fields.size() != 4) return false;
    if (!ParseInt64(fields[0], sample.unixSeconds) || !ParseInt64(fields[3], sample.boundMoneyRaw)) return false;
    if (!DecodeWide(fields[1], sample.identity) || !DecodeWide(fields[2], sample.characterName)) return false;
    return sample.unixSeconds > 0 && !sample.identity.empty();
}

std::string SerializeSample(const Sample& sample) {
    return std::to_string(sample.unixSeconds) + "\t" + EncodeWide(sample.identity) + "\t" +
           EncodeWide(sample.characterName) + "\t" + std::to_string(sample.boundMoneyRaw);
}

} // namespace

std::int64_t UnixNow() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

bool Store::Open(const std::filesystem::path& path, std::int64_t nowUnix, std::wstring& error) {
    path_ = path;
    samples_.clear();
    lastPruneUnix_ = nowUnix;
    error.clear();

    std::error_code ec;
    const auto parent = path_.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    if (ec) {
        error = L"GOLD HISTORY: không tạo được thư mục lưu";
        return false;
    }

    bool needsRewrite = false;
    if (std::filesystem::exists(path_, ec) && !ec) {
        std::ifstream in(path_, std::ios::binary);
        if (!in) {
            error = L"GOLD HISTORY: không mở được file lịch sử";
            return false;
        }
        std::string line;
        if (!std::getline(in, line)) {
            needsRewrite = true;
        } else {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line != kHeader) needsRewrite = true;
            if (!needsRewrite) {
                while (std::getline(in, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (line.empty()) continue;
                    Sample sample{};
                    if (!ParseSample(line, sample)) { needsRewrite = true; continue; }
                    samples_.push_back(std::move(sample));
                }
            }
        }
    } else if (ec) {
        error = L"GOLD HISTORY: không kiểm tra được file lịch sử";
        return false;
    } else {
        needsRewrite = true;
    }

    std::sort(samples_.begin(), samples_.end(), [](const Sample& a, const Sample& b) {
        return a.unixSeconds < b.unixSeconds;
    });
    const std::int64_t cutoff = nowUnix > kRetentionSeconds ? nowUnix - kRetentionSeconds : 0;
    const auto oldSize = samples_.size();
    samples_.erase(std::remove_if(samples_.begin(), samples_.end(), [cutoff](const Sample& s) {
        return s.unixSeconds < cutoff;
    }), samples_.end());
    needsRewrite = needsRewrite || samples_.size() != oldSize;

    if (needsRewrite && !Rewrite(error)) return false;
    return true;
}

bool Store::AppendLine(const Sample& sample, std::wstring& error) {
    if (path_.empty()) { error = L"GOLD HISTORY: store chưa được mở"; return false; }
    std::ofstream out(path_, std::ios::binary | std::ios::app);
    if (!out) { error = L"GOLD HISTORY: không append được sample"; return false; }
    out << SerializeSample(sample) << "\n";
    if (!out) { error = L"GOLD HISTORY: ghi sample thất bại"; return false; }
    return true;
}

bool Store::Append(const Sample& sample, std::int64_t nowUnix, std::wstring& error) {
    error.clear();
    if (sample.unixSeconds <= 0 || sample.identity.empty()) {
        error = L"GOLD HISTORY: sample thiếu timestamp/identity";
        return false;
    }
    if (!AppendLine(sample, error)) return false;
    samples_.push_back(sample);
    if (samples_.size() >= 2 && samples_[samples_.size() - 2].unixSeconds > sample.unixSeconds) {
        std::sort(samples_.begin(), samples_.end(), [](const Sample& a, const Sample& b) {
            return a.unixSeconds < b.unixSeconds;
        });
    }
    if (lastPruneUnix_ == 0 || nowUnix - lastPruneUnix_ >= kAutoPruneIntervalSeconds) {
        return Prune(nowUnix, error);
    }
    return true;
}

bool Store::Prune(std::int64_t nowUnix, std::wstring& error) {
    error.clear();
    const std::int64_t cutoff = nowUnix > kRetentionSeconds ? nowUnix - kRetentionSeconds : 0;
    samples_.erase(std::remove_if(samples_.begin(), samples_.end(), [cutoff](const Sample& s) {
        return s.unixSeconds < cutoff;
    }), samples_.end());
    lastPruneUnix_ = nowUnix;
    return Rewrite(error);
}

bool Store::Rewrite(std::wstring& error) {
    error.clear();
    if (path_.empty()) { error = L"GOLD HISTORY: store chưa được mở"; return false; }
    std::ofstream out(path_, std::ios::binary | std::ios::trunc);
    if (!out) { error = L"GOLD HISTORY: không compact được file"; return false; }
    out << kHeader << "\n";
    for (const auto& sample : samples_) out << SerializeSample(sample) << "\n";
    if (!out) { error = L"GOLD HISTORY: compact file thất bại"; return false; }
    return true;
}

std::optional<Sample> Store::Latest(const std::wstring& identity) const {
    for (auto it = samples_.rbegin(); it != samples_.rend(); ++it) {
        if (it->identity == identity) return *it;
    }
    return std::nullopt;
}

std::optional<Sample> Store::Nearest(const std::wstring& identity,
                                     std::int64_t targetUnix,
                                     std::int64_t maxDistanceSeconds) const {
    if (identity.empty() || maxDistanceSeconds < 0) return std::nullopt;
    std::optional<Sample> best;
    std::int64_t bestDistance = std::numeric_limits<std::int64_t>::max();
    for (const auto& sample : samples_) {
        if (sample.identity != identity) continue;
        const std::int64_t distance = sample.unixSeconds >= targetUnix
            ? sample.unixSeconds - targetUnix : targetUnix - sample.unixSeconds;
        if (distance <= maxDistanceSeconds && distance < bestDistance) {
            best = sample;
            bestDistance = distance;
        }
    }
    return best;
}

bool Store::HasContinuousCoverage(const std::wstring& identity,
                                  std::int64_t startUnix,
                                  std::int64_t endUnix,
                                  std::int64_t endpointToleranceSeconds,
                                  std::int64_t maxGapSeconds) const {
    if (identity.empty() || startUnix > endUnix || endpointToleranceSeconds < 0 || maxGapSeconds <= 0) return false;
    const auto start = Nearest(identity, startUnix, endpointToleranceSeconds);
    const auto end = Nearest(identity, endUnix, endpointToleranceSeconds);
    if (!start || !end || start->unixSeconds > end->unixSeconds) return false;

    std::int64_t previous = start->unixSeconds;
    bool reachedEnd = previous == end->unixSeconds;
    for (const auto& sample : samples_) {
        if (sample.identity != identity || sample.unixSeconds <= previous) continue;
        if (sample.unixSeconds > end->unixSeconds) break;
        if (sample.unixSeconds - previous > maxGapSeconds) return false;
        previous = sample.unixSeconds;
        if (previous == end->unixSeconds) { reachedEnd = true; break; }
    }
    return reachedEnd && end->unixSeconds - previous <= maxGapSeconds;
}

} // namespace gold_history
