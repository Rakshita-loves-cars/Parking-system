#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define MAX_REG_LEN         16
#define MAX_TICKET_LEN      32
#define MAX_SLOTS           256
#define MAX_NAME_LEN        32
#define TIME_STR_BUFFER     32

#define MAGIC_HEADER        0x5041524BUL /* "PARK" in hex */
#define FILE_FORMAT_VERSION 1

/* Color codes for ANSI terminal display */
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_BOLD    "\x1b[1m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/* Vehicle and Slot Categories */
typedef enum {
    CAT_COMPACT = 0,
    CAT_STANDARD,
    CAT_SUV,
    CAT_EV,
    CAT_ACCESSIBLE,
    CAT_COUNT
} VehicleCategory;

/* Physical Slot Status */
typedef enum {
    PHYS_VACANT = 0,
    PHYS_OCCUPIED
} PhysicalStatus;

/* Allocation Algorithm Strategy */
typedef enum {
    ALLOC_GREEDY_DISTANCE = 0,
    ALLOC_BEST_FIT,
    ALLOC_STRATEGY_COUNT
} AllocationStrategy;

/* Standard System Error Codes */
typedef enum {
    STATUS_OK = 0,
    ERR_INVALID_PARAM,
    ERR_INVALID_TIME_RANGE,
    ERR_INTERVAL_OVERLAP,
    ERR_NO_COMPATIBLE_SLOT,
    ERR_TICKET_NOT_FOUND,
    ERR_ALREADY_OCCUPIED,
    ERR_ALREADY_CHECKED_IN,
    ERR_FILE_IO,
    ERR_CORRUPT_DATA,
    ERR_CAPACITY_FULL
} StatusCode;

/* Helper prototypes */
const char* category_to_string(VehicleCategory cat);
const char* strategy_to_string(AllocationStrategy strat);
const char* status_to_string(StatusCode code);
bool is_category_compatible(VehicleCategory vehicle_cat, VehicleCategory slot_cat);
double get_category_rate(VehicleCategory cat);

/* Time formatting and parsing utilities */
time_t parse_datetime_str(const char* dt_str);
void format_datetime_str(time_t t, char* buffer, size_t buflen);
time_t add_minutes(time_t base, int minutes);
time_t add_hours(time_t base, int hours);

#endif /* COMMON_H */
