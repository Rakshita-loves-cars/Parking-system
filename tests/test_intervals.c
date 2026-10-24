#include <stdio.h>
#include <assert.h>
#include "common.h"
#include "slot.h"

static void test_half_open_interval_overlap(void) {
    printf("[TEST] Running half-open interval overlap unit tests...\n");

    time_t t10 = 10000;
    time_t t11 = 13600;
    time_t t12 = 17200;
    time_t t13 = 20800;
    time_t t14 = 24400;

    /* 1. Exact back-to-back touching boundary: [10, 12) and [12, 14) -> NO OVERLAP */
    assert(check_interval_overlap(t10, t12, t12, t14) == false);
    assert(check_interval_overlap(t12, t14, t10, t12) == false);

    /* 2. Completely disjoint: [10, 11) and [13, 14) -> NO OVERLAP */
    assert(check_interval_overlap(t10, t11, t13, t14) == false);
    assert(check_interval_overlap(t13, t14, t10, t11) == false);

    /* 3. Sub-interval inside: [11, 12) inside [10, 13) -> OVERLAP */
    assert(check_interval_overlap(t10, t13, t11, t12) == true);
    assert(check_interval_overlap(t11, t12, t10, t13) == true);

    /* 4. Partial head overlap: [10, 12) and [11, 13) -> OVERLAP */
    assert(check_interval_overlap(t10, t12, t11, t13) == true);

    /* 5. Identical interval: [10, 12) and [10, 12) -> OVERLAP */
    assert(check_interval_overlap(t10, t12, t10, t12) == true);

    printf("  --> PASSED: All 5 half-open interval math assertions succeeded.\n");
}

static void test_chronological_list_insertion(void) {
    printf("[TEST] Running chronological linked list insertion tests...\n");

    ParkingSlot slot;
    memset(&slot, 0, sizeof(ParkingSlot));
    slot.slot_id = 1;
    slot.slot_cat = CAT_STANDARD;

    time_t t_09 = 90000;
    time_t t_11 = 97200;
    time_t t_13 = 104400;
    time_t t_15 = 111600;
    time_t t_17 = 118800;
    time_t t_19 = 126000;

    /* Insert in scrambled order: middle [13, 15), then later [17, 19), then early [09, 11) */
    ReservationNode* n2 = create_reservation_node("TKT-2", "VEH-2", CAT_STANDARD, t_13, t_15);
    ReservationNode* n3 = create_reservation_node("TKT-3", "VEH-3", CAT_STANDARD, t_17, t_19);
    ReservationNode* n1 = create_reservation_node("TKT-1", "VEH-1", CAT_STANDARD, t_09, t_11);

    assert(add_reservation_to_slot(&slot, n2) == STATUS_OK);
    assert(add_reservation_to_slot(&slot, n3) == STATUS_OK);
    assert(add_reservation_to_slot(&slot, n1) == STATUS_OK);
    assert(slot.active_reservations_count == 3);

    /* Verify list order is strictly chronological: TKT-1 -> TKT-2 -> TKT-3 */
    ReservationNode* cur = slot.reservation_head;
    assert(cur != NULL && strcmp(cur->ticket_id, "TKT-1") == 0);
    cur = cur->next;
    assert(cur != NULL && strcmp(cur->ticket_id, "TKT-2") == 0);
    cur = cur->next;
    assert(cur != NULL && strcmp(cur->ticket_id, "TKT-3") == 0);
    assert(cur->next == NULL);

    /* Attempt to insert conflicting reservation [10, 12) which overlaps with [09, 11) */
    ReservationNode* conflict = create_reservation_node("TKT-X", "VEH-X", CAT_STANDARD, 93600, 100800);
    assert(add_reservation_to_slot(&slot, conflict) == ERR_INTERVAL_OVERLAP);
    free_reservation_node(conflict);

    /* Insert perfectly fitting interval in the gap: [11, 13) */
    ReservationNode* fit = create_reservation_node("TKT-GAP", "VEH-GAP", CAT_STANDARD, t_11, t_13);
    assert(add_reservation_to_slot(&slot, fit) == STATUS_OK);
    assert(slot.active_reservations_count == 4);

    /* Test node removal by ticket ID */
    ReservationNode* removed = remove_reservation_from_slot(&slot, "TKT-2");
    assert(removed != NULL);
    assert(strcmp(removed->ticket_id, "TKT-2") == 0);
    assert(slot.active_reservations_count == 3);
    free_reservation_node(removed);

    free_slot_reservations(&slot);
    assert(slot.reservation_head == NULL);
    assert(slot.active_reservations_count == 0);

    printf("  --> PASSED: Chronological list insertion, gap filling, and deletion succeeded.\n");
}

int main(void) {
    printf("====================================================\n");
    printf(" UNIT TEST SUITE: Interval Scheduling & Conflict Engine \n");
    printf("====================================================\n");

    test_half_open_interval_overlap();
    test_chronological_list_insertion();

    printf("\n>>> ALL INTERVAL ENGINE TESTS COMPLETED SUCCESSFULLY! <<<\n\n");
    return 0;
}
