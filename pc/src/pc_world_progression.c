#include "pc_world_progression.h"

#ifndef PC_WORLD_PROGRESSION_TEST
#include "m_common_data.h"
#include "m_home_h.h"
#include "m_house.h"
#include "m_private.h"
#include "m_event.h"

#ifdef ACO_DEV_EAST
extern int g_pc_dev_east;
extern int g_pc_dev_unlock_regions;
extern int g_pc_dev_progress_stage;
#endif
#endif

int pc_world_progression_unlocked_from_home_sizes(const unsigned char* sizes, int count,
                                                  pc_world_region_t region) {
    return pc_world_progression_unlocked_from_progress(sizes, 0, count, region);
}

int pc_world_progression_unlocked_from_progress(const unsigned char* sizes,
                                                const unsigned char* paid_loans,
                                                int count, pc_world_region_t region) {
    int needed_size;
    int needed_loans;
    int i;

    if (sizes == 0 || count <= 0) {
        return 0;
    }

    switch (region) {
        case PC_WORLD_REGION_EAST:
            needed_size = PC_WORLD_HOME_SIZE_MEDIUM;
            needed_loans = 1;
            break;
        case PC_WORLD_REGION_WEST:
            needed_size = PC_WORLD_HOME_SIZE_LARGE;
            needed_loans = 2;
            break;
        default:
            return 0;
    }

    for (i = 0; i < count; i++) {
        if ((sizes[i] <= PC_WORLD_HOME_SIZE_STATUE && sizes[i] >= needed_size) ||
            (paid_loans != 0 && paid_loans[i] >= needed_loans)) {
            return 1;
        }
    }

    return 0;
}

unsigned char pc_world_progression_updated_pad(unsigned char pad, unsigned char home_size) {
    int paid_count = pad & 3;
    int tier_count = home_size >= PC_WORLD_HOME_SIZE_LARGE ? 2 :
                     home_size >= PC_WORLD_HOME_SIZE_MEDIUM ? 1 : 0;

    if (paid_count < tier_count) {
        paid_count = tier_count;
    }
    if (paid_count < 2) {
        paid_count++;
    }

    /* Bits 0-1 count completed debts; bit 2 is cleared at payoff. Preserve
     * every unrelated pad bit because this byte belongs to the native save. */
    return (pad & 0xF8) | paid_count;
}

unsigned char pc_world_progression_mark_legacy_house_debt(unsigned char pad,
                                                         unsigned char home_size,
                                                         unsigned int loan,
                                                         int first_job_active,
                                                         int nook_talk_active) {
    /* Existing saves may already have partial mortgage payments. Native intro
     * sets the first house loan before first-job completion. A balance above
     * the scripted 100-unit work debt is unequivocal house-debt evidence even
     * if an older dev boot left HRAWAIT set. At <=100, require native Nook-talk
     * evidence because the work debt could have been partially repaid there. */
    if (home_size == PC_WORLD_HOME_SIZE_SMALL && loan != 0 && !first_job_active &&
        (loan > 100 || nook_talk_active)) {
        pad |= 4;
    }
    return pad;
}

int pc_world_progression_unlocked_at_test_stage(int stage, pc_world_region_t region) {
    if (stage < 0 || stage > 2) return 0;
    if (region == PC_WORLD_REGION_EAST) return stage >= 1;
    if (region == PC_WORLD_REGION_WEST) return stage >= 2;
    return 0;
}

unsigned char pc_world_progression_pad_after_payment(unsigned char pad, unsigned char home_size,
                                                     unsigned int previous_loan,
                                                     unsigned int remaining_loan,
                                                     unsigned int first_house_loan) {
    if (previous_loan >= first_house_loan) {
        pad |= 4; /* Remember a qualifying debt across partial payments and reloads. */
    }
    if (remaining_loan != 0 || previous_loan == 0 || (pad & 4) == 0) {
        return pad;
    }
    return pc_world_progression_updated_pad(pad, home_size);
}

unsigned char pc_world_progression_gate_collision_attribute(int unlocked,
                                                           unsigned char terrain_attribute,
                                                           unsigned char wall_attribute) {
    if (terrain_attribute == PC_WORLD_COLLISION_ATTRIBUTE_WAVE ||
        (terrain_attribute >= PC_WORLD_COLLISION_ATTRIBUTE_WATER_FIRST &&
         terrain_attribute <= PC_WORLD_COLLISION_ATTRIBUTE_RIVER_LAST) ||
        terrain_attribute == PC_WORLD_COLLISION_ATTRIBUTE_SEA ||
        terrain_attribute == PC_WORLD_COLLISION_ATTRIBUTE_WAVE_SE2 ||
        terrain_attribute == PC_WORLD_COLLISION_ATTRIBUTE_WAVE_SW2 ||
        (terrain_attribute >= PC_WORLD_COLLISION_ATTRIBUTE_WAVE_S &&
         terrain_attribute <= PC_WORLD_COLLISION_ATTRIBUTE_WAVE_SE)) {
        return terrain_attribute;
    }
    return unlocked ? terrain_attribute : wall_attribute;
}

#ifndef PC_WORLD_PROGRESSION_TEST
int pc_world_progression_region_unlocked(pc_world_region_t region) {
    unsigned char sizes[PLAYER_NUM];
    unsigned char paid_loans[PLAYER_NUM];
    int count = 0;
    int i;

#ifdef ACO_DEV_EAST
    /* Expanded towns are open for exploration. Keep the native shoreline
     * collision and suppress regional fences, including in older dev launchers
     * that still supply a mortgage-stage override. */
    if (g_pc_dev_east) {
        return region == PC_WORLD_REGION_WEST || region == PC_WORLD_REGION_EAST;
    }
#endif

    for (i = 0; i < PLAYER_NUM; i++) {
        Private_c* resident = &Save_Get(private_data[i]);
        int home_no;

        if (mPr_CheckPrivate(resident) == FALSE || resident->exists == FALSE) {
            continue;
        }

        home_no = mHS_get_arrange_idx(i);
        if (home_no < 0 || home_no >= PLAYER_NUM) {
            continue;
        }

        mHm_rmsz_c* home_size = &Save_Get(homes[home_no]).size_info;
        int first_job_active = mEv_CheckEvent(mEv_SAVED_FIRSTJOB_PLR0 + i) == TRUE;
        int nook_talk_active = mEv_CheckEvent(mEv_SAVED_HRATALK_PLR0 + i) == TRUE;
        home_size->pad = pc_world_progression_mark_legacy_house_debt(
            home_size->pad, home_size->size, resident->inventory.loan, first_job_active,
            nook_talk_active);
        sizes[count++] = home_size->size;
        paid_loans[count - 1] = home_size->pad & 3;
    }

    return pc_world_progression_unlocked_from_progress(sizes, paid_loans, count, region);
}
const char* pc_world_progression_notice_at(float world_x, float world_z) {
    static const float seam_x[2] = {
        (float)(ACO_TOWN_X0 + 7) * 640.0f,
        (float)(ACO_TOWN_X0 + 14) * 640.0f
    };
    static const char* notice[2] = {
        "Pay off two house loans to open the western coast.",
        "Pay off your first house loan to open the eastern coast."
    };

#ifdef ACO_DEV_EAST
    if (!g_pc_dev_east || world_z < 6.0f * 640.0f || world_z >= 7.0f * 640.0f) {
        return 0;
    }
#else
    (void)world_z;
    return 0;
#endif

    for (int i = 0; i < 2; i++) {
        pc_world_region_t region = i == 0 ? PC_WORLD_REGION_WEST : PC_WORLD_REGION_EAST;
        float distance = world_x - seam_x[i];
        if (distance < 0.0f) distance = -distance;
        if (distance <= 140.0f && !pc_world_progression_region_unlocked(region)) {
            return notice[i];
        }
    }
    return 0;
}

void pc_world_progression_record_loan_payment(int player_no, unsigned int previous_loan,
                                              unsigned int remaining_loan,
                                              unsigned int first_house_loan) {
    int home_no;
    mHm_rmsz_c* size;
    Private_c* resident;

    if (player_no < 0 || player_no >= PLAYER_NUM) {
        return;
    }

    resident = &Save_Get(private_data[player_no]);
    if (mPr_CheckPrivate(resident) == FALSE || resident->exists == FALSE) {
        return;
    }

    home_no = mHS_get_arrange_idx(player_no);
    if (home_no < 0 || home_no >= PLAYER_NUM) {
        return;
    }

    size = &Save_Get(homes[home_no]).size_info;
    size->pad = pc_world_progression_pad_after_payment(size->pad, size->size, previous_loan,
                                                       remaining_loan, first_house_loan);
}
#endif
