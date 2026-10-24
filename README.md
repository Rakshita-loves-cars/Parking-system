# Smart Parking Slot Allocation and Reservation System (SPSARS)
### Advanced C & Data Structures Implementation

A high-performance, algorithmic Smart Parking Reservation and Slot Allocation Engine written in pure C (C99 standard). The system models multi-floor parking facilities, interval scheduling with half-open temporal collision detection $[S, E)$, vehicle compatibility lattices (Compact, Sedan, SUV, Electric Vehicles with chargers, and Accessible PRM bays), dynamic billing with overstay penalties, binary persistence with checksum validation, and an automated experimental simulation framework comparing **Greedy Distance** vs. **Best-Fit Multi-Objective Allocation**.

---

## Key Features

1. **Physical vs. Temporal Decoupling**: Distinguishes whether a parking bay is physically occupied right now vs. reserved for a future time window.
2. **Interval Collision Engine**: Uses mathematically sound half-open intervals $[S, E)$ to guarantee non-overlapping bookings while permitting exact back-to-back reservations (e.g. 10:00–12:00 and 12:00–14:00 on the same bay).
3. **Chronological Linked Lists**: Each parking slot maintains a dynamically sized, chronologically sorted singly-linked list of reservations with $O(k)$ early-termination collision checks.
4. **Vehicle Compatibility & Accessibility Lattice**:
   - `Accessible (PRM)`: Exclusively reserved for drivers with accessibility permits; non-accessible vehicles are strictly prohibited.
   - `EV`: Equipped with high-speed charging; reserved exclusively for Electric Vehicles.
   - `Compact`, `Standard Sedan`, `Large SUV`: Size hierarchies strictly prevent oversized vehicles from occupying undersized bays.
5. **Algorithmic Strategies**:
   - **Greedy Nearest-Slot (First-Fit by Distance)**: Prioritizes minimal walking distance to the entrance terminal.
   - **Best-Fit (Multi-Objective Optimization)**: Minimizes dimension waste (preventing compact cars from cannibalizing SUV bays) and residual time slack.
6. **Dynamic Billing Engine**:
   - Base entry tariff + tiered category hourly rates.
   - Exact minute-level tracking converted to ceiling billing hours.
   - Automatic 2x penalty rate for unauthorized overstay past the reserved window.
7. **Robust Persistence & Auditing**:
   - Binary serialization (`data/parking_state.dat`) with magic number header (`0x5041524B`) and Fletcher-style rolling checksum validation against corruption.
   - One-click CSV export (`data/reservations_export.csv`) for auditing.
8. **Dual Mode Entry (Advance Reservation & Drive-Up Walk-In)**:
   - Advance booking with unique Ticket ID issuance and slot pre-allocation.
   - On-the-spot Drive-Up check-in at the boom barrier for unreserved walk-in vehicles.
9. **Automated Experimental Simulator**:
   - Monte-Carlo style synthetic workload generator with realistic vehicle category distributions.
   - Microsecond-accurate latency measurements (`clock_gettime(CLOCK_MONOTONIC)`).

---

## Directory Structure

```text
chandigarh/
├── Makefile                     # Build automation (all, clean, test, experiments)
├── README.md                    # Project documentation & build guide
├── PROJECT_REPORT.md            # Comprehensive university-level project report
├── include/                     # Header files
│   ├── common.h                 # Shared constants, enums, ANSI colors, date utilities
│   ├── slot.h                   # Slot and Reservation linked list structures
│   ├── allocation.h             # Greedy and Best-Fit algorithm signatures
│   ├── manager.h                # Operational lifecycle, check-in, check-out, billing
│   ├── persistence.h            # Binary and CSV file handling
│   └── simulation.h             # Workload generator and benchmark metrics
├── src/                         # Core C implementations
│   ├── common.c                 # Utility implementations and compatibility lattice
│   ├── slot.c                   # Interval overlap detection and list operations
│   ├── allocation.c             # Greedy vs Best-Fit allocation logic
│   ├── manager.c                # Parking facility management & terminal UI
│   ├── persistence.c            # File serialization with checksum integrity
│   ├── simulation.c             # Synthetic benchmarking engine
│   └── main.c                   # Interactive CLI console application
├── tests/                       # Unit and experimental test harnesses
│   ├── test_intervals.c         # Unit tests for interval collision edge cases
│   ├── test_allocation.c        # Tests for compatibility rules and slot cannibalization
│   └── run_experiments.c        # Automated multi-load benchmark experiments
└── data/                        # Persistent store and CSV export
    ├── parking_state.dat        # System database (binary)
    └── reservations_export.csv  # CSV audit report
```

---

## Compilation and Execution

The project compiles with standard GCC or Clang on Linux, macOS, and POSIX environments without external dependencies.

### 1. Build the Main Application
```bash
make all
```

### 2. Run the Interactive Terminal Application
```bash
./bin/smartpark
# Or directly via make:
make run
```

### 3. Run the Unit Test Suites
```bash
make test
```
*Executes `bin/test_intervals` and `bin/test_allocation`, validating boundary conditions, touching intervals, sub-interval containment, accessibility constraints, and anti-cannibalization logic.*

### 4. Run the Automated Experimental Benchmark
```bash
make experiments
```
*Runs synthetic tests across 30, 60, 100, and 200 vehicle requests, logging microsecond latency, success rates, walking distances, and dimension waste.*

---

## Terminal UI Demonstration

Upon launching `bin/smartpark`, the interactive menu provides:

```text
========================================================================
   SMART PARKING SLOT ALLOCATION AND RESERVATION SYSTEM (SPSARS)       
   Advanced C & DSA Implementation | Interval Scheduling & Best-Fit    
========================================================================

--- MAIN CONTROL CONSOLE ---
 1. View Facility Layout Map (Real-Time)
 2. Reserve a Parking Slot (Advance Booking)
 3. Vehicle Arrival & Check-In
 4. Vehicle Departure, Checkout & Billing
 5. Cancel Existing Reservation
 6. Inspect Specific Slot Timeline Schedule
 7. Look Up Ticket / Receipt Details
 8. Run Algorithmic Benchmark (Greedy vs Best-Fit)
 9. Export All Reservations to CSV
 10. View System Metrics & Revenue Audit
 11. Save System State to Disk
 12. Reload System State from Disk
 0. Exit Application
```

### Color-Coded Facility Grid
Selecting Option `1` displays the multi-floor facility:
- `[S01:A-VAC]` Green: Vacant Accessible bay.
- `[S03:E-VAC]` Green: Vacant Electric Vehicle charging bay.
- `[S05:C-OCC]` Red: Currently physically occupied by an on-site vehicle.
- `[S09:S-RES]` Yellow: Reserved for an upcoming or active reservation window.
