#include "party_build_logic.h"
#include <cassert>

int main() {
    using namespace party_build_logic;
    assert(CanBePartyKey(false, 3, true));
    assert(!CanBePartyKey(true, 3, true));
    assert(!CanBePartyKey(false, 0, true));
    assert(!CanBePartyKey(false, 3, false));

    assert(IsInviteMember(false, 3, 3, 2002, 2001));
    assert(!IsInviteMember(true, 3, 3, 2002, 2001));
    assert(!IsInviteMember(false, 2, 3, 2002, 2001));
    assert(!IsInviteMember(false, 3, 3, 2001, 2001));
    assert(!IsInviteMember(false, 3, 3, 0, 2001));

    assert(ClampRetry(0) == 1);
    assert(ClampRetry(4) == 4);
    assert(ClampRetry(99) == 20);
    return 0;
}
