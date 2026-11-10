# Linux Process Monitor

A Linux process monitor written in C that reads process and system information directly from the `/proc` filesystem. It shows a live process table (PID, PPID, user, state, CPU%, memory%, name) and a system summary (CPU%, memory, uptime, load averages), with periodic refresh, sorting and filtering.

ICS 433 Lab Project, Phase 2. Team: Afrah Alsubhi, Mashael Alghamdi, Fatemah Almarhoon, Alshayma Alarfaj.

## Project Structure

```text
Linux-Process-Monitor/
├── include/             # Header files
├── src/
│   ├── collector.c      # Task 1: reads process and system files from /proc
│   ├── parser.c         # Task 1: parses the raw /proc text
│   ├── metrics.c        # Task 2: CPU% and memory% calculations
│   ├── filter_sort.c    # Task 3: filtering and sorting
│   ├── ui.c             # Task 3: terminal display and keyboard input
│   ├── refresh.c        # Task 4: refresh loop
│   ├── error_handler.c  # Task 4: error reporting
│   └── main.c           # Connects all modules
├── tests/               # Test files for each task
├── Makefile
└── README.md
```

## Requirements

Linux (tested on Ubuntu), GCC and make.

## How to Build and Run

1. Clone the repo or download the contents as a zip.
2. `cd` into the project directory.
3. Build the monitor: `make`
4. Run it: `./procmon`

Options:

* `./procmon -d 1` refreshes every 1 second (allowed range 0.1 to 60, default 2).
* `./procmon -n 5` quits after 5 refreshes.

## Keyboard Commands

| Key | Action |
|---|---|
| `p` | Sort by PID |
| `c` | Sort by CPU% |
| `m` | Sort by memory% |
| `r` | Reverse the sort direction |
| `n` | Filter by process name |
| `u` | Filter by user or UID |
| `s` | Filter by state (for example `R`, `S`, `Z`) |
| `x` | Clear all filters |
| Arrow keys | Scroll |
| `q` or `Ctrl+C` | Quit |

In a filter prompt, press Enter to apply or Esc to cancel.

## Running the Tests

```bash
make test
```

To remove the built files, run `make clean`.

## Notes

* The first refresh shows CPU% as N/A because CPU usage needs two samples.
* Values that cannot be read (for example memory of kernel threads) are shown as N/A.