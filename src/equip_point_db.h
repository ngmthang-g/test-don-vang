#pragma once
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class EquipPointDb {
public:
    bool LoadCsv(std::string_view text, std::string* error = nullptr) {
        entries_.clear();
        if (error) error->clear();
        if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
            static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
            text.remove_prefix(3);
        }
        bool first = true;
        std::size_t lineNo = 0;
        while (!text.empty()) {
            std::size_t nl = text.find('\n');
            std::string_view line = nl == std::string_view::npos ? text : text.substr(0, nl);
            text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
            ++lineNo;
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            if (line.empty()) continue;
            if (first) {
                first = false;
                if (line != "ID,EquipPoint") return Fail(error, "header must be ID,EquipPoint");
                continue;
            }
            const std::size_t comma = line.find(',');
            if (comma == std::string_view::npos || line.find(',', comma + 1) != std::string_view::npos)
                return FailLine(error, lineNo, "expected exactly two columns");
            std::int32_t id = 0;
            int point = 0;
            if (!ParseInt32(line.substr(0, comma), id) || id <= 0)
                return FailLine(error, lineNo, "invalid ID");
            if (!ParseInt(line.substr(comma + 1), point))
                return FailLine(error, lineNo, "invalid EquipPoint");
            entries_.push_back({id, point});
        }
        if (first) return Fail(error, "missing header");
        std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b){ return a.first < b.first; });
        for (std::size_t i = 1; i < entries_.size(); ++i) {
            if (entries_[i - 1].first == entries_[i].first && entries_[i - 1].second != entries_[i].second)
                return Fail(error, "conflicting EquipPoint for ItemID " + std::to_string(entries_[i].first));
        }
        return true;
    }

    std::optional<int> Find(std::int32_t itemID) const {
        const auto it = std::lower_bound(entries_.begin(), entries_.end(), itemID,
            [](const Entry& entry, std::int32_t value){ return entry.first < value; });
        if (it == entries_.end() || it->first != itemID) return std::nullopt;
        return it->second;
    }

    std::size_t Size() const { return entries_.size(); }

private:
    using Entry = std::pair<std::int32_t, int>;
    std::vector<Entry> entries_{};

    static bool ParseInt32(std::string_view s, std::int32_t& out) {
        if (s.empty()) return false;
        std::int64_t tmp = 0;
        const char* begin = s.data(); const char* end = s.data() + s.size();
        auto result = std::from_chars(begin, end, tmp);
        if (result.ec != std::errc{} || result.ptr != end || tmp < INT32_MIN || tmp > INT32_MAX) return false;
        out = static_cast<std::int32_t>(tmp); return true;
    }

    static bool ParseInt(std::string_view s, int& out) {
        if (s.empty()) return false;
        const char* begin = s.data(); const char* end = s.data() + s.size();
        auto result = std::from_chars(begin, end, out);
        return result.ec == std::errc{} && result.ptr == end;
    }

    static bool Fail(std::string* error, std::string message) {
        if (error) *error = std::move(message);
        return false;
    }

    static bool FailLine(std::string* error, std::size_t lineNo, const char* message) {
        return Fail(error, "line " + std::to_string(lineNo) + ": " + message);
    }
};
