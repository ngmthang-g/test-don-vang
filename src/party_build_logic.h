#pragma once

namespace party_build_logic {

inline bool CanBePartyKey(bool isMain, int displayParty, bool partyKey) {
    return !isMain && displayParty > 0 && partyKey;
}

inline bool IsInviteMember(bool memberIsMain, int memberParty, int keyParty,
                           int memberRoleId, int keyRoleId) {
    return !memberIsMain && keyParty > 0 && memberParty == keyParty &&
           memberRoleId > 0 && memberRoleId != keyRoleId;
}

inline int ClampRetry(int value) {
    if (value < 1) return 1;
    if (value > 20) return 20;
    return value;
}

} // namespace party_build_logic
