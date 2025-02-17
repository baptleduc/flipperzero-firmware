#include "cli_shell_line.h"

#define HISTORY_DEPTH 10

ARRAY_DEF(ShellHistory, FuriString*, FURI_STRING_OPLIST); // -V524
#define M_OPL_ShellHistory_t() ARRAY_OPLIST(ShellHistory)

struct CliShellLine {
    size_t history_position;
    size_t line_position;
    ShellHistory_t history;
    CliShell* shell;
};

// ==========
// Public API
// ==========

CliShellLine* cli_shell_line_alloc(CliShell* shell) {
    CliShellLine* line = malloc(sizeof(CliShellLine));
    line->shell = shell;

    ShellHistory_init(line->history);
    FuriString* new_command = furi_string_alloc();
    ShellHistory_push_at(line->history, 0, new_command);
    furi_string_free(new_command);

    return line;
}

void cli_shell_line_free(CliShellLine* line) {
    ShellHistory_clear(line->history);
    free(line);
}

FuriString* cli_shell_line_get_selected(CliShellLine* line) {
    return *ShellHistory_cget(line->history, line->history_position);
}

FuriString* cli_shell_line_get_editing(CliShellLine* line) {
    return *ShellHistory_front(line->history);
}

size_t cli_shell_line_prompt_length(CliShellLine* line) {
    UNUSED(line);
    return strlen(">: ");
}

void cli_shell_line_format_prompt(CliShellLine* line, char* buf, size_t length) {
    UNUSED(line);
    snprintf(buf, length - 1, ">: ");
}

void cli_shell_line_prompt(CliShellLine* line) {
    char buffer[128];
    cli_shell_line_format_prompt(line, buffer, sizeof(buffer));
    printf("\r\n%s", buffer);
    fflush(stdout);
}

void cli_shell_line_ensure_not_overwriting_history(CliShellLine* line) {
    if(line->history_position > 0) {
        FuriString* source = cli_shell_line_get_selected(line);
        FuriString* destination = cli_shell_line_get_editing(line);
        furi_string_set(destination, source);
        line->history_position = 0;
    }
}

size_t cli_shell_line_get_line_position(CliShellLine* line) {
    return line->line_position;
}

void cli_shell_line_set_line_position(CliShellLine* line, size_t position) {
    line->line_position = position;
}

// =======
// Helpers
// =======

typedef enum {
    CliCharClassWord,
    CliCharClassSpace,
    CliCharClassOther,
} CliCharClass;

typedef enum {
    CliSkipDirectionLeft,
    CliSkipDirectionRight,
} CliSkipDirection;

CliCharClass cli_shell_line_char_class(char c) {
    if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
        return CliCharClassWord;
    } else if(c == ' ') {
        return CliCharClassSpace;
    } else {
        return CliCharClassOther;
    }
}

size_t
    cli_shell_line_skip_run(FuriString* string, size_t original_pos, CliSkipDirection direction) {
    if(furi_string_size(string) == 0) return original_pos;
    if(direction == CliSkipDirectionLeft && original_pos == 0) return original_pos;
    if(direction == CliSkipDirectionRight && original_pos == furi_string_size(string))
        return original_pos;

    int8_t look_offset = (direction == CliSkipDirectionLeft) ? -1 : 0;
    int8_t increment = (direction == CliSkipDirectionLeft) ? -1 : 1;
    int32_t position = original_pos;
    CliCharClass start_class =
        cli_shell_line_char_class(furi_string_get_char(string, position + look_offset));

    while(true) {
        position += increment;
        if(position < 0) break;
        if(position >= (int32_t)furi_string_size(string)) break;
        if(cli_shell_line_char_class(furi_string_get_char(string, position + look_offset)) !=
           start_class)
            break;
    }

    return MAX(0, position);
}

// ==============
// Input handlers
// ==============

static bool key_combo_ctrl_c(CliKeyCombo combo, void* context) {
    UNUSED(combo);
    CliShellLine* line = context;
    // reset input
    furi_string_reset(cli_shell_line_get_editing(line));
    line->line_position = 0;
    line->history_position = 0;
    printf("^C");
    cli_shell_line_prompt(line);
    return true;
}

static bool key_combo_cr(CliKeyCombo combo, void* context) {
    UNUSED(combo);
    CliShellLine* line = context;
    // get command and update history
    FuriString* command = furi_string_alloc();
    ShellHistory_pop_at(&command, line->history, line->history_position);
    furi_string_trim(command);
    if(line->history_position > 0) ShellHistory_pop_at(NULL, line->history, 0);
    if(!furi_string_empty(command)) ShellHistory_push_at(line->history, 0, command);
    FuriString* new_command = furi_string_alloc();
    ShellHistory_push_at(line->history, 0, new_command);
    furi_string_free(new_command);
    if(ShellHistory_size(line->history) > HISTORY_DEPTH) {
        ShellHistory_pop_back(NULL, line->history);
    }

    // execute command
    line->line_position = 0;
    line->history_position = 0;
    printf("\r\n");
    if(!furi_string_empty(command)) cli_shell_execute_command(line->shell, command);
    furi_string_free(command);
    cli_shell_line_prompt(line);
    return true;
}

static bool key_combo_up_down(CliKeyCombo combo, void* context) {
    CliShellLine* line = context;
    // go up and down in history
    int increment = (combo.key == CliKeyUp) ? 1 : -1;
    size_t new_pos = CLAMP(
        (int)line->history_position + increment, (int)ShellHistory_size(line->history) - 1, 0);

    // print prompt with selected command
    if(new_pos != line->history_position) {
        line->history_position = new_pos;
        FuriString* command = cli_shell_line_get_selected(line);
        printf(
            ANSI_CURSOR_HOR_POS("1") ">: %s" ANSI_ERASE_LINE(ANSI_ERASE_FROM_CURSOR_TO_END),
            furi_string_get_cstr(command));
        fflush(stdout);
        line->line_position = furi_string_size(command);
    }
    return true;
}

static bool key_combo_left_right(CliKeyCombo combo, void* context) {
    CliShellLine* line = context;
    // go left and right in the current line
    FuriString* command = cli_shell_line_get_selected(line);
    int increment = (combo.key == CliKeyRight) ? 1 : -1;
    size_t new_pos =
        CLAMP((int)line->line_position + increment, (int)furi_string_size(command), 0);

    // move cursor
    if(new_pos != line->line_position) {
        line->line_position = new_pos;
        printf("%s", (increment == 1) ? ANSI_CURSOR_RIGHT_BY("1") : ANSI_CURSOR_LEFT_BY("1"));
        fflush(stdout);
    }
    return true;
}

static bool key_combo_home(CliKeyCombo combo, void* context) {
    UNUSED(combo);
    CliShellLine* line = context;
    // go to the start
    line->line_position = 0;
    printf(ANSI_CURSOR_HOR_POS("%zu"), cli_shell_line_prompt_length(line) + 1);
    fflush(stdout);
    return true;
}

static bool key_combo_end(CliKeyCombo combo, void* context) {
    UNUSED(combo);
    CliShellLine* line = context;
    // go to the end
    line->line_position = furi_string_size(cli_shell_line_get_selected(line));
    printf(
        ANSI_CURSOR_HOR_POS("%zu"), cli_shell_line_prompt_length(line) + line->line_position + 1);
    fflush(stdout);
    return true;
}

static bool key_combo_bksp(CliKeyCombo combo, void* context) {
    UNUSED(combo);
    CliShellLine* line = context;
    // erase one character
    cli_shell_line_ensure_not_overwriting_history(line);
    FuriString* editing_line = cli_shell_line_get_editing(line);
    if(line->line_position == 0) {
        putc(CliKeyBell, stdout);
        fflush(stdout);
        return true;
    }
    line->line_position--;
    furi_string_replace_at(editing_line, line->line_position, 1, "");

    // move cursor, print the rest of the line, restore cursor
    printf(
        ANSI_CURSOR_LEFT_BY("1") "%s" ANSI_ERASE_LINE(ANSI_ERASE_FROM_CURSOR_TO_END),
        furi_string_get_cstr(editing_line) + line->line_position);
    size_t left_by = furi_string_size(editing_line) - line->line_position;
    if(left_by) // apparently LEFT_BY("0") still shifts left by one ._ .
        printf(ANSI_CURSOR_LEFT_BY("%zu"), left_by);
    fflush(stdout);
    return true;
}

static bool key_combo_ctrl_l(CliKeyCombo combo, void* context) {
    UNUSED(combo);
    CliShellLine* line = context;
    // clear screen
    FuriString* command = cli_shell_line_get_selected(line);
    char prompt[64];
    cli_shell_line_format_prompt(line, prompt, sizeof(prompt));
    printf(
        ANSI_ERASE_DISPLAY(ANSI_ERASE_ENTIRE) ANSI_ERASE_SCROLLBACK_BUFFER ANSI_CURSOR_POS(
            "1", "1") "%s%s" ANSI_CURSOR_HOR_POS("%zu"),
        prompt,
        furi_string_get_cstr(command),
        strlen(prompt) + line->line_position + 1 /* 1-based column indexing */);
    fflush(stdout);
    return true;
}

static bool key_combo_ctrl_left_right(CliKeyCombo combo, void* context) {
    CliShellLine* line = context;
    // skip run of similar chars to the left or right
    FuriString* selected_line = cli_shell_line_get_selected(line);
    CliSkipDirection direction = (combo.key == CliKeyLeft) ? CliSkipDirectionLeft :
                                                             CliSkipDirectionRight;
    line->line_position = cli_shell_line_skip_run(selected_line, line->line_position, direction);
    printf(
        ANSI_CURSOR_HOR_POS("%zu"), cli_shell_line_prompt_length(line) + line->line_position + 1);
    fflush(stdout);
    return true;
}

static bool key_combo_ctrl_bksp(CliKeyCombo combo, void* context) {
    UNUSED(combo);
    CliShellLine* line = context;
    // delete run of similar chars to the left
    cli_shell_line_ensure_not_overwriting_history(line);
    FuriString* selected_line = cli_shell_line_get_selected(line);
    size_t run_start =
        cli_shell_line_skip_run(selected_line, line->line_position, CliSkipDirectionLeft);
    furi_string_replace_at(selected_line, run_start, line->line_position - run_start, "");
    line->line_position = run_start;
    printf(
        ANSI_CURSOR_HOR_POS("%zu") "%s" ANSI_ERASE_LINE(ANSI_ERASE_FROM_CURSOR_TO_END)
            ANSI_CURSOR_HOR_POS("%zu"),
        cli_shell_line_prompt_length(line) + line->line_position + 1,
        furi_string_get_cstr(selected_line) + run_start,
        cli_shell_line_prompt_length(line) + run_start + 1);
    fflush(stdout);
    return true;
}

static bool normal_input(CliKeyCombo combo, void* context) {
    CliShellLine* line = context;
    if(combo.modifiers != CliModKeyNo) return false;
    if(combo.key < CliKeySpace || combo.key >= CliKeyDEL) return false;
    // insert character
    cli_shell_line_ensure_not_overwriting_history(line);
    FuriString* editing_line = cli_shell_line_get_editing(line);
    if(line->line_position == furi_string_size(editing_line)) {
        furi_string_push_back(editing_line, combo.key);
        printf("%c", combo.key);
    } else {
        const char in_str[2] = {combo.key, 0};
        furi_string_replace_at(editing_line, line->line_position, 0, in_str);
        printf(ANSI_INSERT_MODE_ENABLE "%c" ANSI_INSERT_MODE_DISABLE, combo.key);
    }
    fflush(stdout);
    line->line_position++;
    return true;
}

CliShellKeyComboSet cli_shell_line_key_combo_set = {
    .fallback = normal_input,
    .count = 14,
    .records =
        {
            {{CliModKeyNo, CliKeyETX}, key_combo_ctrl_c},
            {{CliModKeyNo, CliKeyCR}, key_combo_cr},
            {{CliModKeyNo, CliKeyUp}, key_combo_up_down},
            {{CliModKeyNo, CliKeyDown}, key_combo_up_down},
            {{CliModKeyNo, CliKeyLeft}, key_combo_left_right},
            {{CliModKeyNo, CliKeyRight}, key_combo_left_right},
            {{CliModKeyNo, CliKeyHome}, key_combo_home},
            {{CliModKeyNo, CliKeyEnd}, key_combo_end},
            {{CliModKeyNo, CliKeyBackspace}, key_combo_bksp},
            {{CliModKeyNo, CliKeyDEL}, key_combo_bksp},
            {{CliModKeyNo, CliKeyFF}, key_combo_ctrl_l},
            {{CliModKeyCtrl, CliKeyLeft}, key_combo_ctrl_left_right},
            {{CliModKeyCtrl, CliKeyRight}, key_combo_ctrl_left_right},
            {{CliModKeyNo, CliKeyETB}, key_combo_ctrl_bksp},
        },
};
