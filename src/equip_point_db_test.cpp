#include "equip_point_db.h"
#include <cassert>
#include <iostream>
#include <string>

int main() {
    EquipPointDb db;
    std::string error;
    const std::string csv =
        "ID,EquipPoint\n"
        "10102014,0\n"
        "10122010,6\n";
    assert(db.LoadCsv(csv, &error));
    assert(db.Size() == 2);
    assert(db.Find(10102014).has_value() && *db.Find(10102014) == 0);
    assert(db.Find(10122010).has_value() && *db.Find(10122010) == 6);
    assert(!db.Find(99999999).has_value());

    EquipPointDb samePointDup;
    assert(samePointDup.LoadCsv("ID,EquipPoint\n1,0\n1,0\n", &error));
    assert(samePointDup.Size() == 2);
    assert(*samePointDup.Find(1) == 0);

    EquipPointDb conflictingDup;
    assert(!conflictingDup.LoadCsv("ID,EquipPoint\n1,0\n1,6\n", &error));

    EquipPointDb malformed;
    assert(!malformed.LoadCsv("ID,EquipPoint\nabc,0\n", &error));
    assert(!malformed.LoadCsv("ID,EquipPoint\n1,notnum\n", &error));

    EquipPointDb bom;
    assert(bom.LoadCsv("\xEF\xBB\xBFID,EquipPoint\r\n10122010,6\r\n", &error));
    assert(*bom.Find(10122010) == 6);

    std::cout << "equip_point_db PASS\n";
}
