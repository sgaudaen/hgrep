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
#define EDIT_BUF_LEN 2048

typedef struct {
    char **lines;
    int count;
    int selected;
    int scroll_offset;
    int editing;
    char edit_buf[EDIT_BUF_LEN];
    int edit_cursor;
} History;

History history = {NULL, 0, 0, 0, 0, "", 0};

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
    if (history.editing) {
        mvprintw(0, 0, "hgrep - Editing Command (ESC: Cancel | Enter: Execute)");
    } else {
        mvprintw(0, 0, "hgrep - History Search");
    }
    attroff(A_BOLD);
    
    // Draw filter line
    mvprintw(1, 0, "Filter: %s", filter ? filter : "");
    mvprintw(2, 0, "%.*s", max_x, "================================================================================");

    // Draw history lines (oldest at top, newest at bottom)
    int display_start = 3;
    int max_display = max_y - 5;
    
    // Adjust scroll_offset so selected item is visible
    if (history.count > 0) {
        if (history.selected < history.scroll_offset) {
            history.scroll_offset = history.selected;
        }
        if (history.selected >= history.scroll_offset + max_display) {
            history.scroll_offset = history.selected - max_display + 1;
        }
        
        if (history.scroll_offset < 0) {
            history.scroll_offset = 0;
        }
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
            if (history.editing) {
                // Show edited buffer on the selected line
                mvprintw(display_start + i, 0, "%.*s", max_x - 1, history.edit_buf);
            } else {
                mvprintw(display_start + i, 0, "%.*s", max_x - 1, history.lines[hist_idx]);
            }
            attroff(A_REVERSE);
            
            // Draw cursor after reverse video is off so it's visible
            if (history.editing && history.edit_cursor <= (int)strlen(history.edit_buf)) {
                attron(A_BOLD | A_UNDERLINE);
                mvaddch(display_start + i, history.edit_cursor, '_');
                attroff(A_BOLD | A_UNDERLINE);
            }
        } else {
            mvprintw(display_start + i, 0, "%.*s", max_x - 1, history.lines[hist_idx]);
        }
    }

    // Draw footer
    if (history.editing) {
        mvprintw(max_y - 1, 0, "Left/Right: Move | Home/End: Jump | Backspace/Del: Delete | ESC: Cancel | Enter: Execute");
    } else {
        mvprintw(max_y - 1, 0, "Up/Down: Navigate | i: Edit | q/Ctrl+C: Quit | Enter: Execute");
    }
    
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

void start_editing() {
    if (history.count > 0 && history.selected >= 0) {
        // Clear the edit buffer
        memset(history.edit_buf, 0, EDIT_BUF_LEN);
        
        char *cmd = extract_command(history.lines[history.selected]);
        strncpy(history.edit_buf, cmd, EDIT_BUF_LEN - 1);
        history.edit_buf[EDIT_BUF_LEN - 1] = '\0';
        history.edit_cursor = strlen(history.edit_buf);
        history.editing = 1;
        free(cmd);
    }
}

void handle_edit_input(int ch) {
    if (ch == 27) {  // ESC key
        history.editing = 0;
        return;
    }
    
    if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
        // Execute edited command
        history.editing = 0;
        endwin();
        system("stty sane");
        printf("\nExecuting: %s\n\n", history.edit_buf);
        fflush(stdout);
        system(history.edit_buf);
        exit(0);
    }
    
    if (ch == KEY_LEFT) {
        if (history.edit_cursor > 0) {
            history.edit_cursor--;
        }
        return;
    }
    
    if (ch == KEY_RIGHT) {
        if (history.edit_cursor < (int)strlen(history.edit_buf)) {
            history.edit_cursor++;
        }
        return;
    }
    
    if (ch == KEY_HOME) {
        history.edit_cursor = 0;
        return;
    }
    
    if (ch == KEY_END) {
        history.edit_cursor = strlen(history.edit_buf);
        return;
    }
    
    if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
        if (history.edit_cursor > 0) {
            // Shift characters left
            for (int i = history.edit_cursor - 1; i < (int)strlen(history.edit_buf); i++) {
                history.edit_buf[i] = history.edit_buf[i + 1];
            }
            history.edit_cursor--;
        }
        return;
    }
    
    if (ch == KEY_DC) {  // Delete key
        if (history.edit_cursor < (int)strlen(history.edit_buf)) {
            // Shift characters left
            for (int i = history.edit_cursor; i < (int)strlen(history.edit_buf); i++) {
                history.edit_buf[i] = history.edit_buf[i + 1];
            }
        }
        return;
    }
    
    // Handle printable characters
    if (ch >= 32 && ch < 127) {
        if (history.edit_cursor < EDIT_BUF_LEN - 1) {
            // Get length before modification
            int len = strlen(history.edit_buf);
            
            // Shift characters right to make room
            for (int i = len; i > history.edit_cursor; i--) {
                history.edit_buf[i] = history.edit_buf[i - 1];
            }
            
            // Insert the new character
            history.edit_buf[history.edit_cursor] = ch;
            history.edit_cursor++;
            
            // Null terminate at the new end of string (old length + 1)
            history.edit_buf[len + 1] = '\0';
        }
    }
}

void print_help() {
    printf("hgrep - Interactive History Grep\n");
    printf("\n");
    printf("USAGE:\n");
    printf("  hgrep [PATTERN]\n");
    printf("\n");
    printf("DESCRIPTION:\n");
    printf("  Search bash history interactively and execute commands with optional editing.\n");
    printf("\n");
    printf("OPTIONS:\n");
    printf("  -h, --help      Show this help message and exit\n");
    printf("  PATTERN         Optional grep pattern to filter history on startup\n");
    printf("\n");
    printf("KEYBOARD SHORTCUTS:\n");
    printf("\n");
    printf("Normal Mode:\n");
    printf("  Up/Down         Navigate through history (Up = older, Down = newer)\n");
    printf("  i               Enter edit mode to modify the selected command\n");
    printf("  Enter           Execute the selected command\n");
    printf("  q, Q            Quit the program\n");
    printf("  Ctrl+C          Quit the program\n");
    printf("\n");
    printf("Edit Mode:\n");
    printf("  Left/Right      Move cursor left/right\n");
    printf("  Home/End        Jump to beginning/end of line\n");
    printf("  Backspace/Del   Delete characters\n");
    printf("  Type            Insert characters at cursor\n");
    printf("  Enter           Execute the edited command\n");
    printf("  ESC             Cancel editing and return to normal mode\n");
    printf("\n");
    printf("EXAMPLES:\n");
    printf("  hgrep                    # Show all history\n");
    printf("  hgrep docker             # Show only docker commands\n");
    printf("  hgrep 'git commit'       # Show only git commit commands\n");
    printf("\n");
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

    // Check for help flag in any argument position
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        }
    }

    // First non-flag argument is the filter
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

        if (history.editing) {
            handle_edit_input(ch);
        } else {
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
                case 'i':
                case 'I':
                    start_editing();
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
    }

    endwin();
    cleanup_history();
    return 0;
}
