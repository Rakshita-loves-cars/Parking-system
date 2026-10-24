#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "common.h"
#include "slot.h"
#include "allocation.h"
#include "manager.h"
#include "persistence.h"
#include "simulation.h"

static void clear_input_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

static void print_banner(void) {
    printf("\n%s", ANSI_COLOR_CYAN);
    printf("========================================================================\n");
    printf("   SMART PARKING SLOT ALLOCATION AND RESERVATION SYSTEM (SPSARS)       \n");
    printf("   Advanced C & DSA Implementation | Interval Scheduling & Best-Fit    \n");
    printf("========================================================================\n");
    printf("%s", ANSI_COLOR_RESET);
}

static void print_menu(void) {
    printf("\n%s--- MAIN CONTROL CONSOLE ---%s\n", ANSI_COLOR_BOLD, ANSI_COLOR_RESET);
    printf(" 1. View Facility Layout Map (Real-Time)\n");
    printf(" 2. Reserve a Parking Slot (Advance Booking)\n");
    printf(" 3. Vehicle Arrival & Check-In\n");
    printf(" 4. Vehicle Departure, Checkout & Billing\n");
    printf(" 5. Cancel Existing Reservation\n");
    printf(" 6. Inspect Specific Slot Timeline Schedule\n");
    printf(" 7. Look Up Ticket / Receipt Details\n");
    printf(" 8. Run Algorithmic Benchmark (Greedy vs Best-Fit)\n");
    printf(" 9. Export All Reservations to CSV\n");
    printf(" 10. View System Metrics & Revenue Audit\n");
    printf(" 11. Save System State to Disk\n");
    printf(" 12. Reload System State from Disk\n");
    printf(" 0. Exit Application\n");
    printf("Enter choice [0-12]: ");
}

static VehicleCategory prompt_vehicle_category(void) {
    printf("\nSelect Vehicle Category:\n");
    printf("  1. Compact Car (Hatchbacks, Small City Cars)\n");
    printf("  2. Standard Sedan / Crossover\n");
    printf("  3. Large SUV / Minivan / Pickup\n");
    printf("  4. Electric Vehicle (EV - Requires Charging Bay)\n");
    printf("  5. Accessible Vehicle (PRM - Accessibility Pass Required)\n");
    printf("Choice [1-5]: ");

    int cat_choice = 2;
    if (scanf("%d", &cat_choice) != 1) {
        clear_input_buffer();
        return CAT_STANDARD;
    }
    clear_input_buffer();

    switch (cat_choice) {
        case 1: return CAT_COMPACT;
        case 2: return CAT_STANDARD;
        case 3: return CAT_SUV;
        case 4: return CAT_EV;
        case 5: return CAT_ACCESSIBLE;
        default: return CAT_STANDARD;
    }
}

static void handle_reservation(ParkingFacility* facility) {
    printf("\n%s--- ADVANCE PARKING SLOT RESERVATION ---%s\n", ANSI_COLOR_BOLD, ANSI_COLOR_RESET);
    
    char reg[MAX_REG_LEN];
    printf("Enter Vehicle Registration Number (e.g., CH-01-AB-1234): ");
    if (scanf("%15s", reg) != 1) {
        clear_input_buffer();
        printf("Invalid registration input.\n");
        return;
    }
    clear_input_buffer();

    VehicleCategory cat = prompt_vehicle_category();

    printf("\nReservation Timing Options:\n");
    printf(" 1. Quick booking starting NOW (Current Local Time)\n");
    printf(" 2. Scheduled booking with custom start time\n");
    printf("Select timing option [1-2]: ");
    int t_opt = 1;
    if (scanf("%d", &t_opt) != 1) {
        clear_input_buffer();
        t_opt = 1;
    }
    clear_input_buffer();

    time_t start_time = time(NULL);
    if (t_opt == 2) {
        char s_buf[64];
        printf("Enter Start Datetime (YYYY-MM-DD HH:MM): ");
        if (fgets(s_buf, sizeof(s_buf), stdin)) {
            s_buf[strcspn(s_buf, "\r\n")] = 0;
            time_t parsed = parse_datetime_str(s_buf);
            if (parsed == (time_t)-1) {
                printf("Error parsing date format. Using current time.\n");
            } else {
                start_time = parsed;
            }
        }
    }

    printf("Enter Parking Duration in Hours (e.g., 2, 3): ");
    int duration_hours = 2;
    if (scanf("%d", &duration_hours) != 1 || duration_hours <= 0) {
        clear_input_buffer();
        duration_hours = 2;
    }
    clear_input_buffer();

    time_t end_time = add_hours(start_time, duration_hours);

    printf("\nSelect Allocation Strategy:\n");
    printf(" 1. Greedy Nearest-Slot (First-Fit by Distance to Entrance)\n");
    printf(" 2. Best-Fit (Minimizes Slot Waste & Time Slack)\n");
    printf("Select [1-2, default 2]: ");
    int s_choice = 2;
    if (scanf("%d", &s_choice) != 1) {
        clear_input_buffer();
        s_choice = 2;
    }
    clear_input_buffer();

    AllocationRequest req;
    memset(&req, 0, sizeof(AllocationRequest));
    strncpy(req.vehicle_reg, reg, MAX_REG_LEN - 1);
    req.vehicle_cat = cat;
    req.start_time = start_time;
    req.end_time = end_time;
    req.strategy = (s_choice == 1) ? ALLOC_GREEDY_DISTANCE : ALLOC_BEST_FIT;

    printf("\nProcessing reservation using algorithm: %s...\n", strategy_to_string(req.strategy));

    AllocationResult res = reserve_parking_slot(facility, &req);
    if (res.status == STATUS_OK) {
        printf("\n%s[SUCCESS] Slot Successfully Allocated!%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
        printf("\n%s========================================================================%s\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET);
        printf("%s*** IMPORTANT INSTRUCTION FOR VEHICLE OWNER ***%s\n", ANSI_COLOR_BOLD, ANSI_COLOR_RESET);
        printf(" Your Unique Ticket ID is: %s%s%s\n", ANSI_COLOR_BOLD, res.ticket_id, ANSI_COLOR_RESET);
        printf(" Please note down or screenshot this Ticket ID!\n");
        printf(" The vehicle owner MUST announce / provide this Ticket ID upon arrival\n");
        printf(" at the entry barrier to check-in and open the boom barrier.\n");
        printf("%s========================================================================%s\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET);
        display_ticket_receipt(facility, res.ticket_id);
    } else {
        printf("\n%s[ALLOCATION FAILED]%s %s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET, res.failure_reason);
    }
}

static void handle_checkin(ParkingFacility* facility) {
    printf("\n%s--- VEHICLE ARRIVAL AT ENTRY BARRIER ---%s\n", ANSI_COLOR_BOLD, ANSI_COLOR_RESET);
    printf(" 1. I have an Advance Reservation (Enter Ticket ID or Vehicle Reg)\n");
    printf(" 2. Drive-Up Walk-In Customer (No reservation, allocate slot on the spot)\n");
    printf("Select arrival type [1-2, default 1]: ");
    int arr_choice = 1;
    if (scanf("%d", &arr_choice) != 1) {
        clear_input_buffer();
        arr_choice = 1;
    }
    clear_input_buffer();

    if (arr_choice == 2) {
        /* Drive-up walk-in flow */
        printf("\n%s--- DRIVE-UP INSTANT ALLOCATION ---%s\n", ANSI_COLOR_BOLD, ANSI_COLOR_RESET);
        char reg[MAX_REG_LEN];
        printf("Enter Vehicle Registration Number (e.g., CH-01-AB-1234): ");
        if (scanf("%15s", reg) != 1) {
            clear_input_buffer();
            return;
        }
        clear_input_buffer();

        VehicleCategory cat = prompt_vehicle_category();

        printf("Enter Expected Parking Duration in Hours [default 2]: ");
        int duration_hours = 2;
        if (scanf("%d", &duration_hours) != 1 || duration_hours <= 0) {
            clear_input_buffer();
            duration_hours = 2;
        }
        clear_input_buffer();

        time_t now = time(NULL);
        time_t end_time = add_hours(now, duration_hours);

        AllocationRequest req;
        memset(&req, 0, sizeof(AllocationRequest));
        strncpy(req.vehicle_reg, reg, MAX_REG_LEN - 1);
        req.vehicle_cat = cat;
        req.start_time = now;
        req.end_time = end_time;
        req.strategy = ALLOC_BEST_FIT;

        AllocationResult res = reserve_parking_slot(facility, &req);
        if (res.status != STATUS_OK) {
            printf("\n%s[ENTRY DENIED]%s %s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET, res.failure_reason);
            printf("The parking facility is currently at capacity or all slots are reserved for advance bookings.\n");
            return;
        }

        /* Check the vehicle in immediately */
        check_in_vehicle(facility, res.ticket_id, now);

        int slot_idx = res.allocated_slot_id - 1;
        const ParkingSlot* s = &facility->slots[slot_idx];

        printf("\n%s+====================================================================+%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
        printf("%s|       DRIVE-UP ACCESS GRANTED - ENTRY BOOM BARRIER OPENED          |%s\n", ANSI_COLOR_BOLD, ANSI_COLOR_RESET);
        printf("%s+====================================================================+%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
        printf("  Drive-Up Ticket ID : %s%s%s (KEEP THIS TO EXIT & PAY)\n", ANSI_COLOR_BOLD, res.ticket_id, ANSI_COLOR_RESET);
        printf("  Vehicle Reg Number : %s [%s]\n", reg, category_to_string(cat));
        printf("  %sYOUR ASSIGNED BAY : SLOT #%02d (FLOOR %d, BAY %d)%s\n", 
               ANSI_COLOR_BOLD, s->slot_id, s->floor, s->bay_number, ANSI_COLOR_RESET);
        printf("  Bay Category       : %s\n", category_to_string(s->slot_cat));
        printf("  Walking Distance   : %.1f meters to main pedestrian entrance\n", s->distance_from_entrance);
        printf("  Parking Period     : Instant Access from NOW\n");
        printf("  Status             : Barrier raised. Please proceed directly to Slot #%02d.\n", s->slot_id);
        printf("%s+====================================================================+%s\n\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
        return;
    }

    /* Advance reservation flow */
    printf("\nPlease enter your Ticket ID (or Vehicle Registration Number):\n");
    char ticket_input[MAX_TICKET_LEN];
    printf("Ticket ID / Vehicle Reg: ");
    if (scanf("%31s", ticket_input) != 1) {
        clear_input_buffer();
        return;
    }
    clear_input_buffer();

    int slot_idx = -1;
    ReservationNode* node = find_reservation_by_ticket_or_reg(facility, ticket_input, &slot_idx);
    if (!node || slot_idx < 0) {
        printf("\n%s[CHECK-IN FAILED]%s Ticket ID / Vehicle '%s' not found in facility reservations!\n", 
               ANSI_COLOR_RED, ANSI_COLOR_RESET, ticket_input);
        printf("If you did not reserve in advance, please select Option 2 for Drive-Up entry.\n");
        return;
    }

    time_t now = time(NULL);
    StatusCode code = check_in_vehicle(facility, node->ticket_id, now);
    if (code == STATUS_OK) {
        const ParkingSlot* s = &facility->slots[slot_idx];
        printf("\n%s+====================================================================+%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
        printf("%s|               ENTRY BARRIER OPENED - ACCESS GRANTED                |%s\n", ANSI_COLOR_BOLD, ANSI_COLOR_RESET);
        printf("%s+====================================================================+%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
        printf("  Ticket ID Verified : %s%s%s\n", ANSI_COLOR_BOLD, node->ticket_id, ANSI_COLOR_RESET);
        printf("  Vehicle Reg Number : %s [%s]\n", node->vehicle_reg, category_to_string(node->vehicle_cat));
        printf("  %sYOUR ASSIGNED BAY : SLOT #%02d (FLOOR %d, BAY %d)%s\n", 
               ANSI_COLOR_BOLD, s->slot_id, s->floor, s->bay_number, ANSI_COLOR_RESET);
        printf("  Bay Category       : %s\n", category_to_string(s->slot_cat));
        printf("  Walking Distance   : %.1f meters to main pedestrian entrance\n", s->distance_from_entrance);
        printf("  Status             : Barrier raised. Please proceed directly to Slot #%02d.\n", s->slot_id);
        printf("%s+====================================================================+%s\n\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
    } else {
        printf("\n%s[CHECK-IN FAILED]%s %s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET, status_to_string(code));
    }
}

static void handle_checkout(ParkingFacility* facility) {
    printf("\n%s--- VEHICLE DEPARTURE & BILLING ---%s\n", ANSI_COLOR_BOLD, ANSI_COLOR_RESET);
    char ticket_id[MAX_TICKET_LEN];
    printf("Enter Ticket ID: ");
    if (scanf("%31s", ticket_id) != 1) {
        clear_input_buffer();
        return;
    }
    clear_input_buffer();

    time_t now = time(NULL);
    double total_fee = 0.0;
    StatusCode code = check_out_vehicle(facility, ticket_id, now, &total_fee);
    if (code == STATUS_OK) {
        printf("\n%s[SUCCESS] Vehicle checked out. Slot released!%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
        display_ticket_receipt(facility, ticket_id);
    } else {
        printf("\n%s[FAILED]%s %s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET, status_to_string(code));
    }
}

static void handle_cancellation(ParkingFacility* facility) {
    printf("\n%s--- CANCEL RESERVATION ---%s\n", ANSI_COLOR_BOLD, ANSI_COLOR_RESET);
    char ticket_id[MAX_TICKET_LEN];
    printf("Enter Ticket ID to Cancel: ");
    if (scanf("%31s", ticket_id) != 1) {
        clear_input_buffer();
        return;
    }
    clear_input_buffer();

    StatusCode code = cancel_reservation(facility, ticket_id);
    if (code == STATUS_OK) {
        printf("\n%s[SUCCESS] Reservation cancelled and slot time window released.%s\n", 
               ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
    } else {
        printf("\n%s[FAILED]%s %s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET, status_to_string(code));
    }
}

static void handle_timeline_view(const ParkingFacility* facility) {
    printf("\nEnter Slot ID to Inspect (1 to %d): ", facility->total_slots);
    int slot_id = 1;
    if (scanf("%d", &slot_id) != 1) {
        clear_input_buffer();
        return;
    }
    clear_input_buffer();

    display_slot_timeline(facility, slot_id);
}

static void handle_ticket_lookup(const ParkingFacility* facility) {
    char ticket_id[MAX_TICKET_LEN];
    printf("\nEnter Ticket ID: ");
    if (scanf("%31s", ticket_id) != 1) {
        clear_input_buffer();
        return;
    }
    clear_input_buffer();

    display_ticket_receipt(facility, ticket_id);
}

static void handle_simulation_benchmark(void) {
    printf("\n%s--- AUTOMATED ALGORITHM BENCHMARK SIMULATOR ---%s\n", ANSI_COLOR_BOLD, ANSI_COLOR_RESET);
    printf("This simulation generates synthetic vehicle requests with realistic Poisson-distributed\n");
    printf("arrival times and vehicle category ratios to compare Greedy vs Best-Fit allocation.\n\n");

    int num_req = 100;
    printf("Enter number of simulated requests [default 100]: ");
    if (scanf("%d", &num_req) != 1 || num_req <= 0) {
        clear_input_buffer();
        num_req = 100;
    }
    clear_input_buffer();

    SimulationConfig cfg;
    cfg.num_slots = 24;
    cfg.num_requests = num_req;
    cfg.base_time_hours = 12;
    cfg.seed = 42;
    cfg.load_factor = 1.0;

    printf("\nRunning benchmark across %d requests for Greedy Nearest-Slot...\n", num_req);
    BenchmarkMetrics greedy_res = run_strategy_benchmark(ALLOC_GREEDY_DISTANCE, &cfg);

    printf("Running benchmark across %d requests for Best-Fit (Multi-Objective)...\n", num_req);
    BenchmarkMetrics bestfit_res = run_strategy_benchmark(ALLOC_BEST_FIT, &cfg);

    display_benchmark_comparison(&greedy_res, &bestfit_res);
}

int main(void) {
    /* Ensure data directory exists */
    mkdir("data", 0755);

    ParkingFacility facility;
    /* Initialize default facility with 2 floors, 12 slots each (24 slots) */
    init_parking_facility(&facility, "Chandigarh SmartPark Center", DEFAULT_FLOORS, DEFAULT_SLOTS_PER_FLOOR);

    /* Try loading saved state if present */
    if (access(DEFAULT_DATA_FILE, F_OK) == 0) {
        if (load_facility_state(&facility, DEFAULT_DATA_FILE) == STATUS_OK) {
            printf("%s[INFO] Loaded existing facility state from %s%s\n", 
                   ANSI_COLOR_CYAN, DEFAULT_DATA_FILE, ANSI_COLOR_RESET);
        }
    }

    print_banner();

    int choice = -1;
    while (choice != 0) {
        print_menu();
        if (scanf("%d", &choice) != 1) {
            clear_input_buffer();
            printf("Invalid input. Please enter a valid number.\n");
            continue;
        }
        clear_input_buffer();

        switch (choice) {
            case 1:
                display_lot_map(&facility, time(NULL));
                break;
            case 2:
                handle_reservation(&facility);
                break;
            case 3:
                handle_checkin(&facility);
                break;
            case 4:
                handle_checkout(&facility);
                break;
            case 5:
                handle_cancellation(&facility);
                break;
            case 6:
                handle_timeline_view(&facility);
                break;
            case 7:
                handle_ticket_lookup(&facility);
                break;
            case 8:
                handle_simulation_benchmark();
                break;
            case 9:
                if (export_reservations_to_csv(&facility, DEFAULT_CSV_EXPORT) == STATUS_OK) {
                    printf("%s[SUCCESS] Reservations exported to %s%s\n", 
                           ANSI_COLOR_GREEN, DEFAULT_CSV_EXPORT, ANSI_COLOR_RESET);
                } else {
                    printf("%s[FAILED] Could not export reservations to CSV.%s\n", 
                           ANSI_COLOR_RED, ANSI_COLOR_RESET);
                }
                break;
            case 10:
                display_facility_statistics(&facility);
                break;
            case 11:
                if (save_facility_state(&facility, DEFAULT_DATA_FILE) == STATUS_OK) {
                    printf("%s[SUCCESS] System state saved to %s%s\n", 
                           ANSI_COLOR_GREEN, DEFAULT_DATA_FILE, ANSI_COLOR_RESET);
                } else {
                    printf("%s[FAILED] Error saving system state.%s\n", 
                           ANSI_COLOR_RED, ANSI_COLOR_RESET);
                }
                break;
            case 12:
                if (load_facility_state(&facility, DEFAULT_DATA_FILE) == STATUS_OK) {
                    printf("%s[SUCCESS] System state reloaded from %s%s\n", 
                           ANSI_COLOR_GREEN, DEFAULT_DATA_FILE, ANSI_COLOR_RESET);
                } else {
                    printf("%s[FAILED] Error reloading state file.%s\n", 
                           ANSI_COLOR_RED, ANSI_COLOR_RESET);
                }
                break;
            case 0:
                printf("\nSaving state and shutting down...\n");
                save_facility_state(&facility, DEFAULT_DATA_FILE);
                break;
            default:
                printf("Unknown option %d. Please select an option between 0 and 12.\n", choice);
                break;
        }
    }

    free_parking_facility(&facility);
    printf("Goodbye!\n");
    return 0;
}
