# Smart Parking System: Comprehensive Guide

---

## 1. How the Project Works

The **Smart Parking Slot Allocation and Reservation System (SPSARS)** automates parking facility management by combining interval scheduling with multi-objective resource allocation:

1. **Facility Initialization:**
   The lot is configured with physical slots across multiple floors. Each bay has designated physical properties: category (`Compact`, `Sedan`, `SUV`, `EV`, `Accessible`), walking distance to the main entrance, and real-time physical status (`VACANT` vs. `OCCUPIED`).

2. **Dual-Mode Allocation (Advance Booking or Drive-Up):**
   * **Advance Booking:** A user requests a parking window $[t_{\text{start}}, t_{\text{end}})$ ahead of time.
   * **Drive-Up Walk-In:** A driver arrives directly at the entry barrier without prior reservation. The system searches for an immediately available slot starting `NOW` and automatically assigns it.

3. **Temporal Collision Engine & Compatibility Rules:**
   * Each parking slot maintains its own chronologically sorted linked list of half-open intervals $[S, E)$.
   * Two bookings conflict if and only if $\max(S_1, S_2) < \min(E_1, E_2)$. Touching boundaries (e.g., 10:00–12:00 and 12:00–14:00) produce zero conflict, allowing back-to-back parking.
   * Hard constraints prevent violations: Accessible bays are strictly protected for mobility-impaired drivers, EV charging bays cannot be occupied by combustion vehicles, and oversized vehicles cannot enter small bays.

4. **Algorithmic Selection:**
   * **Greedy Nearest:** Selects the compatible, available bay with the minimum walking distance to the pedestrian exit.
   * **Best-Fit:** Evaluates a composite cost function penalizing dimension waste (preventing compact cars from taking SUV bays) and residual time slack (packing reservations tightly).

5. **Arrival, Check-In & Gate Control:**
   The vehicle owner announces/enters their unique Ticket ID at the barrier kiosk. Upon validation, the physical bay transitions to `OCCUPIED` (Red), the barrier opens, and visual directions to the assigned bay are displayed.

6. **Departure & Dynamic Billing:**
   On checkout, the system computes total charges based on base entry fees, tiered category hourly tariffs, and an automatic **2x penalty on unauthorized overstays** past the reserved window. The bay reverts to `VACANT` (Green).

7. **Persistence & Auditing:**
   The entire facility state is stored in `data/parking_state.dat` protected by a Fletcher-32 checksum to detect file corruption. System records can also be exported to CSV.

---

## 2. File Directory & Purpose (2 Lines Each)

* **`include/common.h`**  
  Defines global enumerations, error codes, color macros, time utilities, and data limits.  
  Acts as the central configuration header shared across all modules in the project.

* **`src/common.c`**  
  Implements vehicle category compatibility rules, tariff pricing tables, and string conversions.  
  Provides robust datetime parsing and timestamp formatting functions using standard C libraries.

* **`include/slot.h`**  
  Declares the `ParkingSlot` structure and the `ReservationNode` linked list interval representation.  
  Provides function prototypes for interval overlap detection, schedule queries, and slot metrics.

* **`src/slot.c`**  
  Implements half-open $[S, E)$ interval math and sorted linked list insertion with early exit.  
  Calculates dimension waste scores and residual time slack for candidate parking slots.

* **`include/allocation.h`**  
  Defines request and result structures for both Greedy Nearest and Best-Fit allocation routines.  
  Exposes the top-level algorithm dispatcher used by the parking coordinator.

* **`src/allocation.c`**  
  Executes Greedy Distance allocation and multi-objective Best-Fit optimization algorithms.  
  Generates unique ticket identifiers and binds approved reservation intervals to designated slots.

* **`include/manager.h`**  
  Declares the `ParkingFacility` container, capacity limits, and operational lifecycle signatures.  
  Defines interface contracts for bookings, cancellations, check-in, checkout, and billing.

* **`src/manager.c`**  
  Coordinates check-ins, exits, dynamic penalty calculations, and ticket lookups.  
  Renders real-time multi-floor ASCII layout maps with ANSI color-coded status badges.

* **`include/persistence.h`**  
  Specifies binary file header layouts, record serialization formats, and default file paths.  
  Declares functions for saving, loading, verifying checksums, and exporting reservation data.

* **`src/persistence.c`**  
  Serializes and deserializes lot state to disk while computing a Fletcher-32 integrity checksum.  
  Generates standardized CSV audit exports of all active and historical reservation records.

* **`include/simulation.h`**  
  Defines benchmarking metrics, test scenario configurations, and request generation prototypes.  
  Supplies interface contracts for running comparative evaluation experiments.

* **`src/simulation.c`**  
  Generates synthetic vehicle workloads with realistic category mixes and Poisson arrival times.  
  Measures microsecond latency, rejection rates, distance, and dimension waste per strategy.

* **`src/main.c`**  
  Drives the interactive command-line interface with a 12-option user navigation console.  
  Connects user inputs to reservation workflows, live gate check-in, and benchmark triggers.

* **`tests/test_intervals.c`**  
  Executes unit tests verifying half-open interval overlap math, edge cases, and touching boundaries.  
  Validates sorted linked list insertion, gap filling, and memory reclamation routines.

* **`tests/test_allocation.c`**  
  Validates vehicle compatibility rules, accessibility pass enforcement, and EV bay protection.  
  Empirically tests slot cannibalization prevention where Best-Fit succeeds and Greedy fails.

* **`tests/run_experiments.c`**  
  Automates multi-load experimental benchmarks across 30, 60, 100, and 200 vehicle requests.  
  Outputs comprehensive comparison tables demonstrating algorithmic trade-offs under saturation.

* **`Makefile`**  
  Automates building binaries, running test suites, launching benchmarks, and cleaning artifacts.  
  Enforces strict C99 compilation flags (`-Wall -Wextra -pedantic -O2`) across all targets.

* **`README.md`**  
  Provides public repository documentation, feature overviews, build commands, and UI guides.  
  Details installation instructions and experimental findings for general developers.

* **`PROJECT_REPORT.md`**  
  Contains an exhaustive university-level academic report with mathematical problem formulations.  
  Presents theoretical complexity proofs, empirical data tables, and future concurrency designs.

---

## 3. Advantages

1. **Sub-Microsecond Algorithmic Performance:**  
   Because each slot maintains reservations sorted by start time, collision checking terminates early in $O(k)$ time ($k \ll M$), yielding allocation latency below 1 microsecond.

2. **Decoupled Physical vs. Temporal States:**  
   Eliminates ambiguity between live presence and forward bookings: a slot can be physically vacant right now while securely reserved for a driver arriving later.

3. **Prevention of Slot Cannibalization (Best-Fit Advantage):**  
   Under heavy traffic, Best-Fit avoids allocating large SUV or EV bays to small compact cars, achieving up to 10–15% higher allocation success rates than Greedy distance heuristics.

4. **Strict Accessibility & Infrastructure Guarantees:**  
   Mandatory accessibility (PRM) bays and high-speed EV charging bays are protected through strict compatibility rules, preventing non-qualifying vehicles from blocking specialized infrastructure.

5. **Fair Billing with Overstay Deterrence:**  
   Ceiling-hour conversion and an automated 2x overstay tariff penalize drivers who overstay their reserved window, discouraging lot congestion.

6. **Robust Data Integrity & Portability:**  
   State files are validated on boot with a Fletcher-32 checksum to guard against data corruption, and the entire project runs on standard C99 with zero external library dependencies.

---

## 4. Drawbacks

1. **Single-Process Execution (No Concurrency Yet):**  
   The current implementation processes requests sequentially in a single terminal process. Supporting simultaneous web/mobile bookings requires multi-threading with mutex locks per slot.

2. **Static Distance Metric (No Pathfinding Graph):**  
   Slot distances to the entrance are pre-assigned static scalars rather than dynamically computed through graph shortest-path algorithms (e.g., Dijkstra's algorithm over garage drive aisles).

3. **In-Memory Scale Bounded by Host RAM:**  
   All active slot records and linked lists reside in main memory. While efficient for garages up to several hundred slots, multi-thousand-slot city networks would benefit from an indexed relational or key-value storage engine.

4. **Terminal Interface Limitations:**  
   The system relies on an interactive CLI menu without a graphical user interface (GUI), web dashboard, or physical IoT sensor/camera hardware integration.
