# Linux Process Monitor

A Linux process monitor written in C that reads process and system information directly from the `/proc` filesystem. It shows a live process table (PID, PPID, user, state, CPU%, memory%, name) and a system summary (CPU%, memory, uptime, load averages), with periodic refresh, sorting and filtering.

procmon is written in C using plain terminal escape codes and `termios`.

ICS 433 Lab Project. Team: Afrah Alsubhi, Mashael Alghamdi, Fatemah Almarhoon, Alshayma Alarfaj.

## Project Structure

```text
Linux-Process-Monitor/
├── include/             # Header files (process.h defines the shared data structures)
├── src/
│   ├── collector.c      # Task 1: reads process and system files from /proc
│   ├── parser.c         # Task 1: parses the raw /proc text
│   ├── metrics.c        # Task 2: CPU% and memory% calculations
│   ├── filter_sort.c    # Task 3: filtering and sorting
│   ├── ui.c             # Task 3: terminal display and keyboard input
│   ├── refresh.c        # Task 4: refresh loop and clean shutdown
│   ├── error_handler.c  # Task 4: error codes and input checks
│   └── main.c           # Connects all modules
├── tests/               # One test program per task
├── scripts/             # Load generators and test helpers
├── docs/                # Design, test plan, test results, demo script, diagram, screenshots
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
5. Remove the built files: `make clean`

Options:

* `./procmon -d <SECONDS>` refreshes every <SECONDS> seconds (allowed range 0.1 to 60, default 2).
* `./procmon -n <COUNT>` quits after <COUNT> refreshes.
* `-b` outputs plaintext instead of the full-screen view (useful for logs and tests)

Note that when procmon runs outside a terminal, it resorts to plain text mode.

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
| `f` | Switch NAME column between the process name and the full command line
| Arrow keys | Scroll |
| `q` or `Ctrl+C` | Quit |

In a filter prompt, press Enter to apply or Esc to cancel.

## Running the Tests

```bash
make test
```

To remove the built files, run `make clean`.

## Load Scripts

The scripts in `scripts/` set up the environments for the tests and for the demo. Run them in a new terminal window while `./procmon` is running.

## Notes and Limitations

* The first refresh shows CPU% as N/A because CPU usage needs two samples.
* CPU% considers all the cores as 100% (CPU using one core out of a four-core computer is 25%).
  `top` considers one core as 100%, so the `top` output should be divided by the number of cores (`nproc`) to match.
* Values that cannot be read (for example, memory of kernel threads) are shown as N/A.
* Processes with very short life-spans are likely to begin and terminate between two samples and thus not appear.
* A maximum of 2048 processes are displayed; if the list gets full, an appropriate message appears on the screen.
* Procmon ignores processes which it does not have access to read (for example, with the use of `hidepid`).
