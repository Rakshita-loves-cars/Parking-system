#include "common.h"

const char *category_to_string(VehicleCategory cat) {
  switch (cat) {
  case CAT_COMPACT:
    return "Compact Car";
  case CAT_STANDARD:
    return "Standard Sedan";
  case CAT_SUV:
    return "Large SUV / Van";
  case CAT_EV:
    return "Electric Vehicle (EV)";
  case CAT_ACCESSIBLE:
    return "Accessible (PRM)";
  default:
    return "Unknown";
  }
}

const char *strategy_to_string(AllocationStrategy strat) {
  switch (strat) {
  case ALLOC_GREEDY_DISTANCE:
    return "Greedy Nearest (First-Fit by Distance)";
  case ALLOC_BEST_FIT:
    return "Best-Fit (Slack & Compatibility)";
  default:
    return "Unknown Strategy";
  }
}

const char *status_to_string(StatusCode code) {
  switch (code) {
  case STATUS_OK:
    return "Operation Successful";
  case ERR_INVALID_PARAM:
    return "Invalid Input Parameter";
  case ERR_INVALID_TIME_RANGE:
    return "Invalid Time Window (End <= Start)";
  case ERR_INTERVAL_OVERLAP:
    return "Conflicting Reservation Exists";
  case ERR_NO_COMPATIBLE_SLOT:
    return "No Compatible/Available Slot Found";
  case ERR_TICKET_NOT_FOUND:
    return "Ticket ID Not Found";
  case ERR_ALREADY_OCCUPIED:
    return "Slot is Already Physically Occupied";
  case ERR_ALREADY_CHECKED_IN:
    return "Vehicle Already Checked In";
  case ERR_FILE_IO:
    return "File Read/Write Error";
  case ERR_CORRUPT_DATA:
    return "Corrupted or Invalid File Format";
  case ERR_CAPACITY_FULL:
    return "Parking Facility is Completely Full";
  default:
    return "Unknown System Error";
  }
}

/*
 * Compatibility Rule Lattice:
 * 1. Accessible (PRM) vehicle MUST only occupy CAT_ACCESSIBLE slot.
 *    Non-accessible vehicles CANNOT occupy CAT_ACCESSIBLE slot (accessibility
 * guarantee).
 * 2. EV requires CAT_EV slot (charging port). Non-EV CANNOT occupy CAT_EV.
 * 3. Compact vehicle can fit in COMPACT, STANDARD, or SUV.
 * 4. Standard vehicle can fit in STANDARD or SUV.
 * 5. SUV can only fit in SUV slot.
 */
bool is_category_compatible(VehicleCategory vehicle_cat,
                            VehicleCategory slot_cat) {
  if (vehicle_cat == CAT_ACCESSIBLE) {
    return (slot_cat == CAT_ACCESSIBLE);
  }
  if (slot_cat == CAT_ACCESSIBLE) {
    /* Preserved strictly for accessible vehicles */
    return false;
  }
  if (vehicle_cat == CAT_EV) {
    return (slot_cat == CAT_EV);
  }
  if (slot_cat == CAT_EV) {
    /* Preserved for EV charging */
    return false;
  }

  if (vehicle_cat == CAT_SUV) {
    return (slot_cat == CAT_SUV);
  }
  if (vehicle_cat == CAT_STANDARD) {
    return (slot_cat == CAT_STANDARD || slot_cat == CAT_SUV);
  }
  if (vehicle_cat == CAT_COMPACT) {
    return (slot_cat == CAT_COMPACT || slot_cat == CAT_STANDARD ||
            slot_cat == CAT_SUV);
  }

  return false;
}

double get_category_rate(VehicleCategory cat) {
  switch (cat) {
  case CAT_COMPACT:
    return 2.50; /* $2.50 / hour */
  case CAT_STANDARD:
    return 3.50; /* $3.50 / hour */
  case CAT_SUV:
    return 5.00; /* $5.00 / hour */
  case CAT_EV:
    return 4.50; /* $4.50 / hour (includes charging access) */
  case CAT_ACCESSIBLE:
    return 2.00; /* $2.00 / hour (subsidized accessibility) */
  default:
    return 3.00;
  }
}

/* Parse "YYYY-MM-DD HH:MM" into time_t using standard sscanf for max
 * portability */
time_t parse_datetime_str(const char *dt_str) {
  if (!dt_str)
    return (time_t)-1;
  int year = 0, month = 0, day = 0, hour = 0, min = 0;
  if (sscanf(dt_str, "%d-%d-%d %d:%d", &year, &month, &day, &hour, &min) != 5) {
    return (time_t)-1;
  }
  struct tm tm_val;
  memset(&tm_val, 0, sizeof(struct tm));
  tm_val.tm_year = year - 1900;
  tm_val.tm_mon = month - 1;
  tm_val.tm_mday = day;
  tm_val.tm_hour = hour;
  tm_val.tm_min = min;
  tm_val.tm_sec = 0;
  tm_val.tm_isdst = -1;

  return mktime(&tm_val);
}

void format_datetime_str(time_t t, char *buffer, size_t buflen) {
  if (!buffer || buflen == 0)
    return;
  if (t <= 0) {
    snprintf(buffer, buflen, "N/A");
    return;
  }
  struct tm *tm_info = localtime(&t);
  if (!tm_info) {
    snprintf(buffer, buflen, "Invalid Time");
    return;
  }
  strftime(buffer, buflen, "%Y-%m-%d %H:%M", tm_info);
}

time_t add_minutes(time_t base, int minutes) {
  return base + ((time_t)minutes * 60);
}

time_t add_hours(time_t base, int hours) {
  return base + ((time_t)hours * 3600);
}
