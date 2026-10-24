#include "allocation.h"
#include <float.h>

/*
 * Strategy 1: Greedy Nearest-Slot (First-Fit by Distance)
 * Selects the compatible, available slot with the minimum walking distance from the entrance.
 * Time Complexity: O(M * k), where M = total slots, k = reservations per slot.
 */
int allocate_greedy_distance(const ParkingSlot* slots, 
                             int num_slots, 
                             VehicleCategory cat, 
                             time_t start, 
                             time_t end) {
    if (!slots || num_slots <= 0) return -1;

    int best_slot_idx = -1;
    double min_distance = DBL_MAX;

    for (int i = 0; i < num_slots; ++i) {
        if (!is_category_compatible(cat, slots[i].slot_cat)) {
            continue;
        }

        if (is_slot_available_in_interval(&slots[i], start, end)) {
            if (slots[i].distance_from_entrance < min_distance) {
                min_distance = slots[i].distance_from_entrance;
                best_slot_idx = i;
            }
        }
    }

    return best_slot_idx;
}

/*
 * Strategy 2: Best-Fit (Multi-Objective Optimization)
 * Balances three factors:
 *   1. Minimizing dimension waste (e.g. avoid placing Compact cars in SUV bays).
 *   2. Minimizing residual idle time slack (packing reservations tightly).
 *   3. Secondary tie-breaking by walking distance.
 *
 * Cost Formula:
 *   Cost = (W_DIM * dim_waste) + (W_SLACK * time_slack_hours) + (W_DIST * normalized_distance)
 */
int allocate_best_fit(const ParkingSlot* slots, 
                      int num_slots, 
                      VehicleCategory cat, 
                      time_t start, 
                      time_t end) {
    if (!slots || num_slots <= 0) return -1;

    const double W_DIM   = 100.0; /* Heavy penalty for wasting large slot capacity */
    const double W_SLACK = 15.0;  /* Prefers tight time interval packing */
    const double W_DIST  = 1.0;   /* Secondary distance tie-breaker */

    int best_slot_idx = -1;
    double min_cost = DBL_MAX;

    for (int i = 0; i < num_slots; ++i) {
        if (!is_category_compatible(cat, slots[i].slot_cat)) {
            continue;
        }

        if (is_slot_available_in_interval(&slots[i], start, end)) {
            int dim_waste = calculate_dimension_waste(cat, slots[i].slot_cat);
            double slack = calculate_time_slack(&slots[i], start, end);
            double norm_dist = slots[i].distance_from_entrance / 100.0;

            double cost = (W_DIM * dim_waste) + (W_SLACK * slack) + (W_DIST * norm_dist);

            if (cost < min_cost) {
                min_cost = cost;
                best_slot_idx = i;
            }
        }
    }

    return best_slot_idx;
}

/*
 * Evaluates the request and executes allocation using the chosen strategy
 */
AllocationResult process_allocation_request(ParkingSlot* slots,
                                            int num_slots,
                                            const AllocationRequest* req) {
    AllocationResult res;
    memset(&res, 0, sizeof(AllocationResult));
    res.allocated_slot_id = -1;

    if (!slots || !req || num_slots <= 0) {
        res.status = ERR_INVALID_PARAM;
        snprintf(res.failure_reason, sizeof(res.failure_reason), "Invalid system parameters");
        return res;
    }

    if (req->start_time >= req->end_time) {
        res.status = ERR_INVALID_TIME_RANGE;
        snprintf(res.failure_reason, sizeof(res.failure_reason), "Start time must be before end time");
        return res;
    }

    int selected_idx = -1;
    if (req->strategy == ALLOC_BEST_FIT) {
        selected_idx = allocate_best_fit(slots, num_slots, req->vehicle_cat, req->start_time, req->end_time);
    } else {
        selected_idx = allocate_greedy_distance(slots, num_slots, req->vehicle_cat, req->start_time, req->end_time);
    }

    if (selected_idx < 0) {
        res.status = ERR_NO_COMPATIBLE_SLOT;
        snprintf(res.failure_reason, sizeof(res.failure_reason), 
                 "No compatible slot free in interval [%ld, %ld)", 
                 (long)req->start_time, (long)req->end_time);
        return res;
    }

    ParkingSlot* target_slot = &slots[selected_idx];

    /* Generate a unique ticket ID: TKT-<slot_id>-<timestamp_tail> */
    snprintf(res.ticket_id, sizeof(res.ticket_id), "TKT-S%02d-%04X", 
             target_slot->slot_id, (unsigned int)(rand() & 0xFFFF));

    ReservationNode* node = create_reservation_node(res.ticket_id,
                                                   req->vehicle_reg,
                                                   req->vehicle_cat,
                                                   req->start_time,
                                                   req->end_time);
    if (!node) {
        res.status = ERR_FILE_IO;
        snprintf(res.failure_reason, sizeof(res.failure_reason), "Memory allocation failure");
        return res;
    }

    StatusCode add_status = add_reservation_to_slot(target_slot, node);
    if (add_status != STATUS_OK) {
        free_reservation_node(node);
        res.status = add_status;
        snprintf(res.failure_reason, sizeof(res.failure_reason), "%s", status_to_string(add_status));
        return res;
    }

    res.status = STATUS_OK;
    res.allocated_slot_id = target_slot->slot_id;
    res.slot_distance = target_slot->distance_from_entrance;
    res.allocation_score = 100.0;

    return res;
}
