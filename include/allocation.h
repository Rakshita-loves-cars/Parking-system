#ifndef ALLOCATION_H
#define ALLOCATION_H

#include "common.h"
#include "slot.h"

/* Allocation Request Data */
typedef struct {
    char vehicle_reg[MAX_REG_LEN];
    VehicleCategory vehicle_cat;
    time_t start_time;
    time_t end_time;
    AllocationStrategy strategy;
} AllocationRequest;

/* Allocation Outcome Data */
typedef struct {
    StatusCode status;
    int allocated_slot_id;
    double slot_distance;
    double allocation_score;
    char ticket_id[MAX_TICKET_LEN];
    char failure_reason[64];
} AllocationResult;

/* Core Algorithm Functions */
int allocate_greedy_distance(const ParkingSlot* slots, 
                             int num_slots, 
                             VehicleCategory cat, 
                             time_t start, 
                             time_t end);

int allocate_best_fit(const ParkingSlot* slots, 
                      int num_slots, 
                      VehicleCategory cat, 
                      time_t start, 
                      time_t end);

AllocationResult process_allocation_request(ParkingSlot* slots,
                                            int num_slots,
                                            const AllocationRequest* req);

#endif /* ALLOCATION_H */
