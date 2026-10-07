#include "auto_role_logic.h"
#include <cstdio>
#include <map>
#include <vector>

using auto_role_logic::AssignRuntimeRoles;
using auto_role_logic::ClientKey;

static int g_fail = 0;
static void Check(bool ok, const char* name) {
    if (!ok) { ++g_fail; std::printf("FAIL %s\n", name); }
}

static std::map<std::uint32_t, int> RolesByPid(const std::vector<ClientKey>& clients) {
    const auto roles = AssignRuntimeRoles(clients);
    std::map<std::uint32_t, int> out;
    for (std::size_t i = 0; i < clients.size(); ++i) out[clients[i].pid] = roles[i];
    return out;
}

int main() {
    std::vector<ClientKey> a = {
        {300, 30, true, false}, {100, 10, true, false}, {200, 20, true, true}, {0, 5, false, false}
    };
    auto r = RolesByPid(a);
    Check(r[20] == 1, "main:designated-only");
    Check(r[10] == 2 && r[30] == 3 && r[5] == 4, "con:roleid-then-pid-order");

    std::vector<ClientKey> reversed(a.rbegin(), a.rend());
    Check(RolesByPid(reversed) == r, "rescan:deterministic-independent-of-scan-order");

    for (auto& c : a) c.designatedMain = false;
    r = RolesByPid(a);
    bool noMain = true;
    for (const auto& [pid, role] : r) (void)pid, noMain = noMain && role != 1;
    Check(noMain, "missing-main:no-auto-promote");

    a[0].designatedMain = true;
    a[1].designatedMain = true;
    r = RolesByPid(a);
    noMain = true;
    for (const auto& [pid, role] : r) (void)pid, noMain = noMain && role != 1;
    Check(noMain, "ambiguous-main:fail-safe-no-main");

    std::vector<ClientKey> many;
    many.push_back({9999, 9999, true, true});
    for (std::uint32_t i = 0; i < 31; ++i) many.push_back({1000 + i, 100 + i, true, false});
    const auto manyRoles = AssignRuntimeRoles(many);
    int slots = 0, overflow = 0;
    for (std::size_t i = 1; i < manyRoles.size(); ++i) {
        if (manyRoles[i] >= 2 && manyRoles[i] <= 31) ++slots;
        if (manyRoles[i] == 32) ++overflow;
    }
    Check(manyRoles[0] == 1 && slots == 30 && overflow == 1, ">30:overflow-fail-safe");

    std::printf("RESULT %d/6 PASS\n", 6 - g_fail);
    return g_fail ? 1 : 0;
}
