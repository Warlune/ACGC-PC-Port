#ifndef PC_WORLD_PROGRESSION_H
#define PC_WORLD_PROGRESSION_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum pc_world_region_e {
    PC_WORLD_REGION_WEST,
    PC_WORLD_REGION_EAST
} pc_world_region_t;

/* Numeric home tiers are stable save-format values, also used by pure tests. */
#define PC_WORLD_HOME_SIZE_SMALL 0
#define PC_WORLD_HOME_SIZE_MEDIUM 1
#define PC_WORLD_HOME_SIZE_LARGE 2
#define PC_WORLD_HOME_SIZE_UPPER 3
#define PC_WORLD_HOME_SIZE_STATUE 4

/* Stable native collision attribute values used to preserve water semantics. */
#define PC_WORLD_COLLISION_ATTRIBUTE_WAVE 11
#define PC_WORLD_COLLISION_ATTRIBUTE_WATER_FIRST 12
#define PC_WORLD_COLLISION_ATTRIBUTE_RIVER_LAST 21
#define PC_WORLD_COLLISION_ATTRIBUTE_SEA 24
#define PC_WORLD_COLLISION_ATTRIBUTE_WAVE_SE2 25
#define PC_WORLD_COLLISION_ATTRIBUTE_WAVE_SW2 26
#define PC_WORLD_COLLISION_ATTRIBUTE_WAVE_S 36
#define PC_WORLD_COLLISION_ATTRIBUTE_WAVE_SW 37
#define PC_WORLD_COLLISION_ATTRIBUTE_WAVE_SE 38

/* A home tier is durable evidence that the preceding mortgage was completed. */
int pc_world_progression_unlocked_from_home_sizes(const unsigned char* sizes, int count,
                                                  pc_world_region_t region);

int pc_world_progression_unlocked_from_progress(const unsigned char* sizes,
                                                const unsigned char* paid_loans,
                                                int count, pc_world_region_t region);
unsigned char pc_world_progression_updated_pad(unsigned char pad, unsigned char home_size);
unsigned char pc_world_progression_mark_legacy_house_debt(unsigned char pad,
                                                         unsigned char home_size,
                                                         unsigned int loan,
                                                         int first_job_active,
                                                         int nook_talk_active);
unsigned char pc_world_progression_pad_after_payment(unsigned char pad, unsigned char home_size,
                                                     unsigned int previous_loan,
                                                     unsigned int remaining_loan,
                                                     unsigned int first_house_loan);
unsigned char pc_world_progression_gate_collision_attribute(int unlocked,
                                                           unsigned char terrain_attribute,
                                                           unsigned char wall_attribute);

/* Unlocks are shared: the highest completed tier of any existing resident counts. */
int pc_world_progression_region_unlocked(pc_world_region_t region);
int pc_world_progression_unlocked_at_test_stage(int stage, pc_world_region_t region);
const char* pc_world_progression_notice_at(float world_x, float world_z);

/* Record repayment progress in the selected resident's existing save data. */
void pc_world_progression_record_loan_payment(int player_no, unsigned int previous_loan,
                                              unsigned int remaining_loan,
                                              unsigned int first_house_loan);

#ifdef __cplusplus
}
#endif

#endif
