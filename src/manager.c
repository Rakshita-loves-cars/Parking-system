#include "manager.h"
#include <math.h>

void init_parking_facility(ParkingFacility* facility, 
                           const char* name, 
                           int num_floors, 
                           int slots_per_floor) {
    if (!facility) return;
    memset(facility, 0, sizeof(ParkingFacility));

    if (name && strlen(name) > 0) {
        strncpy(facility->facility_name, name, MAX_NAME_LEN - 1);
    } else {
        strncpy(facility->facility_name, DEFAULT_FACILITY_NAME, MAX_NAME_LEN - 1);
    }

    facility->default_strategy = ALLOC_GREEDY_DISTANCE;
    facility->total_reservations_served = 0;
    facility->total_revenue_collected = 0.0;

    int total = num_floors * slots_per_floor;
    if (total > MAX_SLOTS) total = MAX_SLOTS;
    facility->total_slots = total;

    int slot_id_counter = 1;
    for (int f = 1; f <= num_floors && slot_id_counter <= total; ++f) {
        for (int b = 1; b <= slots_per_floor && slot_id_counter <= total; ++b) {
            int idx = slot_id_counter - 1;
            ParkingSlot* s = &facility->slots[idx];
            s->slot_id = slot_id_counter;
            s->floor = f;
            s->bay_number = b;
            s->physical_status = PHYS_VACANT;
            s->current_occupant_reg[0] = '\0';
            s->reservation_head = NULL;
            s->active_reservations_count = 0;

            /* Realistic Category Assignment by Floor and Bay */
            if (f == 1) {
                if (b == 1 || b == 2) {
                    s->slot_cat = CAT_ACCESSIBLE; /* Prime ground floor near elevators */
                    s->distance_from_entrance = 10.0 + (b * 5.0);
                } else if (b == 3 || b == 4) {
                    s->slot_cat = CAT_EV; /* Equipped with high-power EV charging */
                    s->distance_from_entrance = 20.0 + (b * 5.0);
                } else if (b <= 8) {
                    s->slot_cat = CAT_COMPACT;
                    s->distance_from_entrance = 30.0 + (b * 4.0);
                } else {
                    s->slot_cat = CAT_STANDARD;
                    s->distance_from_entrance = 50.0 + (b * 3.0);
                }
            } else {
                /* Upper Floors */
                if (b <= 3) {
                    s->slot_cat = CAT_STANDARD;
                    s->distance_from_entrance = 70.0 + (b * 5.0);
                } else if (b <= 7) {
                    s->slot_cat = CAT_SUV; /* Larger bays on upper deck */
                    s->distance_from_entrance = 85.0 + (b * 4.0);
                } else if (b == 8 || b == 9) {
                    s->slot_cat = CAT_EV;
                    s->distance_from_entrance = 105.0 + (b * 3.0);
                } else {
                    s->slot_cat = CAT_COMPACT;
                    s->distance_from_entrance = 115.0 + (b * 3.0);
                }
            }
            slot_id_counter++;
        }
    }
}

void free_parking_facility(ParkingFacility* facility) {
    if (!facility) return;
    for (int i = 0; i < facility->total_slots; ++i) {
        free_slot_reservations(&facility->slots[i]);
    }
}

AllocationResult reserve_parking_slot(ParkingFacility* facility, const AllocationRequest* req) {
    if (!facility || !req) {
        AllocationResult res;
        memset(&res, 0, sizeof(AllocationResult));
        res.status = ERR_INVALID_PARAM;
        return res;
    }

    AllocationResult res = process_allocation_request(facility->slots, facility->total_slots, req);
    if (res.status == STATUS_OK) {
        facility->total_reservations_served++;
    }
    return res;
}

StatusCode cancel_reservation(ParkingFacility* facility, const char* ticket_id) {
    if (!facility || !ticket_id) return ERR_INVALID_PARAM;

    for (int i = 0; i < facility->total_slots; ++i) {
        ReservationNode* node = remove_reservation_from_slot(&facility->slots[i], ticket_id);
        if (node) {
            /* If vehicle was checked in, revert physical slot status */
            if (node->checked_in) {
                facility->slots[i].physical_status = PHYS_VACANT;
                facility->slots[i].current_occupant_reg[0] = '\0';
            }
            free_reservation_node(node);
            return STATUS_OK;
        }
    }

    return ERR_TICKET_NOT_FOUND;
}

ReservationNode* find_reservation_by_ticket_or_reg(const ParkingFacility* facility, 
                                                   const char* identifier, 
                                                   int* slot_idx_out) {
    if (!facility || !identifier) return NULL;

    for (int i = 0; i < facility->total_slots; ++i) {
        ReservationNode* cur = facility->slots[i].reservation_head;
        while (cur != NULL) {
            if (strncmp(cur->ticket_id, identifier, MAX_TICKET_LEN) == 0 ||
                strncmp(cur->vehicle_reg, identifier, MAX_REG_LEN) == 0) {
                if (slot_idx_out) *slot_idx_out = i;
                return cur;
            }
            cur = cur->next;
        }
    }
    return NULL;
}

StatusCode check_in_vehicle(ParkingFacility* facility, const char* ticket_id, time_t checkin_time) {
    if (!facility || !ticket_id) return ERR_INVALID_PARAM;

    int slot_idx = -1;
    ReservationNode* node = find_reservation_by_ticket_or_reg(facility, ticket_id, &slot_idx);
    if (!node || slot_idx < 0) {
        return ERR_TICKET_NOT_FOUND;
    }

    if (node->checked_in) {
        return ERR_ALREADY_CHECKED_IN;
    }
    if (facility->slots[slot_idx].physical_status == PHYS_OCCUPIED) {
        return ERR_ALREADY_OCCUPIED;
    }

    node->checked_in = true;
    node->actual_checkin_time = checkin_time;
    facility->slots[slot_idx].physical_status = PHYS_OCCUPIED;
    strncpy(facility->slots[slot_idx].current_occupant_reg, node->vehicle_reg, MAX_REG_LEN - 1);
    return STATUS_OK;
}

double calculate_parking_charges(VehicleCategory cat, 
                                 time_t reserved_start, 
                                 time_t reserved_end, 
                                 time_t actual_checkin, 
                                 time_t actual_checkout) {
    double base_rate = get_category_rate(cat);
    double fixed_entry_fee = 2.00; /* $2 entry fee */

    /* Billing uses the greater of reserved duration or actual duration */
    time_t bill_start = (actual_checkin > 0 && actual_checkin < reserved_start) ? actual_checkin : reserved_start;
    time_t bill_end   = (actual_checkout > 0) ? actual_checkout : reserved_end;

    double billed_seconds = (bill_end > bill_start) ? (double)(bill_end - bill_start) : 0.0;
    double billed_hours = ceil(billed_seconds / 3600.0);
    if (billed_hours < 1.0) billed_hours = 1.0;

    double standard_fee = fixed_entry_fee + (billed_hours * base_rate);

    /* Check for overstay violation */
    double overstay_penalty = 0.0;
    if (actual_checkout > reserved_end) {
        double overstay_seconds = (double)(actual_checkout - reserved_end);
        double overstay_hours = ceil(overstay_seconds / 3600.0);
        /* 2x penalty rate for unauthorized overstay */
        overstay_penalty = overstay_hours * (base_rate * 2.0);
    }

    return (standard_fee + overstay_penalty);
}

StatusCode check_out_vehicle(ParkingFacility* facility, 
                             const char* ticket_id, 
                             time_t checkout_time, 
                             double* fee_out) {
    if (!facility || !ticket_id || !fee_out) return ERR_INVALID_PARAM;

    int slot_idx = -1;
    ReservationNode* node = find_reservation_by_ticket_or_reg(facility, ticket_id, &slot_idx);
    if (!node || slot_idx < 0) {
        return ERR_TICKET_NOT_FOUND;
    }

    node->actual_checkout_time = checkout_time;
    double fee = calculate_parking_charges(node->vehicle_cat,
                                           node->start_time,
                                           node->end_time,
                                           node->actual_checkin_time,
                                           checkout_time);
    node->total_fee = fee;
    *fee_out = fee;
    facility->total_revenue_collected += fee;

    /* Release physical slot */
    facility->slots[slot_idx].physical_status = PHYS_VACANT;
    facility->slots[slot_idx].current_occupant_reg[0] = '\0';
    node->checked_in = false;

    return STATUS_OK;
}

void display_lot_map(const ParkingFacility* facility, time_t view_time) {
    if (!facility) return;

    char time_buf[TIME_STR_BUFFER];
    format_datetime_str(view_time, time_buf, sizeof(time_buf));

    printf("\n========================================================================================\n");
    printf("   %s%s%s - FACILITY LAYOUT MAP (Query Time: %s)\n", 
           ANSI_COLOR_BOLD, facility->facility_name, ANSI_COLOR_RESET, time_buf);
    printf("========================================================================================\n");
    printf(" Legend: %s[VACANT]%s  %s[OCCUPIED]%s  %s[RESERVED-ACTIVE]%s  %s(EV Charging)%s  %s(Accessible)%s\n",
           ANSI_COLOR_GREEN, ANSI_COLOR_RESET,
           ANSI_COLOR_RED, ANSI_COLOR_RESET,
           ANSI_COLOR_YELLOW, ANSI_COLOR_RESET,
           ANSI_COLOR_CYAN, ANSI_COLOR_RESET,
           ANSI_COLOR_BLUE, ANSI_COLOR_RESET);
    printf("----------------------------------------------------------------------------------------\n");

    int current_floor = -1;
    for (int i = 0; i < facility->total_slots; ++i) {
        const ParkingSlot* s = &facility->slots[i];
        if (s->floor != current_floor) {
            current_floor = s->floor;
            printf("\n%s[ FLOOR %d ]%s\n", ANSI_COLOR_BOLD, current_floor, ANSI_COLOR_RESET);
        }

        ReservationNode* active_res = get_active_reservation_at_time(s, view_time);
        bool is_physically_busy = (s->physical_status == PHYS_OCCUPIED);
        bool is_reserved_now   = (active_res != NULL);

        const char* color_code = ANSI_COLOR_GREEN;
        const char* status_tag = "VAC";

        if (is_physically_busy) {
            color_code = ANSI_COLOR_RED;
            status_tag = "OCC";
        } else if (is_reserved_now) {
            color_code = ANSI_COLOR_YELLOW;
            status_tag = "RES";
        }

        char cat_code = 'S';
        if (s->slot_cat == CAT_COMPACT)    cat_code = 'C';
        else if (s->slot_cat == CAT_SUV)   cat_code = 'U';
        else if (s->slot_cat == CAT_EV)    cat_code = 'E';
        else if (s->slot_cat == CAT_ACCESSIBLE) cat_code = 'A';

        printf("%s[S%02d:%c-%s]%s ", color_code, s->slot_id, cat_code, status_tag, ANSI_COLOR_RESET);

        if ((i + 1) % 6 == 0) {
            printf("\n");
        }
    }
    printf("\n========================================================================================\n\n");
}

void display_slot_timeline(const ParkingFacility* facility, int slot_id) {
    if (!facility || slot_id < 1 || slot_id > facility->total_slots) {
        printf("Invalid slot ID %d.\n", slot_id);
        return;
    }

    const ParkingSlot* s = &facility->slots[slot_id - 1];
    printf("\n------------------------------------------------------------\n");
    printf(" Timeline Schedule for Slot #%02d [Floor %d, Bay %d]\n", s->slot_id, s->floor, s->bay_number);
    printf(" Category: %s | Distance from Entrance: %.1f meters\n", 
           category_to_string(s->slot_cat), s->distance_from_entrance);
    printf(" Physical Status: %s %s\n", 
           s->physical_status == PHYS_OCCUPIED ? ANSI_COLOR_RED "OCCUPIED" : ANSI_COLOR_GREEN "VACANT",
           ANSI_COLOR_RESET);
    if (s->physical_status == PHYS_OCCUPIED) {
        printf(" Current Vehicle: %s\n", s->current_occupant_reg);
    }
    printf(" Active Bookings Count: %d\n", s->active_reservations_count);
    printf("------------------------------------------------------------\n");

    const ReservationNode* cur = s->reservation_head;
    if (!cur) {
        printf(" No future or active reservations scheduled for this slot.\n");
    } else {
        int idx = 1;
        while (cur != NULL) {
            char s_buf[TIME_STR_BUFFER], e_buf[TIME_STR_BUFFER];
            format_datetime_str(cur->start_time, s_buf, sizeof(s_buf));
            format_datetime_str(cur->end_time, e_buf, sizeof(e_buf));

            printf(" %d. Ticket: %s | Vehicle: %-10s (%s)\n", 
                   idx++, cur->ticket_id, cur->vehicle_reg, category_to_string(cur->vehicle_cat));
            printf("    Window: [%s  to  %s) | Checked-in: %s\n", 
                   s_buf, e_buf, cur->checked_in ? "YES" : "NO");
            cur = cur->next;
        }
    }
    printf("------------------------------------------------------------\n\n");
}

void display_ticket_receipt(const ParkingFacility* facility, const char* ticket_id) {
    if (!facility || !ticket_id) return;

    int slot_idx = -1;
    ReservationNode* node = find_reservation_by_ticket_or_reg(facility, ticket_id, &slot_idx);
    if (node && slot_idx >= 0) {
        char s_buf[TIME_STR_BUFFER], e_buf[TIME_STR_BUFFER];
        char in_buf[TIME_STR_BUFFER], out_buf[TIME_STR_BUFFER];

        format_datetime_str(node->start_time, s_buf, sizeof(s_buf));
        format_datetime_str(node->end_time, e_buf, sizeof(e_buf));
        format_datetime_str(node->actual_checkin_time, in_buf, sizeof(in_buf));
        format_datetime_str(node->actual_checkout_time, out_buf, sizeof(out_buf));

        printf("\n+==========================================================+\n");
        printf("|                 SMART PARKING TICKET RECEIPT             |\n");
        printf("+==========================================================+\n");
        printf("  Ticket ID        : %s\n", node->ticket_id);
        printf("  Allocated Slot   : Slot #%02d (Floor %d, Bay %d)\n", 
               facility->slots[slot_idx].slot_id, facility->slots[slot_idx].floor, facility->slots[slot_idx].bay_number);
        printf("  Slot Category    : %s\n", category_to_string(facility->slots[slot_idx].slot_cat));
        printf("  Walking Distance : %.1f meters to entrance\n", facility->slots[slot_idx].distance_from_entrance);
        printf("  Vehicle Reg      : %s [%s]\n", node->vehicle_reg, category_to_string(node->vehicle_cat));
        printf("  Reserved Window  : %s -> %s\n", s_buf, e_buf);
        printf("  Actual Check-in  : %s\n", in_buf);
        printf("  Actual Check-out : %s\n", out_buf);
        printf("  Status           : %s\n", node->checked_in ? "ACTIVE (ON-SITE)" : "COMPLETED / PENDING");
        if (node->total_fee > 0.0) {
            printf("  Total Billed Fee : $%.2f\n", node->total_fee);
        }
        printf("+==========================================================+\n\n");
        return;
    }

    printf("Ticket / Vehicle '%s' not found in facility records.\n", ticket_id);
}

void display_facility_statistics(const ParkingFacility* facility) {
    if (!facility) return;

    int occupied_count = 0;
    int total_scheduled_bookings = 0;

    for (int i = 0; i < facility->total_slots; ++i) {
        if (facility->slots[i].physical_status == PHYS_OCCUPIED) {
            occupied_count++;
        }
        total_scheduled_bookings += facility->slots[i].active_reservations_count;
    }

    double physical_occupancy_pct = ((double)occupied_count / (double)facility->total_slots) * 100.0;

    printf("\n============================================================\n");
    printf("   %s%s - SYSTEM METRICS & AUDIT%s\n", ANSI_COLOR_BOLD, facility->facility_name, ANSI_COLOR_RESET);
    printf("============================================================\n");
    printf("  Total Parking Slots Available : %d\n", facility->total_slots);
    printf("  Current Physical Occupancy    : %d / %d (%.1f%%)\n", 
           occupied_count, facility->total_slots, physical_occupancy_pct);
    printf("  Active Forward Reservations   : %d\n", total_scheduled_bookings);
    printf("  Cumulative Bookings Served    : %d\n", facility->total_reservations_served);
    printf("  Total Facility Revenue        : $%.2f\n", facility->total_revenue_collected);
    printf("  Default Allocation Strategy   : %s\n", strategy_to_string(facility->default_strategy));
    printf("============================================================\n\n");
}
