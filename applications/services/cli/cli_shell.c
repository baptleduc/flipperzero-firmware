#include "cli_shell.h"
#include "cli_ansi.h"
#include "cli_i.h"
#include "cli_commands.h"
#include <stdio.h>
#include <furi_hal_version.h>
#include <m-array.h>
#include <loader/loader.h>
#include <toolbox/pipe.h>
#include <flipper_application/plugins/plugin_manager.h>
#include <loader/firmware_api/firmware_api.h>

#define TAG "CliShell"

#define HISTORY_DEPTH             10
#define COMPLETION_COLUMNS        3
#define COMPLETION_COLUMN_WIDTH   "30"
#define COMPLETION_COLUMN_WIDTH_I 30
#define ANSI_TIMEOUT_MS           10

ARRAY_DEF(ShellHistory, FuriString*, FURI_STRING_OPLIST); // -V524
#define M_OPL_ShellHistory_t() ARRAY_OPLIST(ShellHistory)

typedef struct {
    Cli* cli;

    FuriEventLoop* event_loop;
    PipeSide* pipe;
    CliAnsiParser* ansi_parser;
    FuriEventLoopTimer* ansi_parsing_timer;

    size_t history_position;
    size_t line_position;
    ShellHistory_t history;

    CommandCompletions_t completions;
    size_t selected_completion;
    bool is_displaying_completions;
} CliShell;

typedef struct {
    CliCommand* command;
    PipeSide* pipe;
    FuriString* args;
} CliCommandThreadData;

// ===============
// History helpers
// ===============

static FuriString* cli_shell_history_selected_line(CliShell* cli_shell) {
    return *ShellHistory_cget(cli_shell->history, cli_shell->history_position);
}

static FuriString* cli_shell_history_editing_line(CliShell* cli_shell) {
    return *ShellHistory_front(cli_shell->history);
}

// =========
// Execution
// =========

static int32_t cli_command_thread(void* context) {
    CliCommandThreadData* thread_data = context;
    if(!(thread_data->command->flags & CliCommandFlagDontAttachStdio))
        pipe_install_as_stdio(thread_data->pipe);

    thread_data->command->execute_callback(
        thread_data->pipe, thread_data->args, thread_data->command->context);

    fflush(stdout);
    return 0;
}

static void cli_shell_execute_command(CliShell* cli_shell, FuriString* command) {
    // split command into command and args
    size_t space = furi_string_search_char(command, ' ');
    if(space == FURI_STRING_FAILURE) space = furi_string_size(command);
    FuriString* command_name = furi_string_alloc_set(command);
    furi_string_left(command_name, space);
    FuriString* args = furi_string_alloc_set(command);
    furi_string_right(args, space + 1);

    PluginManager* plugin_manager = NULL;
    Loader* loader = NULL;
    CliCommand command_data;

    do {
        // find handler
        if(!cli_get_command(cli_shell->cli, command_name, &command_data)) {
            printf(
                ANSI_FG_RED "could not find command `%s`, try `help`" ANSI_RESET,
                furi_string_get_cstr(command_name));
            break;
        }

        // load external command
        if(command_data.flags & CliCommandFlagExternal) {
            plugin_manager =
                plugin_manager_alloc(PLUGIN_APP_ID, PLUGIN_API_VERSION, firmware_api_interface);
            FuriString* path = furi_string_alloc_printf(
                "%s/cli_%s.fal", CLI_COMMANDS_PATH, furi_string_get_cstr(command_name));
            uint32_t plugin_cnt_last = plugin_manager_get_count(plugin_manager);
            PluginManagerError error =
                plugin_manager_load_single(plugin_manager, furi_string_get_cstr(path));
            furi_string_free(path);

            if(error != PluginManagerErrorNone) {
                printf(ANSI_FG_RED "failed to load external command" ANSI_RESET);
                break;
            }

            const CliCommandDescriptor* plugin =
                plugin_manager_get_ep(plugin_manager, plugin_cnt_last);
            furi_assert(plugin);
            furi_check(furi_string_cmp_str(command_name, plugin->name) == 0);
            command_data.execute_callback = plugin->execute_callback;
            command_data.flags = plugin->flags | CliCommandFlagExternal;
            command_data.stack_depth = plugin->stack_depth;
        }

        // lock loader
        if(command_data.flags & CliCommandFlagParallelUnsafe) {
            loader = furi_record_open(RECORD_LOADER);
            bool success = loader_lock(loader);
            if(!success) {
                printf(ANSI_FG_RED
                       "this command cannot be run while an application is open" ANSI_RESET);
                break;
            }
        }

        // run command in separate thread
        CliCommandThreadData thread_data = {
            .command = &command_data,
            .pipe = cli_shell->pipe,
            .args = args,
        };
        FuriThread* thread = furi_thread_alloc_ex(
            furi_string_get_cstr(command_name),
            command_data.stack_depth,
            cli_command_thread,
            &thread_data);
        furi_thread_start(thread);
        furi_thread_join(thread);
        furi_thread_free(thread);
    } while(0);

    furi_string_free(command_name);
    furi_string_free(args);

    // unlock loader
    if(loader) loader_unlock(loader);
    furi_record_close(RECORD_LOADER);

    // unload external command
    if(plugin_manager) plugin_manager_free(plugin_manager);
}

// ====================
// Line editing helpers
// ====================

static size_t cli_shell_prompt_length(CliShell* cli_shell) {
    UNUSED(cli_shell);
    return strlen(">: ");
}

static void cli_shell_format_prompt(CliShell* cli_shell, char* buf, size_t length) {
    UNUSED(cli_shell);
    snprintf(buf, length - 1, ">: ");
}

static void cli_shell_prompt(CliShell* cli_shell) {
    char buffer[128];
    cli_shell_format_prompt(cli_shell, buffer, sizeof(buffer));
    printf("\r\n%s", buffer);
    fflush(stdout);
}

/**
 * If a line from history has been selected, moves it into the active line
 */
static void cli_shell_ensure_not_overwriting_history(CliShell* cli_shell) {
    if(cli_shell->history_position > 0) {
        FuriString* source = cli_shell_history_selected_line(cli_shell);
        FuriString* destination = cli_shell_history_editing_line(cli_shell);
        furi_string_set(destination, source);
        cli_shell->history_position = 0;
    }
}

typedef enum {
    CliCharClassWord,
    CliCharClassSpace,
    CliCharClassOther,
} CliCharClass;

/**
 * @brief Determines the class that a character belongs to
 * 
 * The return value of this function should not be used on its own; it should
 * only be used for comparing it with other values returned by this function.
 * This function is used internally in `cli_skip_run`.
 */
static CliCharClass cli_char_class(char c) {
    if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
        return CliCharClassWord;
    } else if(c == ' ') {
        return CliCharClassSpace;
    } else {
        return CliCharClassOther;
    }
}

typedef enum {
    CliSkipDirectionLeft,
    CliSkipDirectionRight,
} CliSkipDirection;

/**
 * @brief Skips a run of characters of the same class
 * 
 * @param string Input string
 * @param original_pos Position to start the search at
 * @param direction Direction in which to perform the search
 * @returns The position at which the run ends
 */
static size_t cli_skip_run(FuriString* string, size_t original_pos, CliSkipDirection direction) {
    if(furi_string_size(string) == 0) return original_pos;
    if(direction == CliSkipDirectionLeft && original_pos == 0) return original_pos;
    if(direction == CliSkipDirectionRight && original_pos == furi_string_size(string))
        return original_pos;

    int8_t look_offset = (direction == CliSkipDirectionLeft) ? -1 : 0;
    int8_t increment = (direction == CliSkipDirectionLeft) ? -1 : 1;
    int32_t position = original_pos;
    CliCharClass start_class =
        cli_char_class(furi_string_get_char(string, position + look_offset));

    while(true) {
        position += increment;
        if(position < 0) break;
        if(position >= (int32_t)furi_string_size(string)) break;
        if(cli_char_class(furi_string_get_char(string, position + look_offset)) != start_class)
            break;
    }

    return MAX(0, position);
}

// ============
// Autocomplete
// ============

/**
 * @brief Update for the completions menu
 */
typedef enum {
    CliShellCompletionsActionOpen,
    CliShellCompletionsActionClose,
    CliShellCompletionsActionUp,
    CliShellCompletionsActionDown,
    CliShellCompletionsActionLeft,
    CliShellCompletionsActionRight,
    CliShellCompletionsActionInput,
    CliShellCompletionsActionSelect,
} CliShellCompletionsAction;

typedef enum {
    CliShellCompletionSegmentTypeCommand,
    CliShellCompletionSegmentTypeArguments,
} CliShellCompletionSegmentType;

typedef struct {
    CliShellCompletionSegmentType type;
    size_t start;
    size_t length;
} CliShellCompletionSegment;

static CliShellCompletionSegment cli_shell_completion_segment(CliShell* cli_shell) {
    CliShellCompletionSegment segment;

    FuriString* input = furi_string_alloc_set(cli_shell_history_editing_line(cli_shell));
    furi_string_left(input, cli_shell->line_position);

    // find index of first non-space character
    size_t first_non_space = 0;
    while(1) {
        size_t ret = furi_string_search_char(input, ' ', first_non_space);
        if(ret == FURI_STRING_FAILURE) break;
        if(ret - first_non_space > 1) break;
        first_non_space++;
    }

    size_t first_space_in_command = furi_string_search_char(input, ' ', first_non_space);

    if(first_space_in_command == FURI_STRING_FAILURE) {
        segment.type = CliShellCompletionSegmentTypeCommand;
        segment.start = first_non_space;
        segment.length = furi_string_size(input) - first_non_space;
    } else {
        segment.type = CliShellCompletionSegmentTypeArguments;
        segment.start = 0;
        segment.length = 0;
        // support removed, might reimplement in the future
    }

    furi_string_free(input);
    return segment;
}

static void cli_shell_fill_completions(CliShell* cli_shell) {
    CommandCompletions_reset(cli_shell->completions);

    CliShellCompletionSegment segment = cli_shell_completion_segment(cli_shell);
    FuriString* input = furi_string_alloc_set(cli_shell_history_editing_line(cli_shell));
    furi_string_right(input, segment.start);
    furi_string_left(input, segment.length);

    if(segment.type == CliShellCompletionSegmentTypeCommand) {
        CliCommandTree_t* commands = cli_get_commands(cli_shell->cli);
        for
            M_EACH(registered_command, *commands, CliCommandTree_t) {
                FuriString* command_name = *registered_command->key_ptr;
                if(furi_string_start_with(command_name, input)) {
                    CommandCompletions_push_back(cli_shell->completions, command_name);
                }
            }

    } else {
        // support removed, might reimplement in the future
    }

    furi_string_free(input);
}

static void cli_shell_completions_render(CliShell* cli_shell, CliShellCompletionsAction action) {
    furi_assert(cli_shell);
    if(action == CliShellCompletionsActionOpen) furi_check(!cli_shell->is_displaying_completions);
    if(action == CliShellCompletionsActionInput) furi_check(cli_shell->is_displaying_completions);
    if(action == CliShellCompletionsActionClose) furi_check(cli_shell->is_displaying_completions);

    char prompt[64];
    cli_shell_format_prompt(cli_shell, prompt, sizeof(prompt));

    if(action == CliShellCompletionsActionOpen || action == CliShellCompletionsActionInput) {
        // show or update completions menu (full re-render)
        cli_shell_fill_completions(cli_shell);
        cli_shell->selected_completion = 0;

        printf("\n\r");
        size_t position = 0;
        for
            M_EACH(completion, cli_shell->completions, CommandCompletions_t) {
                if(position == cli_shell->selected_completion) printf(ANSI_INVERT);
                printf("%-" COMPLETION_COLUMN_WIDTH "s", furi_string_get_cstr(*completion));
                if(position == cli_shell->selected_completion) printf(ANSI_RESET);
                if((position % COMPLETION_COLUMNS == COMPLETION_COLUMNS - 1) &&
                   position != CommandCompletions_size(cli_shell->completions)) {
                    printf("\r\n");
                }
                position++;
            }

        if(!position) {
            printf(ANSI_FG_RED "no completions" ANSI_RESET);
        }

        size_t total_rows = (position / COMPLETION_COLUMNS) + 1;
        printf(
            ANSI_ERASE_DISPLAY(ANSI_ERASE_FROM_CURSOR_TO_END) ANSI_CURSOR_UP_BY("%zu")
                ANSI_CURSOR_HOR_POS("%zu"),
            total_rows,
            strlen(prompt) + cli_shell->line_position + 1);

        cli_shell->is_displaying_completions = true;

    } else if(action == CliShellCompletionsActionClose) {
        // clear completions menu
        printf(
            ANSI_CURSOR_HOR_POS("%zu") ANSI_ERASE_DISPLAY(ANSI_ERASE_FROM_CURSOR_TO_END)
                ANSI_CURSOR_HOR_POS("%zu"),
            strlen(prompt) + furi_string_size(cli_shell_history_selected_line(cli_shell)) + 1,
            strlen(prompt) + cli_shell->line_position + 1);
        cli_shell->is_displaying_completions = false;

    } else if(
        action == CliShellCompletionsActionUp || action == CliShellCompletionsActionDown ||
        action == CliShellCompletionsActionLeft || action == CliShellCompletionsActionRight) {
        if(CommandCompletions_empty_p(cli_shell->completions)) return;

        // move selection
        size_t old_selection = cli_shell->selected_completion;
        int new_selection_unclamped = cli_shell->selected_completion;
        if(action == CliShellCompletionsActionUp) new_selection_unclamped -= COMPLETION_COLUMNS;
        if(action == CliShellCompletionsActionDown) new_selection_unclamped += COMPLETION_COLUMNS;
        if(action == CliShellCompletionsActionLeft) new_selection_unclamped--;
        if(action == CliShellCompletionsActionRight) new_selection_unclamped++;
        size_t new_selection = CLAMP_WRAPAROUND(
            new_selection_unclamped, (int)CommandCompletions_size(cli_shell->completions) - 1, 0);
        cli_shell->selected_completion = new_selection;

        if(new_selection != old_selection) {
            // determine selection coordinates relative to top-left of suggestion menu
            size_t old_x = (old_selection % COMPLETION_COLUMNS) * COMPLETION_COLUMN_WIDTH_I;
            size_t old_y = old_selection / COMPLETION_COLUMNS;
            size_t new_x = (new_selection % COMPLETION_COLUMNS) * COMPLETION_COLUMN_WIDTH_I;
            size_t new_y = new_selection / COMPLETION_COLUMNS;
            printf("\n\r");

            // print old selection in normal colors
            if(old_y) printf(ANSI_CURSOR_DOWN_BY("%zu"), old_y);
            printf(ANSI_CURSOR_HOR_POS("%zu"), old_x + 1);
            printf(
                "%-" COMPLETION_COLUMN_WIDTH "s",
                furi_string_get_cstr(
                    *CommandCompletions_cget(cli_shell->completions, old_selection)));
            if(old_y) printf(ANSI_CURSOR_UP_BY("%zu"), old_y);
            printf(ANSI_CURSOR_HOR_POS("1"));

            // print new selection in inverted colors
            if(new_y) printf(ANSI_CURSOR_DOWN_BY("%zu"), new_y);
            printf(ANSI_CURSOR_HOR_POS("%zu"), new_x + 1);
            printf(
                ANSI_INVERT "%-" COMPLETION_COLUMN_WIDTH "s" ANSI_RESET,
                furi_string_get_cstr(
                    *CommandCompletions_cget(cli_shell->completions, new_selection)));

            // return cursor
            printf(ANSI_CURSOR_UP_BY("%zu"), new_y + 1);
            printf(
                ANSI_CURSOR_HOR_POS("%zu"),
                strlen(prompt) + furi_string_size(cli_shell_history_selected_line(cli_shell)) + 1);
        }

    } else if(action == CliShellCompletionsActionSelect) {
        // insert selection into prompt
        CliShellCompletionSegment segment = cli_shell_completion_segment(cli_shell);
        FuriString* input = cli_shell_history_selected_line(cli_shell);
        FuriString* completion =
            *CommandCompletions_cget(cli_shell->completions, cli_shell->selected_completion);
        furi_string_replace_at(
            input, segment.start, segment.length, furi_string_get_cstr(completion));
        printf(
            ANSI_CURSOR_HOR_POS("%zu") "%s" ANSI_ERASE_LINE(ANSI_ERASE_FROM_CURSOR_TO_END),
            strlen(prompt) + 1,
            furi_string_get_cstr(input));

        int position_change = (int)furi_string_size(completion) - (int)segment.length;
        cli_shell->line_position = MAX(0, (int)cli_shell->line_position + position_change);

        cli_shell_completions_render(cli_shell, CliShellCompletionsActionClose);

    } else {
        furi_crash();
    }

    fflush(stdout);
}

// =============
// Input handler
// =============

static void cli_shell_process_key(CliShell* cli_shell, CliKeyCombo key_combo) {
    FURI_LOG_T(
        TAG, "mod=%d, key=%d='%c'", key_combo.modifiers, key_combo.key, (char)key_combo.key);

    if(key_combo.modifiers == 0 && key_combo.key == CliKeyETX) { // usually Ctrl+C
        // reset input
        if(cli_shell->is_displaying_completions)
            cli_shell_completions_render(cli_shell, CliShellCompletionsActionClose);
        furi_string_reset(cli_shell_history_editing_line(cli_shell));
        cli_shell->line_position = 0;
        cli_shell->history_position = 0;
        printf("^C");
        cli_shell_prompt(cli_shell);

    } else if(key_combo.modifiers == 0 && key_combo.key == CliKeyFF) { // usually Ctrl+L
        // clear screen
        FuriString* command = cli_shell_history_selected_line(cli_shell);
        char prompt[64];
        cli_shell_format_prompt(cli_shell, prompt, sizeof(prompt));
        printf(
            ANSI_ERASE_DISPLAY(ANSI_ERASE_ENTIRE) ANSI_ERASE_SCROLLBACK_BUFFER ANSI_CURSOR_POS(
                "1", "1") "%s%s" ANSI_CURSOR_HOR_POS("%zu"),
            prompt,
            furi_string_get_cstr(command),
            strlen(prompt) + cli_shell->line_position + 1 /* 1-based column indexing */);
        fflush(stdout);

    } else if(key_combo.modifiers == 0 && key_combo.key == CliKeyCR) {
        if(cli_shell->is_displaying_completions) {
            // select completion
            cli_shell_completions_render(cli_shell, CliShellCompletionsActionSelect);
        } else {
            // get command and update history
            FuriString* command = furi_string_alloc();
            ShellHistory_pop_at(&command, cli_shell->history, cli_shell->history_position);
            furi_string_trim(command);
            if(cli_shell->history_position > 0) ShellHistory_pop_at(NULL, cli_shell->history, 0);
            if(!furi_string_empty(command)) ShellHistory_push_at(cli_shell->history, 0, command);
            FuriString* new_command = furi_string_alloc();
            ShellHistory_push_at(cli_shell->history, 0, new_command);
            furi_string_free(new_command);
            if(ShellHistory_size(cli_shell->history) > HISTORY_DEPTH) {
                ShellHistory_pop_back(NULL, cli_shell->history);
            }

            // execute command
            cli_shell->line_position = 0;
            cli_shell->history_position = 0;
            printf("\r\n");
            if(!furi_string_empty(command)) cli_shell_execute_command(cli_shell, command);
            furi_string_free(command);
            cli_shell_prompt(cli_shell);
        }

    } else if(key_combo.modifiers == 0 && (key_combo.key == CliKeyUp || key_combo.key == CliKeyDown)) {
        if(cli_shell->is_displaying_completions) {
            cli_shell_completions_render(
                cli_shell,
                (key_combo.key == CliKeyUp) ? CliShellCompletionsActionUp :
                                              CliShellCompletionsActionDown);
        } else {
            // go up and down in history
            int increment = (key_combo.key == CliKeyUp) ? 1 : -1;
            size_t new_pos = CLAMP(
                (int)cli_shell->history_position + increment,
                (int)ShellHistory_size(cli_shell->history) - 1,
                0);

            // print prompt with selected command
            if(new_pos != cli_shell->history_position) {
                cli_shell->history_position = new_pos;
                FuriString* command = cli_shell_history_selected_line(cli_shell);
                printf(
                    ANSI_CURSOR_HOR_POS("1") ">: %s" ANSI_ERASE_LINE(
                        ANSI_ERASE_FROM_CURSOR_TO_END),
                    furi_string_get_cstr(command));
                fflush(stdout);
                cli_shell->line_position = furi_string_size(command);
            }
        }

    } else if(
        key_combo.modifiers == 0 &&
        (key_combo.key == CliKeyLeft || key_combo.key == CliKeyRight)) {
        if(cli_shell->is_displaying_completions) {
            cli_shell_completions_render(
                cli_shell,
                (key_combo.key == CliKeyLeft) ? CliShellCompletionsActionLeft :
                                                CliShellCompletionsActionRight);

        } else {
            // go left and right in the current line
            FuriString* command = cli_shell_history_selected_line(cli_shell);
            int increment = (key_combo.key == CliKeyRight) ? 1 : -1;
            size_t new_pos = CLAMP(
                (int)cli_shell->line_position + increment, (int)furi_string_size(command), 0);

            // move cursor
            if(new_pos != cli_shell->line_position) {
                cli_shell->line_position = new_pos;
                printf(
                    "%s", (increment == 1) ? ANSI_CURSOR_RIGHT_BY("1") : ANSI_CURSOR_LEFT_BY("1"));
                fflush(stdout);
            }
        }

    } else if(key_combo.modifiers == 0 && key_combo.key == CliKeyHome) {
        // go to the start
        if(cli_shell->is_displaying_completions)
            cli_shell_completions_render(cli_shell, CliShellCompletionsActionClose);
        cli_shell->line_position = 0;
        printf(ANSI_CURSOR_HOR_POS("%d"), cli_shell_prompt_length(cli_shell) + 1);
        fflush(stdout);

    } else if(key_combo.modifiers == 0 && key_combo.key == CliKeyEnd) {
        // go to the end
        if(cli_shell->is_displaying_completions)
            cli_shell_completions_render(cli_shell, CliShellCompletionsActionClose);
        FuriString* line = cli_shell_history_selected_line(cli_shell);
        cli_shell->line_position = furi_string_size(line);
        printf(
            ANSI_CURSOR_HOR_POS("%zu"),
            cli_shell_prompt_length(cli_shell) + cli_shell->line_position + 1);
        fflush(stdout);

    } else if(
        key_combo.modifiers == 0 &&
        (key_combo.key == CliKeyBackspace || key_combo.key == CliKeyDEL)) {
        // erase one character
        cli_shell_ensure_not_overwriting_history(cli_shell);
        FuriString* line = cli_shell_history_editing_line(cli_shell);
        if(cli_shell->line_position == 0) {
            putc(CliKeyBell, stdout);
            fflush(stdout);
            return;
        }
        cli_shell->line_position--;
        furi_string_replace_at(line, cli_shell->line_position, 1, "");

        // move cursor, print the rest of the line, restore cursor
        printf(
            ANSI_CURSOR_LEFT_BY("1") "%s" ANSI_ERASE_LINE(ANSI_ERASE_FROM_CURSOR_TO_END),
            furi_string_get_cstr(line) + cli_shell->line_position);
        size_t left_by = furi_string_size(line) - cli_shell->line_position;
        if(left_by) // apparently LEFT_BY("0") still shifts left by one ._ .
            printf(ANSI_CURSOR_LEFT_BY("%zu"), left_by);
        fflush(stdout);

        if(cli_shell->is_displaying_completions)
            cli_shell_completions_render(cli_shell, CliShellCompletionsActionInput);

    } else if(
        key_combo.modifiers == CliModKeyCtrl &&
        (key_combo.key == CliKeyLeft || key_combo.key == CliKeyRight)) {
        // skip run of similar chars to the left or right
        if(cli_shell->is_displaying_completions)
            cli_shell_completions_render(cli_shell, CliShellCompletionsActionClose);
        FuriString* line = cli_shell_history_selected_line(cli_shell);
        CliSkipDirection direction = (key_combo.key == CliKeyLeft) ? CliSkipDirectionLeft :
                                                                     CliSkipDirectionRight;
        cli_shell->line_position = cli_skip_run(line, cli_shell->line_position, direction);
        printf(
            ANSI_CURSOR_HOR_POS("%zu"),
            cli_shell_prompt_length(cli_shell) + cli_shell->line_position + 1);
        fflush(stdout);

    } else if(key_combo.modifiers == 0 && key_combo.key == CliKeyETB) {
        // delete run of similar chars to the left
        cli_shell_ensure_not_overwriting_history(cli_shell);
        FuriString* line = cli_shell_history_selected_line(cli_shell);
        size_t run_start = cli_skip_run(line, cli_shell->line_position, CliSkipDirectionLeft);
        furi_string_replace_at(line, run_start, cli_shell->line_position - run_start, "");
        cli_shell->line_position = run_start;
        printf(
            ANSI_CURSOR_HOR_POS("%zu") "%s" ANSI_ERASE_LINE(ANSI_ERASE_FROM_CURSOR_TO_END)
                ANSI_CURSOR_HOR_POS("%zu"),
            cli_shell_prompt_length(cli_shell) + cli_shell->line_position + 1,
            furi_string_get_cstr(line) + run_start,
            cli_shell_prompt_length(cli_shell) + run_start + 1);
        fflush(stdout);

        if(cli_shell->is_displaying_completions)
            cli_shell_completions_render(cli_shell, CliShellCompletionsActionInput);

    } else if(key_combo.modifiers == 0 && key_combo.key == CliKeyTab) {
        // display completions
        cli_shell_ensure_not_overwriting_history(cli_shell);
        cli_shell_completions_render(
            cli_shell,
            cli_shell->is_displaying_completions ? CliShellCompletionsActionRight :
                                                   CliShellCompletionsActionOpen);

    } else if(key_combo.modifiers == 0 && key_combo.key == CliKeyEsc) {
        // close completions menu
        if(cli_shell->is_displaying_completions)
            cli_shell_completions_render(cli_shell, CliShellCompletionsActionClose);

    } else if(key_combo.modifiers == 0 && key_combo.key >= CliKeySpace && key_combo.key < CliKeyDEL) {
        // insert character
        cli_shell_ensure_not_overwriting_history(cli_shell);
        FuriString* line = cli_shell_history_editing_line(cli_shell);
        if(cli_shell->line_position == furi_string_size(line)) {
            furi_string_push_back(line, key_combo.key);
            printf("%c", key_combo.key);
        } else {
            const char in_str[2] = {key_combo.key, 0};
            furi_string_replace_at(line, cli_shell->line_position, 0, in_str);
            printf(ANSI_INSERT_MODE_ENABLE "%c" ANSI_INSERT_MODE_DISABLE, key_combo.key);
        }
        fflush(stdout);
        cli_shell->line_position++;

        if(cli_shell->is_displaying_completions)
            cli_shell_completions_render(cli_shell, CliShellCompletionsActionInput);
    }
}

static void cli_shell_pipe_broken(PipeSide* pipe, void* context) {
    UNUSED(pipe);
    CliShell* cli_shell = context;
    furi_event_loop_stop(cli_shell->event_loop);
}

static void cli_shell_data_available(PipeSide* pipe, void* context) {
    UNUSED(pipe);
    CliShell* cli_shell = context;

    furi_event_loop_timer_start(cli_shell->ansi_parsing_timer, furi_ms_to_ticks(ANSI_TIMEOUT_MS));

    // process ANSI escape sequences
    int c = getchar();
    furi_assert(c >= 0);
    CliAnsiParserResult parse_result = cli_ansi_parser_feed(cli_shell->ansi_parser, c);
    if(!parse_result.is_done) return;
    CliKeyCombo key_combo = parse_result.result;
    if(key_combo.key == CliKeyUnrecognized) return;

    cli_shell_process_key(cli_shell, key_combo);
}

static void cli_shell_timer_expired(void* context) {
    CliShell* cli_shell = context;
    CliAnsiParserResult parse_result = cli_ansi_parser_feed_timeout(cli_shell->ansi_parser);
    if(!parse_result.is_done) return;
    CliKeyCombo key_combo = parse_result.result;
    if(key_combo.key == CliKeyUnrecognized) return;

    cli_shell_process_key(cli_shell, key_combo);
}

static CliShell* cli_shell_alloc(PipeSide* pipe) {
    CliShell* cli_shell = malloc(sizeof(CliShell));
    cli_shell->cli = furi_record_open(RECORD_CLI);
    cli_shell->ansi_parser = cli_ansi_parser_alloc();
    cli_shell->event_loop = furi_event_loop_alloc();
    ShellHistory_init(cli_shell->history);
    FuriString* new_command = furi_string_alloc();
    ShellHistory_push_at(cli_shell->history, 0, new_command);
    furi_string_free(new_command);

    CommandCompletions_init(cli_shell->completions);
    cli_shell->selected_completion = 0;

    cli_shell->ansi_parsing_timer = furi_event_loop_timer_alloc(
        cli_shell->event_loop, cli_shell_timer_expired, FuriEventLoopTimerTypeOnce, cli_shell);

    cli_shell->pipe = pipe;
    pipe_install_as_stdio(cli_shell->pipe);
    pipe_attach_to_event_loop(cli_shell->pipe, cli_shell->event_loop);
    pipe_set_callback_context(cli_shell->pipe, cli_shell);
    pipe_set_data_arrived_callback(cli_shell->pipe, cli_shell_data_available, 0);
    pipe_set_broken_callback(cli_shell->pipe, cli_shell_pipe_broken, FuriEventLoopEventFlagEdge);

    return cli_shell;
}

static void cli_shell_free(CliShell* cli_shell) {
    CommandCompletions_clear(cli_shell->completions);
    furi_event_loop_timer_free(cli_shell->ansi_parsing_timer);
    pipe_detach_from_event_loop(cli_shell->pipe);
    pipe_free(cli_shell->pipe);
    ShellHistory_clear(cli_shell->history);
    furi_event_loop_free(cli_shell->event_loop);
    cli_ansi_parser_free(cli_shell->ansi_parser);
    furi_record_close(RECORD_CLI);
    free(cli_shell);
}

static void cli_shell_motd(void) {
    printf(ANSI_FLIPPER_BRAND_ORANGE
           "\r\n"
           "              _.-------.._                    -,\r\n"
           "          .-\"```\"--..,,_/ /`-,               -,  \\ \r\n"
           "       .:\"          /:/  /'\\  \\     ,_...,  `. |  |\r\n"
           "      /       ,----/:/  /`\\ _\\~`_-\"`     _;\r\n"
           "     '      / /`\"\"\"'\\ \\ \\.~`_-'      ,-\"'/ \r\n"
           "    |      | |  0    | | .-'      ,/`  /\r\n"
           "   |    ,..\\ \\     ,.-\"`       ,/`    /\r\n"
           "  ;    :    `/`\"\"\\`           ,/--==,/-----,\r\n"
           "  |    `-...|        -.___-Z:_______J...---;\r\n"
           "  :         `                           _-'\r\n"
           " _L_  _     ___  ___  ___  ___  ____--\"`___  _     ___\r\n"
           "| __|| |   |_ _|| _ \\| _ \\| __|| _ \\   / __|| |   |_ _|\r\n"
           "| _| | |__  | | |  _/|  _/| _| |   /  | (__ | |__  | |\r\n"
           "|_|  |____||___||_|  |_|  |___||_|_\\   \\___||____||___|\r\n"
           "\r\n" ANSI_FG_BR_WHITE "Welcome to Flipper Zero Command Line Interface!\r\n"
           "Read the manual: https://docs.flipper.net/development/cli\r\n"
           "Run `help` or `?` to list available commands\r\n"
           "\r\n" ANSI_RESET);

    const Version* firmware_version = furi_hal_version_get_firmware_version();
    if(firmware_version) {
        printf(
            "Firmware version: %s %s (%s%s built on %s)\r\n",
            version_get_gitbranch(firmware_version),
            version_get_version(firmware_version),
            version_get_githash(firmware_version),
            version_get_dirty_flag(firmware_version) ? "-dirty" : "",
            version_get_builddate(firmware_version));
    }
}

static int32_t cli_shell_thread(void* context) {
    PipeSide* pipe = context;
    CliShell* cli_shell = cli_shell_alloc(pipe);

    FURI_LOG_D(TAG, "Started");
    cli_enumerate_external_commands(cli_shell->cli);
    cli_shell_motd();
    cli_shell_prompt(cli_shell);
    furi_event_loop_run(cli_shell->event_loop);
    FURI_LOG_D(TAG, "Stopped");

    cli_shell_free(cli_shell);
    return 0;
}

FuriThread* cli_shell_start(PipeSide* pipe) {
    FuriThread* thread =
        furi_thread_alloc_ex("CliShell", CLI_SHELL_STACK_SIZE, cli_shell_thread, pipe);
    furi_thread_start(thread);
    return thread;
}
