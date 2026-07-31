#include "../el_native/features/energy_attack_speed.h"
#include <cassert>

int main() {
    assert(StatCompare::AppliedValue(1.0f, 1.0f) == 1.0f);
    assert(StatCompare::AppliedValue(1.2f, 2.5f) == 3.0f);
    assert(StatCompare::AppliedValue(1.2f, 0.0f) == 1.2f);
    return 0;
}
