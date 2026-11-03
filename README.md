# Linux Process Monitor Using /proc

University operating systems project. A Linux-only, read-only process monitor written in **C**.

## Requirements
- Linux (Ubuntu recommended)
- GCC and make (`sudo apt install build-essential`)
- Optional later: ncurses development library (`sudo apt install libncurses-dev`)

## Build and run
```bash
make
./build/monitor
```
The initial executable is a placeholder until the modules are integrated.

## Modules
1. **Collector & Parser:** scan `/proc`, read and parse process and system records.
2. **Metrics:** calculate process CPU share and memory percentages from sampled values.
3. **UI & Filter/Sort:** display records and allow filtering/sorting.
4. **Refresh & Error Handling:** refresh timing, nonfatal error reporting, testing.

## Team conventions
- Shared contracts live in `include/`; agree on changes before editing them.
- Each member works in a separate branch and opens a pull request.
- Implement and test module functions independently with mock data.
- A missing or inaccessible process should not crash the monitor.
- Do not modify or control Linux processes; read `/proc` only.

## Phase 2
Target: at least 50-70% of the system implemented in C, with a PDF progress report, screenshots, organized code, and tests.
