# Linux Process Monitor

A C-based Linux process monitor that reads process information from `/proc`.

## Project Structure

```text
Linux-Process-Monitor/
├── include/          # Header files
├── src/              # C source files (including main.c)
├── tests/            # Test files
├── docs/
│   └── screenshots/  # Project screenshots
└── README.md
```

All `.c` and `.h` files are empty placeholders for implementation.

## How to use
1. Clone the repo or download the contents in a zip
2. `cd` into the directory of this project in your PC
3. Run the command: `make`
4. Run the executable by entering the command: `./procmon`
5. Press `c` to sort by CPU demand or `m` to sort by memory command
6. Press `q` to exit or press `CTRL+Q`
