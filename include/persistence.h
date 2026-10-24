#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "common.h"
#include "manager.h"

#define DEFAULT_DATA_FILE "data/parking_state.dat"
#define DEFAULT_CSV_EXPORT "data/reservations_export.csv"

/* File Header structure for binary integrity check */
#pragma pack(push, 1)
typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t total_slots;
  char facility_name[MAX_NAME_LEN];
  uint32_t total_reservations_served;
  double total_revenue_collected;
  uint32_t checksum;
} FileHeader;

typedef struct {
  int slot_id;
  int floor;
  int bay_number;
  int slot_cat;
  double distance;
  int physical_status;
  char current_occupant[MAX_REG_LEN];
  uint16_t reservation_count;
} StoredSlotRecord;

typedef struct {
  char ticket_id[MAX_TICKET_LEN];
  char vehicle_reg[MAX_REG_LEN];
  int vehicle_cat;
  int64_t start_time;
  int64_t end_time;
  int8_t checked_in;
  int64_t actual_checkin_time;
  int64_t actual_checkout_time;
  double total_fee;
} StoredReservationRecord;
#pragma pack(pop)

/* Persistence APIs */
StatusCode save_facility_state(const ParkingFacility *facility,
                               const char *filepath);
StatusCode load_facility_state(ParkingFacility *facility, const char *filepath);
StatusCode export_reservations_to_csv(const ParkingFacility *facility,
                                      const char *csv_filepath);

#endif /* PERSISTENCE_H */
