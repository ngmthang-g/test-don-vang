#include "gold_history_store.h"

#include <filesystem>
#include <string>

int main() {
    namespace fs = std::filesystem;
    const fs::path path = fs::temp_directory_path() / "tl_gold_history_store_test.tsv";
    std::error_code ec; fs::remove(path, ec);

    gold_history::Store store;
    std::wstring error;
    constexpr std::int64_t now = 2'000'000'000LL;
    if (!store.Open(path, now, error)) return 101;
    if (!store.Append({now - 7200, L"Role_1", L"MAIN A", 1'000'000}, now, error)) return 102;
    if (!store.Append({now - 3605, L"Role_1", L"MAIN A", 1'500'000}, now, error)) return 103;
    if (!store.Append({now - 20, L"Role_1", L"MAIN A", 2'000'000}, now, error)) return 104;
    if (!store.Append({now - 10, L"Role_2", L"MAIN B", 9'000'000}, now, error)) return 105;

    const auto latest = store.Latest(L"Role_1");
    if (!latest || latest->boundMoneyRaw != 2'000'000) return 106;
    const auto hour = store.Nearest(L"Role_1", now - 3600, 45);
    if (!hour || hour->boundMoneyRaw != 1'500'000) return 107;
    if (store.Nearest(L"Role_1", now - 1000, 45)) return 108;
    // This intentionally contains a large internal gap, so a 60-minute report must be N/A.
    if (store.HasContinuousCoverage(L"Role_1", now - 3600, now, 45, 90)) return 109;

    // A dense stream around the full 60-minute window must be accepted.
    for (std::int64_t t = now - 3575; t <= now - 35; t += 30) {
        if (!store.Append({t, L"Role_3", L"MAIN C", 3'000'000 + t}, now, error)) return 110;
    }
    if (!store.Append({now - 5, L"Role_3", L"MAIN C", 4'000'000}, now, error)) return 111;
    if (!store.HasContinuousCoverage(L"Role_3", now - 3600, now, 45, 90)) return 112;

    // Re-open proves persistence and identity isolation.
    gold_history::Store reopened;
    if (!reopened.Open(path, now, error)) return 113;
    const auto role2 = reopened.Latest(L"Role_2");
    if (!role2 || role2->boundMoneyRaw != 9'000'000) return 114;

    // Out-of-order append must not corrupt Latest(), and >3-day data is pruned.
    if (!reopened.Append({now - gold_history::kRetentionSeconds - 1, L"Role_1", L"OLD", 1}, now, error)) return 115;
    const auto latestAfterOldAppend = reopened.Latest(L"Role_1");
    if (!latestAfterOldAppend || latestAfterOldAppend->boundMoneyRaw != 2'000'000) return 116;
    if (!reopened.Prune(now, error)) return 117;
    for (const auto& s : reopened.Samples()) if (s.boundMoneyRaw == 1) return 118;

    fs::remove(path, ec);
    return 0;
}
