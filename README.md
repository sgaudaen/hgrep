# hgrep - Interactive History Grep

An interactive terminal UI for searching and executing bash history commands.

## Features

- **Search History**: Filter bash history using grep patterns
- **Arrow Key Navigation**: Use ↑/↓ arrow keys to navigate through results
- **Quick Execution**: Press Enter to execute the selected command
- **Terminal UI**: Built with ncurses for a smooth terminal experience

## Requirements

- GCC compiler
- ncurses development libraries

### Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get install build-essential libncurses-dev
```

**Fedora/RHEL:**
```bash
sudo dnf install gcc libncurses-devel
```

**macOS:**
```bash
brew install ncurses
```

## Building

```bash
make
```

## Installation

Install to `~/.local/bin/`:
```bash
make install
```

Or use the binary directly from the source directory:
```bash
./hgrep
```

## Usage

### Basic Usage
Show all history and navigate with arrow keys:
```bash
hgrep
```

### Search with Filter
Pass a search pattern to filter history on startup:
```bash
hgrep "grep"
hgrep "docker"
hgrep "git commit"
```

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| ↑ | Move up in history list |
| ↓ | Move down in history list |
| Enter | Execute selected command |
| q / Q | Quit (also Ctrl+C) |

## How It Works

1. Launch `hgrep` with an optional search pattern
2. The program runs `history | grep <pattern>` (or just `history` if no pattern)
3. Results are displayed in an interactive ncurses UI
4. Navigate with arrow keys
5. Press Enter to execute the selected command
6. After execution completes, you're returned to the search interface

## Examples

```bash
# Search for all git commands in history
hgrep git

# Search for docker commands
hgrep docker

# Search for failed commands (containing 'error')
hgrep error

# View all history
hgrep
```

## Cleaning Up

Remove built files:
```bash
make clean
```

Uninstall from `~/.local/bin/`:
```bash
make uninstall
```

## License

MIT
