#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

namespace auto_role_logic {

struct ClientKey {
    std::uint64_t roleId = 0;
    std::uint32_t pid = 0;
    bool hasRoleId = false;
    bool designatedMain = false;
};

inline bool StableChildLess(const ClientKey& lhs, const ClientKey& rhs) {
    if (lhs.hasRoleId != rhs.hasRoleId) return lhs.hasRoleId;
    if (lhs.hasRoleId && lhs.roleId != rhs.roleId) return lhs.roleId < rhs.roleId;
    return lhs.pid < rhs.pid;
}

// Returns one runtime role per input client.
// 1 = MAIN, 2..(1+childCount) = deterministic CON slots,
// firstChildRole+childCount = overflow CON (no scheduler slot).
inline std::vector<int> AssignRuntimeRoles(const std::vector<ClientKey>& clients,
                                           int mainRole = 1,
                                           int firstChildRole = 2,
                                           int childCount = 30) {
    const int overflowRole = firstChildRole + childCount;
    std::vector<int> roles(clients.size(), overflowRole);

    std::size_t mainIndex = clients.size();
    int mainMatches = 0;
    for (std::size_t i = 0; i < clients.size(); ++i) {
        if (!clients[i].designatedMain) continue;
        mainIndex = i;
        ++mainMatches;
    }
    if (mainMatches != 1) mainIndex = clients.size();
    if (mainIndex < clients.size()) roles[mainIndex] = mainRole;

    std::vector<std::size_t> childIndices;
    childIndices.reserve(clients.size());
    for (std::size_t i = 0; i < clients.size(); ++i)
        if (i != mainIndex) childIndices.push_back(i);
    std::stable_sort(childIndices.begin(), childIndices.end(), [&](std::size_t lhs, std::size_t rhs) {
        return StableChildLess(clients[lhs], clients[rhs]);
    });

    int nextRole = firstChildRole;
    for (std::size_t index : childIndices) {
        if (nextRole < overflowRole) roles[index] = nextRole++;
        else roles[index] = overflowRole;
    }
    return roles;
}

} // namespace auto_role_logic
