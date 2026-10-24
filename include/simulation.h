#ifndef SIMULATION_H
#define SIMULATION_H

#include "allocation.h"
#include "common.h"
#include "manager.h"

/* Metrics collected during experimental evaluation */
typedef struct {
  AllocationStrategy strategy;
  int total_requests;
  int successful_allocations;
  int rejected_requests;
  double success_rate_pct;
  double avg_allocation_latency_us; /* in microseconds */
  double avg_walking_distance_meters;
  double avg_dimension_waste;
  double avg_time_slack_hours;
  double final_occupancy_rate_pct;
  double total_revenue_generated;
} BenchmarkMetrics;

/* Test Scenario Configuration */
typedef struct {
  int num_slots;
  int num_requests;
  int base_time_hours;
  int seed;
  double load_factor; /* 0.5 to 1.5 of theoretical capacity */
} SimulationConfig;

/* Synthetic Request Generator */
AllocationRequest generate_random_request(time_t base_time,
                                          int time_window_hours, int req_idx);

/* Benchmark Runner */
BenchmarkMetrics run_strategy_benchmark(AllocationStrategy strategy,
                                        const SimulationConfig *config);

void display_benchmark_comparison(const BenchmarkMetrics *greedy_res,
                                  const BenchmarkMetrics *bestfit_res);

#endif /* SIMULATION_H */
