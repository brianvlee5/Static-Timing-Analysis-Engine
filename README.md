# Static Timing Analysis (STA) Engine

![Language](https://img.shields.io/badge/Language-C%2B%2B11-blue.svg)
![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey.svg)
![EDA](https://img.shields.io/badge/Area-Static%20Timing%20Analysis-orange.svg)

## 📖 Introduction
This project implements a **gate-level Static Timing Analysis (STA)** engine for combinational circuits. The engine parses a Verilog netlist and a standard cell library (Liberty format) to construct a timing graph. It performs topological analysis to calculate propagation delays, transition times, and identifies the **longest (critical)** and **shortest paths** under specific input patterns.

The implementation follows a 4-step analysis flow:
1.  **Graph Construction & Loading Calculation**: Building the DAG and computing output capacitance.
2.  **Timing Propagation**: Calculating delay and slew (transition) for each instance.
3.  **Path Analysis**: Identifying the critical path (longest delay) and the shortest path.
4.  **Gate Information Reporting**: Verifying logic values and timing for specific input vectors.

---

## 🚀 Key Features
* **Robust Parsing**: Handles flattened gate-level Verilog netlists and simplified Liberty (`.lib`) files.
* **Topological Timing Propagation**: Propagates arrival times and transition slews through the circuit graph.
* **Non-Linear Delay Model (NLDM)**: Uses lookup tables (LUT) from the library to interpolate delay and output transition based on *Input Transition* and *Output Load*.
* **Worst-Case Analysis**: Automatically evaluates both rising and falling output transitions to determine the worst-case propagation delay.

---

## ⚙️ Specifications & Assumptions
To ensure accurate analysis, the engine adheres to the following boundary conditions and constraints:

* **Circuit Type**: Purely combinational circuits (no flip-flops/latches).
* **Supported Gates**: `NAND2`, `NOR2`, `INV` (Inverter).
* **Wire Delay**: Fixed at **0.005 ns** for every connection.
* **Primary Inputs**: Input transition time is set to **0 ns**.
* **Primary Outputs**: Output loading is fixed at **0.03 pF**.
* **Timing Logic**:
    * If a cell has multiple inputs, the **latest arriving** signal defines the input transition time.
    * Output delay is the maximum of the *Rise* and *Fall* scenarios.

---

## 📊 Performance
The following table summarizes the runtime performance and analysis results on standard benchmarks.

| Benchmark | # Gates | # Nets | Critical Path (ns) | Runtime (s) |
| :--- | :--- | :--- | :--- | :--- |
| **c17** | 7 | 12 | 0.16 | 0.005s |
| **c432** | 171 | 207 | 2.51 | 0.006s |
| **c54321** | 41,449 | 54,321 | 0.42 | 1.33s |
| **c123456** | 80,438 | 123,456 | 0.16 | 2.99s |

---

## 🛠️ Build & Usage

### Prerequisites
* GCC/Clang supporting C++ STL
* GNU Make

### Compilation
Navigate to the project root and run:
```bash
    make
```

This will generate the executable named `sta`.

### Execution
The program requires three input arguments:
```bash

    ./sta <netlist_file> -l <lib_file> -i <input_patterns_file>
```

* `<netlist_file>`: The circuit structure (Verilog format).
* `<lib_file>`: Standard cell library containing timing tables.
* `<input_patterns_file>`: Input vectors for logic verification.

#### Example

```bash
    ./sta example.v -l test_lib.lib -i input.txt
```
---

## 📂 Output Files
Upon successful execution, the tool generates four report files corresponding to the analysis steps:

| Step | File Name Pattern | Description |
| :--- | :--- | :--- |
| **1** | `<lib>_<module>_load.txt` | Reports output loading (capacitance) for each instance. |
| **2** | `<lib>_<module>_delay.txt` | Reports worst-case propagation delay and output transition time. |
| **3** | `<lib>_<module>_path.txt` | Lists the **Longest** and **Shortest** delay paths through the circuit. |
| **4** | `<lib>_<module>_gate_info.txt` | Details logic value, cell delay, and transition for specific input patterns. |

---

## 🔗 References & Acknowledgement
### 📄 Assignment
This project was developed for **CAD Programming Assignment 2** (Static Timing Analysis).

### 📚 Algorithms
The implementation applies standard STA techniques:
* **Graph Theory**: Directed Acyclic Graph (DAG) traversal.
* **Topological Sort**: Ensuring signals are processed in causal order.
* **NLDM Interpolation**: Bilinear interpolation for table lookup.

---

## ⚠️ License & Academic Integrity

This project is released for **educational and portfolio purposes only**.

* **Copyright**: The implementation code is © 2026 Bo-An Li. All rights reserved.
* **Course Material**: The problem statement, benchmarks, and library files belong to the course administration.
* **Warning**: If you are a student currently taking this course, **do not copy or plagiarize this code**. Academic dishonesty may lead to severe consequences.