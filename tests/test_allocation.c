#include <stdio.h>
#include <assert.h>
#include "common.h"
#include "slot.h"
#include "allocation.h"
#include "manager.h"

static void test_category_compatibility_rules(void) {
    printf("[TEST] Testing vehicle category compatibility and accessibility rules...\n");

    /* Accessible pass vehicles MUST get accessible slot */
    assert(is_category_compatible(CAT_ACCESSIBLE, CAT_ACCESSIBLE) == true);
    assert(is_category_compatible(CAT_ACCESSIBLE, CAT_STANDARD) == false);

    /* Non-accessible CANNOT take accessible slot */
    assert(is_category_compatible(CAT_STANDARD, CAT_ACCESSIBLE) == false);
    assert(is_category_compatible(CAT_COMPACT, CAT_ACCESSIBLE) == false);
    assert(is_category_compatible(CAT_SUV, CAT_ACCESSIBLE) == false);

    /* EV exclusivity */
    assert(is_category_compatible(CAT_EV, CAT_EV) == true);
    assert(is_category_compatible(CAT_STANDARD, CAT_EV) == false);
    assert(is_category_compatible(CAT_SUV, CAT_EV) == false);

    /* SUV sizing */
    assert(is_category_compatible(CAT_SUV, CAT_SUV) == true);
    assert(is_category_compatible(CAT_SUV, CAT_COMPACT) == false);
    assert(is_category_compatible(CAT_SUV, CAT_STANDARD) == false);

    /* Compact flexibility */
    assert(is_category_compatible(CAT_COMPACT, CAT_COMPACT) == true);
    assert(is_category_compatible(CAT_COMPACT, CAT_STANDARD) == true);
    assert(is_category_compatible(CAT_COMPACT, CAT_SUV) == true);

    printf("  --> PASSED: All compatibility lattice assertions verified.\n");
}

static void test_greedy_vs_best_fit_cannibalization(void) {
    printf("[TEST] Testing slot cannibalization (Greedy vs Best-Fit divergence)...\n");

    /* Create a miniature parking lot:
     * Slot 1: SUV Slot, Distance = 10m (closer to entrance)
     * Slot 2: Compact Slot, Distance = 25m (further from entrance)
     */
    ParkingSlot slots_greedy[2];
    memset(slots_greedy, 0, sizeof(slots_greedy));
    slots_greedy[0].slot_id = 1;
    slots_greedy[0].slot_cat = CAT_SUV;
    slots_greedy[0].distance_from_entrance = 10.0;

    slots_greedy[1].slot_id = 2;
    slots_greedy[1].slot_cat = CAT_COMPACT;
    slots_greedy[1].distance_from_entrance = 25.0;

    /* Replicate for Best-Fit */
    ParkingSlot slots_bestfit[2];
    memcpy(slots_bestfit, slots_greedy, sizeof(slots_bestfit));

    time_t t_start = 100000;
    time_t t_end   = 107200;

    /* 1. Request arrives for a COMPACT car */
    AllocationRequest req1;
    memset(&req1, 0, sizeof(AllocationRequest));
    strcpy(req1.vehicle_reg, "CH-01-COMPACT");
    req1.vehicle_cat = CAT_COMPACT;
    req1.start_time = t_start;
    req1.end_time = t_end;

    /* Greedy chooses Slot 1 (Distance 10m < 25m), even though it is an SUV slot! */
    req1.strategy = ALLOC_GREEDY_DISTANCE;
    AllocationResult res_greedy_1 = process_allocation_request(slots_greedy, 2, &req1);
    assert(res_greedy_1.status == STATUS_OK);
    assert(res_greedy_1.allocated_slot_id == 1); /* Cannibalized SUV slot! */

    /* Best-Fit chooses Slot 2 (Exact category match, 0 dimension waste), despite greater distance */
    req1.strategy = ALLOC_BEST_FIT;
    AllocationResult res_bestfit_1 = process_allocation_request(slots_bestfit, 2, &req1);
    assert(res_bestfit_1.status == STATUS_OK);
    assert(res_bestfit_1.allocated_slot_id == 2); /* Preserved SUV slot! */

    /* 2. Now a second request arrives for an SUV! */
    AllocationRequest req2;
    memset(&req2, 0, sizeof(AllocationRequest));
    strcpy(req2.vehicle_reg, "CH-01-SUV");
    req2.vehicle_cat = CAT_SUV;
    req2.start_time = t_start;
    req2.end_time = t_end;

    /* Under Greedy: Slot 1 is taken by the compact car, Slot 2 cannot fit an SUV -> REJECTED! */
    req2.strategy = ALLOC_GREEDY_DISTANCE;
    AllocationResult res_greedy_2 = process_allocation_request(slots_greedy, 2, &req2);
    assert(res_greedy_2.status == ERR_NO_COMPATIBLE_SLOT);

    /* Under Best-Fit: Slot 1 (SUV) was preserved -> ACCEPTED! */
    req2.strategy = ALLOC_BEST_FIT;
    AllocationResult res_bestfit_2 = process_allocation_request(slots_bestfit, 2, &req2);
    assert(res_bestfit_2.status == STATUS_OK);
    assert(res_bestfit_2.allocated_slot_id == 1);

    /* Clean up allocated reservation nodes */
    free_slot_reservations(&slots_greedy[0]);
    free_slot_reservations(&slots_greedy[1]);
    free_slot_reservations(&slots_bestfit[0]);
    free_slot_reservations(&slots_bestfit[1]);

    printf("  --> PASSED: Verified that Best-Fit prevents slot cannibalization where Greedy fails.\n");
}

int main(void) {
    printf("====================================================\n");
    printf(" UNIT TEST SUITE: Allocation & Strategy Engines     \n");
    printf("====================================================\n");

    test_category_compatibility_rules();
    test_greedy_vs_best_fit_cannibalization();

    printf("\n>>> ALL ALLOCATION SUITE TESTS COMPLETED SUCCESSFULLY! <<<\n\n");
    return 0;
}
