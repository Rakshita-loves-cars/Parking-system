#include "slot.h"

ReservationNode* create_reservation_node(const char* ticket_id,
                                         const char* vehicle_reg,
                                         VehicleCategory cat,
                                         time_t start,
                                         time_t end) {
    if (!ticket_id || !vehicle_reg || start >= end) {
        return NULL;
    }

    ReservationNode* node = (ReservationNode*)malloc(sizeof(ReservationNode));
    if (!node) return NULL;

    memset(node, 0, sizeof(ReservationNode));
    strncpy(node->ticket_id, ticket_id, MAX_TICKET_LEN - 1);
    strncpy(node->vehicle_reg, vehicle_reg, MAX_REG_LEN - 1);
    node->vehicle_cat = cat;
    node->start_time = start;
    node->end_time = end;
    node->checked_in = false;
    node->actual_checkin_time = 0;
    node->actual_checkout_time = 0;
    node->total_fee = 0.0;
    node->next = NULL;

    return node;
}

void free_reservation_node(ReservationNode* node) {
    if (node) {
        free(node);
    }
}

/*
 * Half-open interval overlap detection:
 * Intervals [s1, e1) and [s2, e2) overlap if and only if:
 *   max(s1, s2) < min(e1, e2)
 *
 * Notice that if e1 == s2, max(s1, s2) = s2, min(e1, e2) = e1 = s2.
 * Since s2 < s2 is FALSE, back-to-back reservations DO NOT overlap.
 */
bool check_interval_overlap(time_t s1, time_t e1, time_t s2, time_t e2) {
    time_t start_max = (s1 > s2) ? s1 : s2;
    time_t end_min   = (e1 < e2) ? e1 : e2;
    return (start_max < end_min);
}

/*
 * Checks if the slot is free during [start, end)
 * Leverages the fact that the reservation linked list is sorted by start_time.
 */
bool is_slot_available_in_interval(const ParkingSlot* slot, time_t start, time_t end) {
    if (!slot || start >= end) return false;

    const ReservationNode* cur = slot->reservation_head;
    while (cur != NULL) {
        if (check_interval_overlap(cur->start_time, cur->end_time, start, end)) {
            return false; /* Direct conflict detected */
        }
        /* Early exit optimization: Since reservations are chronologically ordered,
           if cur->start_time >= end, no subsequent reservation can overlap. */
        if (cur->start_time >= end) {
            break;
        }
        cur = cur->next;
    }
    return true;
}

/*
 * Inserts reservation into the slot's sorted linked list.
 */
StatusCode add_reservation_to_slot(ParkingSlot* slot, ReservationNode* new_node) {
    if (!slot || !new_node) return ERR_INVALID_PARAM;
    if (new_node->start_time >= new_node->end_time) return ERR_INVALID_TIME_RANGE;

    if (!is_slot_available_in_interval(slot, new_node->start_time, new_node->end_time)) {
        return ERR_INTERVAL_OVERLAP;
    }

    /* Insert into sorted list */
    if (slot->reservation_head == NULL || new_node->start_time < slot->reservation_head->start_time) {
        new_node->next = slot->reservation_head;
        slot->reservation_head = new_node;
    } else {
        ReservationNode* cur = slot->reservation_head;
        while (cur->next != NULL && cur->next->start_time <= new_node->start_time) {
            cur = cur->next;
        }
        new_node->next = cur->next;
        cur->next = new_node;
    }

    slot->active_reservations_count++;
    return STATUS_OK;
}

ReservationNode* remove_reservation_from_slot(ParkingSlot* slot, const char* ticket_id) {
    if (!slot || !ticket_id || !slot->reservation_head) return NULL;

    ReservationNode* cur = slot->reservation_head;
    ReservationNode* prev = NULL;

    while (cur != NULL) {
        if (strncmp(cur->ticket_id, ticket_id, MAX_TICKET_LEN) == 0) {
            if (prev == NULL) {
                slot->reservation_head = cur->next;
            } else {
                prev->next = cur->next;
            }
            cur->next = NULL;
            slot->active_reservations_count--;
            return cur;
        }
        prev = cur;
        cur = cur->next;
    }

    return NULL;
}

ReservationNode* find_reservation_in_slot(const ParkingSlot* slot, const char* ticket_id) {
    if (!slot || !ticket_id) return NULL;

    ReservationNode* cur = slot->reservation_head;
    while (cur != NULL) {
        if (strncmp(cur->ticket_id, ticket_id, MAX_TICKET_LEN) == 0) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

ReservationNode* get_active_reservation_at_time(const ParkingSlot* slot, time_t query_time) {
    if (!slot) return NULL;

    ReservationNode* cur = slot->reservation_head;
    while (cur != NULL) {
        if (query_time >= cur->start_time && query_time < cur->end_time) {
            return cur;
        }
        if (cur->start_time > query_time) {
            break;
        }
        cur = cur->next;
    }
    return NULL;
}

void free_slot_reservations(ParkingSlot* slot) {
    if (!slot) return;
    ReservationNode* cur = slot->reservation_head;
    while (cur != NULL) {
        ReservationNode* next = cur->next;
        free(cur);
        cur = next;
    }
    slot->reservation_head = NULL;
    slot->active_reservations_count = 0;
}

/*
 * Calculates the residual idle gap (slack) around the requested interval.
 * Returns slack time in hours.
 */
double calculate_time_slack(const ParkingSlot* slot, time_t start, time_t end) {
    if (!slot) return 9999.0;

    const ReservationNode* cur = slot->reservation_head;
    const ReservationNode* prev = NULL;

    while (cur != NULL && cur->start_time < start) {
        prev = cur;
        cur = cur->next;
    }

    /* If cur points to the exact reservation itself (e.g. called after insertion), advance to next */
    if (cur != NULL && cur->start_time == start && cur->end_time == end) {
        cur = cur->next;
    }

    double left_slack = 0.0;
    double right_slack = 0.0;

    if (prev != NULL) {
        left_slack = (double)(start - prev->end_time) / 3600.0;
        if (left_slack < 0.0) left_slack = 0.0;
    } else {
        left_slack = 2.0; /* Baseline when slot is free to the left */
    }

    if (cur != NULL) {
        right_slack = (double)(cur->start_time - end) / 3600.0;
        if (right_slack < 0.0) right_slack = 0.0;
    } else {
        right_slack = 2.0; /* Baseline when slot is free to the right */
    }

    return (left_slack + right_slack);
}

int calculate_dimension_waste(VehicleCategory vehicle_cat, VehicleCategory slot_cat) {
    if (vehicle_cat == CAT_COMPACT) {
        if (slot_cat == CAT_COMPACT) return 0;
        if (slot_cat == CAT_STANDARD) return 1;
        if (slot_cat == CAT_SUV) return 2;
    } else if (vehicle_cat == CAT_STANDARD) {
        if (slot_cat == CAT_STANDARD) return 0;
        if (slot_cat == CAT_SUV) return 1;
    } else if (vehicle_cat == CAT_SUV) {
        if (slot_cat == CAT_SUV) return 0;
    } else if (vehicle_cat == CAT_EV && slot_cat == CAT_EV) {
        return 0;
    } else if (vehicle_cat == CAT_ACCESSIBLE && slot_cat == CAT_ACCESSIBLE) {
        return 0;
    }
    return 3; /* Suboptimal or fallback */
}
