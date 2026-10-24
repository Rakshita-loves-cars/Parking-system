#ifndef SLOT_H
#define SLOT_H

#include "common.h"

/* Reservation node representing a scheduled interval [start_time, end_time) */
typedef struct ReservationNode {
    char ticket_id[MAX_TICKET_LEN];
    char vehicle_reg[MAX_REG_LEN];
    VehicleCategory vehicle_cat;
    time_t start_time;
    time_t end_time;
    bool checked_in;
    time_t actual_checkin_time;
    time_t actual_checkout_time;
    double total_fee;
    struct ReservationNode* next;
} ReservationNode;

/* Parking Slot representation */
typedef struct {
    int slot_id;
    int floor;
    int bay_number;
    VehicleCategory slot_cat;
    double distance_from_entrance; /* Distance in meters */
    PhysicalStatus physical_status;
    char current_occupant_reg[MAX_REG_LEN];
    ReservationNode* reservation_head; /* Chronologically sorted linked list */
    int active_reservations_count;
} ParkingSlot;

/* Node allocation & deallocation */
ReservationNode* create_reservation_node(const char* ticket_id,
                                         const char* vehicle_reg,
                                         VehicleCategory cat,
                                         time_t start,
                                         time_t end);
void free_reservation_node(ReservationNode* node);

/* Interval math and slot validation */
bool check_interval_overlap(time_t s1, time_t e1, time_t s2, time_t e2);
bool is_slot_available_in_interval(const ParkingSlot* slot, time_t start, time_t end);
StatusCode add_reservation_to_slot(ParkingSlot* slot, ReservationNode* new_node);
ReservationNode* remove_reservation_from_slot(ParkingSlot* slot, const char* ticket_id);
ReservationNode* find_reservation_in_slot(const ParkingSlot* slot, const char* ticket_id);
ReservationNode* get_active_reservation_at_time(const ParkingSlot* slot, time_t query_time);
void free_slot_reservations(ParkingSlot* slot);

/* Metrics for advanced Best-Fit allocation */
double calculate_time_slack(const ParkingSlot* slot, time_t start, time_t end);
int calculate_dimension_waste(VehicleCategory vehicle_cat, VehicleCategory slot_cat);

#endif /* SLOT_H */
