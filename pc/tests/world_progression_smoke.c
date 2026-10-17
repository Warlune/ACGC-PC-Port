#include "pc_world_progression.h"

#include <stdio.h>

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "world progression check failed at line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main(void) {
    unsigned char sizes[4] = { 0, 0, 0, 0 };
    unsigned char paid[4] = { 0, 0, 0, 0 };

    REQUIRE(pc_world_progression_pad_after_payment(0, 0, 0, 0, 17400) == 0);
    REQUIRE(pc_world_progression_pad_after_payment(0, 0, 100, 0, 17400) == 0);
    REQUIRE(pc_world_progression_pad_after_payment(0, 0, 17400, 100, 17400) == 4);
    REQUIRE(pc_world_progression_pad_after_payment(4, 0, 100, 0, 17400) == 1);
    REQUIRE(pc_world_progression_pad_after_payment(8, 0, 398000, 0, 17400) == 9);
    REQUIRE(pc_world_progression_pad_after_payment(1, 1, 398000, 12000, 17400) == 5);
    REQUIRE(pc_world_progression_pad_after_payment(5, 1, 12000, 0, 17400) == 2);
    REQUIRE(pc_world_progression_mark_legacy_house_debt(8, 0, 12000, 0, 0) == 12);
    REQUIRE(pc_world_progression_mark_legacy_house_debt(8, 0, 100, 1, 0) == 8);
    REQUIRE(pc_world_progression_mark_legacy_house_debt(8, 0, 100, 0, 0) == 8);
    REQUIRE(pc_world_progression_mark_legacy_house_debt(8, 0, 100, 0, 1) == 12);
    REQUIRE(pc_world_progression_mark_legacy_house_debt(8, 0, 50, 0, 0) == 8);
    REQUIRE(pc_world_progression_mark_legacy_house_debt(8, 0, 50, 0, 1) == 12);
    REQUIRE(pc_world_progression_mark_legacy_house_debt(8, 0, 0, 0, 0) == 8);
    REQUIRE(pc_world_progression_mark_legacy_house_debt(8, 1, 12000, 0, 1) == 8);
    REQUIRE(pc_world_progression_pad_after_payment(
                pc_world_progression_mark_legacy_house_debt(0, 0, 12000, 0, 0),
                0, 12000, 0, 17400) == 1);
    REQUIRE(pc_world_progression_unlocked_at_test_stage(0, PC_WORLD_REGION_EAST) == 0);
    REQUIRE(pc_world_progression_unlocked_at_test_stage(0, PC_WORLD_REGION_WEST) == 0);
    REQUIRE(pc_world_progression_unlocked_at_test_stage(1, PC_WORLD_REGION_EAST) == 1);
    REQUIRE(pc_world_progression_unlocked_at_test_stage(1, PC_WORLD_REGION_WEST) == 0);
    REQUIRE(pc_world_progression_unlocked_at_test_stage(2, PC_WORLD_REGION_EAST) == 1);
    REQUIRE(pc_world_progression_unlocked_at_test_stage(2, PC_WORLD_REGION_WEST) == 1);
    REQUIRE(pc_world_progression_unlocked_at_test_stage(3, PC_WORLD_REGION_WEST) == 0);
    REQUIRE(pc_world_progression_updated_pad(12, 0) == 9);
    REQUIRE(pc_world_progression_updated_pad(13, 0) == 10);
    REQUIRE(pc_world_progression_updated_pad(14, 0) == 10);
    REQUIRE(pc_world_progression_updated_pad(8, 1) == 10);
    REQUIRE(pc_world_progression_updated_pad(0xA4, 0) == 0xA1);
    REQUIRE(pc_world_progression_gate_collision_attribute(0, 7, 33) == 33);
    REQUIRE(pc_world_progression_gate_collision_attribute(1, 7, 33) == 7);
    REQUIRE(pc_world_progression_gate_collision_attribute(0, 9, 34) == 34);
    REQUIRE(pc_world_progression_gate_collision_attribute(1, 9, 34) == 9);
    REQUIRE(pc_world_progression_gate_collision_attribute(
                0, PC_WORLD_COLLISION_ATTRIBUTE_SEA, 33) == PC_WORLD_COLLISION_ATTRIBUTE_SEA);
    REQUIRE(pc_world_progression_gate_collision_attribute(
                1, PC_WORLD_COLLISION_ATTRIBUTE_SEA, 33) == PC_WORLD_COLLISION_ATTRIBUTE_SEA);
    REQUIRE(pc_world_progression_gate_collision_attribute(
                0, PC_WORLD_COLLISION_ATTRIBUTE_WATER_FIRST, 34) ==
            PC_WORLD_COLLISION_ATTRIBUTE_WATER_FIRST);

    REQUIRE(!pc_world_progression_unlocked_from_home_sizes(sizes, 4, PC_WORLD_REGION_EAST));
    REQUIRE(!pc_world_progression_unlocked_from_home_sizes(sizes, 4, PC_WORLD_REGION_WEST));

    /* Zero initial loan / partial repayment do not record a completed loan. */
    REQUIRE(!pc_world_progression_unlocked_from_progress(sizes, paid, 4, PC_WORLD_REGION_EAST));
    REQUIRE(!pc_world_progression_unlocked_from_progress(sizes, paid, 4, PC_WORLD_REGION_WEST));
    paid[0] = 1; /* Full payoff of the first qualifying house loan. */
    REQUIRE(pc_world_progression_unlocked_from_progress(sizes, paid, 4, PC_WORLD_REGION_EAST));
    REQUIRE(!pc_world_progression_unlocked_from_progress(sizes, paid, 4, PC_WORLD_REGION_WEST));
    paid[0] = 0; /* Repeated zero balance is not another payoff event. */
    sizes[0] = 1; /* First loan fully paid and first home expansion completed. */
    REQUIRE(pc_world_progression_unlocked_from_home_sizes(sizes, 4, PC_WORLD_REGION_EAST));
    REQUIRE(!pc_world_progression_unlocked_from_home_sizes(sizes, 4, PC_WORLD_REGION_WEST));

    /* Shared progression: one resident's completed tier unlocks for everyone. */
    sizes[0] = 0;
    sizes[2] = 2; /* Second home loan paid. */
    REQUIRE(pc_world_progression_unlocked_from_home_sizes(sizes, 4, PC_WORLD_REGION_EAST));
    REQUIRE(pc_world_progression_unlocked_from_home_sizes(sizes, 4, PC_WORLD_REGION_WEST));

    sizes[2] = 1;
    paid[2] = 2; /* Second full payoff, before choosing/building the expansion. */
    REQUIRE(pc_world_progression_unlocked_from_progress(sizes, paid, 4, PC_WORLD_REGION_WEST));

    /* Existing maximum upgrades preserve both unlocks; corrupt tier values are ignored. */
    sizes[2] = 4;
    REQUIRE(pc_world_progression_unlocked_from_home_sizes(sizes, 4, PC_WORLD_REGION_EAST));
    REQUIRE(pc_world_progression_unlocked_from_home_sizes(sizes, 4, PC_WORLD_REGION_WEST));
    sizes[2] = 7;
    paid[2] = 0;
    REQUIRE(!pc_world_progression_unlocked_from_home_sizes(sizes, 4, PC_WORLD_REGION_EAST));
    REQUIRE(!pc_world_progression_unlocked_from_home_sizes(sizes, 4, PC_WORLD_REGION_WEST));

    /* Empty inputs and unknown regions fail closed. */
    REQUIRE(!pc_world_progression_unlocked_from_home_sizes(sizes, 0, PC_WORLD_REGION_EAST));
    REQUIRE(!pc_world_progression_unlocked_from_home_sizes(NULL, 4, PC_WORLD_REGION_EAST));
    REQUIRE(!pc_world_progression_unlocked_from_home_sizes(sizes, 4, (pc_world_region_t)99));

    puts("world progression smoke passed");
    return 0;
}
