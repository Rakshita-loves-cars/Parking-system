#include "simulation.h"
#include <time.h>

AllocationRequest generate_random_request(time_t base_time, int time_window_hours, int req_idx) {
    AllocationRequest req;
    memset(&req, 0, sizeof(AllocationRequest));

    /* Generate alphanumeric vehicle registration */
    snprintf(req.vehicle_reg, sizeof(req.vehicle_reg), "CH-01-%04d", (req_idx * 17 + 1000) % 9999);

    /* Realistic vehicle distribution:
     * 30% Compact, 40% Standard, 15% SUV, 10% EV, 5% Accessible
     */
    int r = rand() % 100;
    if (r < 30) {
        req.vehicle_cat = CAT_COMPACT;
    } else if (r < 70) {
        req.vehicle_cat = CAT_STANDARD;
    } else if (r < 85) {
        req.vehicle_cat = CAT_SUV;
    } else if (r < 95) {
        req.vehicle_cat = CAT_EV;
    } else {
        req.vehicle_cat = CAT_ACCESSIBLE;
    }

    /* Start time staggered within time_window_hours */
    int start_offset_minutes = (rand() % (time_window_hours * 60));
    /* Duration between 1 hour (60 min) and 4 hours (240 min) */
    int duration_minutes = 60 + (rand() % 181);

    req.start_time = add_minutes(base_time, start_offset_minutes);
    req.end_time   = add_minutes(req.start_time, duration_minutes);

    return req;
}

static double get_elapsed_microseconds(struct timespec* start, struct timespec* end) {
    double sec = (double)(end->tv_sec - start->tv_sec);
    double nsec = (double)(end->tv_nsec - start->tv_nsec);
    return (sec * 1e6) + (nsec / 1e3);
}

BenchmarkMetrics run_strategy_benchmark(AllocationStrategy strategy, 
                                        const SimulationConfig* config) {
    BenchmarkMetrics metrics;
    memset(&metrics, 0, sizeof(BenchmarkMetrics));
    metrics.strategy = strategy;
    metrics.total_requests = config->num_requests;

    /* Initialize temporary simulation facility */
    ParkingFacility facility;
    int floors = (config->num_slots > 24) ? 4 : 2;
    int slots_per_floor = config->num_slots / floors;
    init_parking_facility(&facility, "BenchmarkSim", floors, slots_per_floor);

    srand(config->seed);
    time_t base_time = time(NULL);

    double total_latency_us = 0.0;
    double total_distance = 0.0;
    double total_dim_waste = 0.0;
    double total_slack = 0.0;

    for (int i = 0; i < config->num_requests; ++i) {
        AllocationRequest req = generate_random_request(base_time, config->base_time_hours, i + 1);
        req.strategy = strategy;

        struct timespec t_start, t_end;
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        AllocationResult res = reserve_parking_slot(&facility, &req);

        clock_gettime(CLOCK_MONOTONIC, &t_end);
        total_latency_us += get_elapsed_microseconds(&t_start, &t_end);

        if (res.status == STATUS_OK) {
            metrics.successful_allocations++;
            total_distance += res.slot_distance;

            int slot_idx = res.allocated_slot_id - 1;
            total_dim_waste += calculate_dimension_waste(req.vehicle_cat, facility.slots[slot_idx].slot_cat);
            total_slack += calculate_time_slack(&facility.slots[slot_idx], req.start_time, req.end_time);
        } else {
            metrics.rejected_requests++;
        }
    }

    metrics.success_rate_pct = ((double)metrics.successful_allocations / (double)config->num_requests) * 100.0;
    metrics.avg_allocation_latency_us = total_latency_us / (double)config->num_requests;
    
    if (metrics.successful_allocations > 0) {
        metrics.avg_walking_distance_meters = total_distance / (double)metrics.successful_allocations;
        metrics.avg_dimension_waste = total_dim_waste / (double)metrics.successful_allocations;
        metrics.avg_time_slack_hours = total_slack / (double)metrics.successful_allocations;
    }

    int occupied_slots = 0;
    for (int i = 0; i < facility.total_slots; ++i) {
        if (facility.slots[i].active_reservations_count > 0) {
            occupied_slots++;
        }
    }
    metrics.final_occupancy_rate_pct = ((double)occupied_slots / (double)facility.total_slots) * 100.0;

    free_parking_facility(&facility);
    return metrics;
}

void display_benchmark_comparison(const BenchmarkMetrics* greedy, 
                                 const BenchmarkMetrics* bestfit) {
    if (!greedy || !bestfit) return;

    printf("\n========================================================================================\n");
    printf("        EMPIRICAL ALGORITHM BENCHMARK & COMPARISON EVALUATION REPORT                    \n");
    printf("========================================================================================\n");
    printf(" Total Synthetic Vehicle Requests Processed : %d\n", greedy->total_requests);
    printf(" Test Environment                           : Single-threaded C99 GCC (POSIX monotonic clock)\n");
    printf("----------------------------------------------------------------------------------------\n");
    printf(" %-32s | %-22s | %-22s\n", "Metric / Performance Indicator", "Greedy Nearest (First-Fit)", "Best-Fit (Multi-Objective)");
    printf("----------------------------------------------------------------------------------------\n");
    printf(" %-32s | %-22d | %-22d\n", "Successful Allocations", greedy->successful_allocations, bestfit->successful_allocations);
    printf(" %-32s | %-22d | %-22d\n", "Rejected Requests", greedy->rejected_requests, bestfit->rejected_requests);
    printf(" %-32s | %20.2f %% | %20.2f %%\n", "Success Rate", greedy->success_rate_pct, bestfit->success_rate_pct);
    printf(" %-32s | %18.2f us | %18.2f us\n", "Mean Allocation Latency", greedy->avg_allocation_latency_us, bestfit->avg_allocation_latency_us);
    printf(" %-32s | %19.2f m | %19.2f m\n", "Mean Walking Distance", greedy->avg_walking_distance_meters, bestfit->avg_walking_distance_meters);
    printf(" %-32s | %22.3f | %22.3f\n", "Mean Dimension Waste Score", greedy->avg_dimension_waste, bestfit->avg_dimension_waste);
    printf(" %-32s | %17.2f hrs | %17.2f hrs\n", "Mean Time Slack (Gap)", greedy->avg_time_slack_hours, bestfit->avg_time_slack_hours);
    printf(" %-32s | %20.2f %% | %20.2f %%\n", "Slot Utilization / Occupancy", greedy->final_occupancy_rate_pct, bestfit->final_occupancy_rate_pct);
    printf("========================================================================================\n");
    printf(" Algorithmic Insights:\n");
    printf(" 1. Greedy Distance achieves lower mean walking distance by prioritizing slots near entrance,\n");
    printf("    but frequently wastes large slots for smaller cars, causing higher rejection for SUVs.\n");
    printf(" 2. Best-Fit preserves specialized slot capacity (SUV/EV) and packs schedules tightly,\n");
    printf("    improving overall acceptance rate under high-load conditions.\n");
    printf("========================================================================================\n\n");
}
