#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_LINES 10000
#define MAX_LINE_LEN 1024

typedef struct {
    char **lines;
    int count;
    int selected;
    int scroll_offset;
} History;

History history = {NULL, 0, 0, 0};

void cleanup_history() {
    if (history.lines) {
        for (int i = 0; i < history.count; i++) {
            free(history.lines[i]);
        }
        free(history.lines);
    }
}

void load_history_with_grep(const char *filter) {
    FILE *fp;
    char line[MAX_LINE_LEN];
    char history_path[512];
    const char *home = getenv("HOME");

    cleanup_history();
    history.lines = malloc(sizeof(char *) * MAX_LINES);
    history.count = 0;
    history.selected = -1;
    history.scroll_offset = 0;

    if (!home) {
        home = "/root";
    }

    snprintf(history_path, sizeof(history_path), "%s/.bash_history", home);
    fp = fopen(history_path, "r");
    
    if (fp == NULL) {
        return;
    }

    int total_lines = 0;
    // First pass: count total lines
    while (fgets(line, sizeof(line), fp) != NULL) {
        total_lines++;
    }
    rewind(fp);

    int current_line = 0;
    // Second pass: read and display
    while (fgets(line, sizeof(line), fp) != NULL && history.count < MAX_LINES) {
        current_line++;
        line[strcspn(line, "\n")] = 0;
        
        // Apply filter if provided
        if (filter && strlen(filter) > 0) {
            if (strstr(line, filter) == NULL) {
                continue;
            }
        }
        
        // Create formatted line with line number starting from HISTSIZE offset
        // Typically history starts at 1 and increments
        char formatted_line[MAX_LINE_LEN + 32];
        snprintf(formatted_line, sizeof(formatted_line), "%5d  %s", current_line, line);
        
        history.lines[history.count] = malloc(strlen(formatted_line) + 1);
        strcpy(history.lines[history.count], formatted_line);
        history.count++;
    }

    fclose(fp);
    
    // Start selection at the last (most recent) command
    if (history.count > 0) {
        history.selected = history.count - 1;
    }
}

void draw_ui(const char *filter) {
    clear();
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // Draw header
    attron(A_BOLD);
    mvprintw(0, 0, "hgrep - History Search");
    attroff(A_BOLD);
    
    // Draw filter line
    mvprintw(1, 0, "Filter: %s", filter ? filter : "");
    mvprintw(2, 0, "%.*s", max_x, "================================================================================");

    // Draw history lines (oldest at top, newest at bottom)
    int display_start = 3;
    int max_display = max_y - 5;
    
    // Adjust scroll_offset so selected item is visible
    if (history.count > 0) {
        // If selected is above scroll_offset, scroll up to show it
        if (history.selected < history.scroll_offset) {
            history.scroll_offset = history.selected;
        }
        // If selected is below the viewport, scroll down to show it
        if (history.selected >= history.scroll_offset + max_display) {
            history.scroll_offset = history.selected - max_display + 1;
        }
        
        // Make sure scroll_offset doesn't go negative
        if (history.scroll_offset < 0) {
            history.scroll_offset = 0;
        }
        // Make sure we don't scroll past the end
        if (history.scroll_offset + max_display > history.count) {
            history.scroll_offset = history.count - max_display;
            if (history.scroll_offset < 0) {
                history.scroll_offset = 0;
            }
        }
    }

    for (int i = 0; i < max_display && (history.scroll_offset + i) < history.count; i++) {
        int hist_idx = history.scroll_offset + i;
        if (hist_idx == history.selected) {
            attron(A_REVERSE);
            mvprintw(display_start + i, 0, "%.*s", max_x - 1, history.lines[hist_idx]);
            attroff(A_REVERSE);
        } else {
            mvprintw(display_start + i, 0, "%.*s", max_x - 1, history.lines[hist_idx]);
        }
    }

    // Draw footer
    mvprintw(max_y - 1, 0, "↑: Older | ↓: Newer | Enter: Execute | Ctrl+C: Quit");
    
    refresh();
}

char* extract_command(const char *history_line) {
    // History format is typically: "  123  command"
    // Skip leading spaces and line number
    const char *p = history_line;
    
    // Skip leading whitespace
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    
    // Skip line number (digits)
    while (*p >= '0' && *p <= '9') {
        p++;
    }
    
    // Skip whitespace after line number
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    
    char *cmd = malloc(strlen(p) + 1);
    strcpy(cmd, p);
    return cmd;
}

void execute_command(const char *history_line) {
    char *cmd = extract_command(history_line);
    
    // Properly close ncurses
    endwin();
    
    // Reset terminal to normal state
    system("stty sane");
    
    printf("\nExecuting: %s\n\n", cmd);
    fflush(stdout);
    
    // Run the command
    system(cmd);
    
    free(cmd);
    
    // Exit the program after running the command
    exit(0);
}

int main(int argc, char *argv[]) {
    char filter[256] = "";
    int ch;

    if (argc > 1) {
        strncpy(filter, argv[1], sizeof(filter) - 1);
        filter[sizeof(filter) - 1] = '\0';
    }

    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    // Set up terminal
    set_escdelay(0);

    load_history_with_grep(filter);

    while (1) {
        draw_ui(filter);
        ch = getch();

        switch (ch) {
            case KEY_UP:
                if (history.selected > 0) {
                    history.selected--;
                }
                break;
            case KEY_DOWN:
                if (history.selected < history.count - 1) {
                    history.selected++;
                }
                break;
            case '\n':
            case '\r':
            case KEY_ENTER:
                if (history.count > 0 && history.selected >= 0) {
                    execute_command(history.lines[history.selected]);
                    load_history_with_grep(filter);
                }
                break;
            case 'q':
            case 'Q':
            case 3:  // Ctrl+C
                endwin();
                cleanup_history();
                exit(0);
                break;
        }
    }

    endwin();
    cleanup_history();
    return 0;
}
