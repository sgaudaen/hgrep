#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
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

// Forward declaration
void execute_in_interactive_shell(const char *cmd);


void cleanup_history() {
    if (history.lines) {
        for (int i = 0; i < history.count; i++) {
            free(history.lines[i]);
        }
        free(history.lines);
    }
}

void load_history_with_grep(const char *grep_args) {
    FILE *fp;
    char line[MAX_LINE_LEN];
    char history_path[512];
    char cmd[2048];
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
    
    // Build grep command with line numbers
    // If no args provided, just cat the file with line numbers
    if (!grep_args || strlen(grep_args) == 0) {
        snprintf(cmd, sizeof(cmd), "cat -n %s", history_path);
    } else {
        // Use grep with user-provided arguments and line numbers
        snprintf(cmd, sizeof(cmd), "grep -n %s %s", grep_args, history_path);
    }

    fp = popen(cmd, "r");
    if (fp == NULL) {
        return;
    }

    // Read output from grep/cat
    while (fgets(line, sizeof(line), fp) != NULL && history.count < MAX_LINES) {
        line[strcspn(line, "\n")] = 0;
        
        // Store the line (grep -n format includes line number)
        history.lines[history.count] = malloc(strlen(line) + 1);
        strcpy(history.lines[history.count], line);
        history.count++;
    }

    pclose(fp);
    
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
    // History format from grep -n: "123:command" or from cat -n: "  123  command"
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
    
    // Skip the colon separator (from grep -n) or whitespace (from cat -n)
    if (*p == ':') {
        p++;
    } else {
        // Skip whitespace after line number (from cat -n)
        while (*p == ' ' || *p == '\t') {
            p++;
        }
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
        execute_in_interactive_shell(history.edit_buf);
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
    printf("  hgrep [GREP_ARGS]\n");
    printf("\n");
    printf("DESCRIPTION:\n");
    printf("  Search bash history interactively and execute commands with optional editing.\n");
    printf("  Supports all grep options for powerful filtering.\n");
    printf("\n");
    printf("GREP_ARGS:\n");
    printf("  Pass any valid grep arguments to filter history:\n");
    printf("  -i              Case-insensitive search\n");
    printf("  -v              Invert match (show commands NOT matching pattern)\n");
    printf("  -E              Use extended regex\n");
    printf("  -e PATTERN      Specify pattern\n");
    printf("  No args         Show all history\n");
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
    printf("  hgrep -i DOCKER          # Case-insensitive docker search\n");
    printf("  hgrep -E 'git|svn'       # Show git or svn commands (regex)\n");
    printf("  hgrep -v '#'             # Exclude comments\n");
    printf("\n");
}

// Execute a command in an interactive bash shell to ensure it's added to history
void execute_in_interactive_shell(const char *cmd) {
    const char *home = getenv("HOME");
    if (!home) {
        home = "/root";
    }
    
    char history_file[512];
    snprintf(history_file, sizeof(history_file), "%s/.bash_history", home);
    
    // Append the command to bash history file
    FILE *hist = fopen(history_file, "a");
    if (hist != NULL) {
        fprintf(hist, "%s\n", cmd);
        fclose(hist);
    }
    
    // Execute the command in the current shell
    system(cmd);
}

void execute_command(const char *history_line) {
    char *cmd = extract_command(history_line);
    
    // Properly close ncurses
    endwin();
    
    // Reset terminal to normal state
    system("stty sane");
    
    printf("\nExecuting: %s\n\n", cmd);
    fflush(stdout);
    
    // Run the command in an interactive bash shell so it's added to history
    execute_in_interactive_shell(cmd);
    
    free(cmd);
    
    // Exit the program after running the command
    exit(0);
}

int main(int argc, char *argv[]) {
    char grep_args[1024] = "";
    int ch;

    // Check for help flag in any argument position
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        }
    }

    // Collect all non-help arguments into grep_args string
    if (argc > 1) {
        int pos = 0;
        for (int i = 1; i < argc && pos < (int)sizeof(grep_args) - 1; i++) {
            int arg_len = strlen(argv[i]);
            // Check if argument needs quoting (contains spaces or special chars)
            int needs_quote = 0;
            for (int j = 0; j < arg_len; j++) {
                if (argv[i][j] == ' ' || argv[i][j] == '|' || argv[i][j] == '*') {
                    needs_quote = 1;
                    break;
                }
            }
            
            if (pos > 0 && pos < (int)sizeof(grep_args) - 1) {
                grep_args[pos++] = ' ';
            }
            
            if (needs_quote && pos < (int)sizeof(grep_args) - 1) {
                grep_args[pos++] = '\'';
            }
            
            // Copy argument
            for (int j = 0; j < arg_len && pos < (int)sizeof(grep_args) - 2; j++) {
                grep_args[pos++] = argv[i][j];
            }
            
            if (needs_quote && pos < (int)sizeof(grep_args) - 1) {
                grep_args[pos++] = '\'';
            }
        }
        grep_args[pos] = '\0';
    }

    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    // Set up terminal
    set_escdelay(0);

    load_history_with_grep(grep_args);

    while (1) {
        draw_ui(grep_args);
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
                        load_history_with_grep(grep_args);
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
