#ifndef MANAGER_H
#define MANAGER_H

#include "common.h"
#include "slot.h"
#include "allocation.h"

#define DEFAULT_FLOORS           2
#define DEFAULT_SLOTS_PER_FLOOR  12
#define DEFAULT_FACILITY_NAME    "SmartPark Hub"

/* Primary facility container */
typedef struct {
    ParkingSlot slots[MAX_SLOTS];
    int total_slots;
    char facility_name[MAX_NAME_LEN];
    AllocationStrategy default_strategy;
    int total_reservations_served;
    double total_revenue_collected;
} ParkingFacility;

/* Initialization and Teardown */
void init_parking_facility(ParkingFacility* facility, 
                           const char* name, 
                           int num_floors, 
                           int slots_per_floor);

void free_parking_facility(ParkingFacility* facility);

ReservationNode* find_reservation_by_ticket_or_reg(const ParkingFacility* facility, 
                                                   const char* identifier, 
                                                   int* slot_idx_out);

/* Core Operational Workflows */
AllocationResult reserve_parking_slot(ParkingFacility* facility, const AllocationRequest* req);
StatusCode cancel_reservation(ParkingFacility* facility, const char* ticket_id);
StatusCode check_in_vehicle(ParkingFacility* facility, const char* ticket_id, time_t checkin_time);
StatusCode check_out_vehicle(ParkingFacility* facility, 
                             const char* ticket_id, 
                             time_t checkout_time, 
                             double* fee_out);

/* Tariff & Billing */
double calculate_parking_charges(VehicleCategory cat, 
                                 time_t reserved_start, 
                                 time_t reserved_end, 
                                 time_t actual_checkin, 
                                 time_t actual_checkout);

/* Reporting and Views */
void display_lot_map(const ParkingFacility* facility, time_t view_time);
void display_slot_timeline(const ParkingFacility* facility, int slot_id);
void display_ticket_receipt(const ParkingFacility* facility, const char* ticket_id);
void display_facility_statistics(const ParkingFacility* facility);

/* Utility */
void generate_unique_ticket_id(char* buffer, size_t buflen, int slot_id);

#endif /* MANAGER_H */
