#include "cli_shell.h"
#include "../cli_ansi.h"
#include "../cli_i.h"
#include "../cli_commands.h"
#include "cli_shell_completions.h"
#include "cli_shell_line.h"
#include <stdio.h>
#include <furi_hal_version.h>
#include <m-array.h>
#include <loader/loader.h>
#include <toolbox/pipe.h>
#include <flipper_application/plugins/plugin_manager.h>
#include <loader/firmware_api/firmware_api.h>

#define TAG "CliShell"

#define HISTORY_DEPTH   10
#define ANSI_TIMEOUT_MS 10

typedef struct {
    Cli* cli;

    FuriEventLoop* event_loop;
    PipeSide* pipe;
    CliAnsiParser* ansi_parser;
    FuriEventLoopTimer* ansi_parsing_timer;

    CliShellLine line;

    CliShellCompletions completions;
} CliShell;

typedef struct {
    CliCommand* command;
    PipeSide* pipe;
    FuriString* args;
} CliCommandThreadData;

// ===============
// History helpers
// ===============

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

            // external commands have to run in an external thread
            furi_check(!(command_data.flags & CliCommandFlagUseShellThread));
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

        if(command_data.flags & CliCommandFlagUseShellThread) {
            // run command in this thread
            command_data.execute_callback(cli_shell->pipe, args, command_data.context);
        } else {
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
        }
    } while(0);

    furi_string_free(command_name);
    furi_string_free(args);

    // unlock loader
    if(loader) loader_unlock(loader);
    furi_record_close(RECORD_LOADER);

    // unload external command
    if(plugin_manager) plugin_manager_free(plugin_manager);
}

// =============
// Input handler
// =============

static void cli_shell_process_key(CliShell* cli_shell, CliKeyCombo key_combo) {
    FURI_LOG_T(
        TAG, "mod=%d, key=%d='%c'", key_combo.modifiers, key_combo.key, (char)key_combo.key);

    if(key_combo.modifiers == 0 && key_combo.key == CliKeyETX) { // usually Ctrl+C
        // reset input
        if(cli_shell->completions.is_displaying)
            cli_shell_completions_render(
                &cli_shell->completions,
                &cli_shell->line,
                cli_shell->cli,
                CliShellCompletionsActionClose);
        furi_string_reset(cli_shell_line_get_editing(&cli_shell->line));
        cli_shell->line.line_position = 0;
        cli_shell->line.history_position = 0;
        printf("^C");
        cli_shell_line_prompt(&cli_shell->line);

    } else if(key_combo.modifiers == 0 && key_combo.key == CliKeyFF) { // usually Ctrl+L
        // clear screen
        FuriString* command = cli_shell_line_get_selected(&cli_shell->line);
        char prompt[64];
        cli_shell_line_format_prompt(&cli_shell->line, prompt, sizeof(prompt));
        printf(
            ANSI_ERASE_DISPLAY(ANSI_ERASE_ENTIRE) ANSI_ERASE_SCROLLBACK_BUFFER ANSI_CURSOR_POS(
                "1", "1") "%s%s" ANSI_CURSOR_HOR_POS("%zu"),
            prompt,
            furi_string_get_cstr(command),
            strlen(prompt) + cli_shell->line.line_position + 1 /* 1-based column indexing */);
        fflush(stdout);

    } else if(key_combo.modifiers == 0 && key_combo.key == CliKeyCR) {
        if(cli_shell->completions.is_displaying) {
            // select completion
            cli_shell_completions_render(
                &cli_shell->completions,
                &cli_shell->line,
                cli_shell->cli,
                CliShellCompletionsActionSelect);
        } else {
            // get command and update history
            FuriString* command = furi_string_alloc();
            ShellHistory_pop_at(
                &command, cli_shell->line.history, cli_shell->line.history_position);
            furi_string_trim(command);
            if(cli_shell->line.history_position > 0)
                ShellHistory_pop_at(NULL, cli_shell->line.history, 0);
            if(!furi_string_empty(command))
                ShellHistory_push_at(cli_shell->line.history, 0, command);
            FuriString* new_command = furi_string_alloc();
            ShellHistory_push_at(cli_shell->line.history, 0, new_command);
            furi_string_free(new_command);
            if(ShellHistory_size(cli_shell->line.history) > HISTORY_DEPTH) {
                ShellHistory_pop_back(NULL, cli_shell->line.history);
            }

            // execute command
            cli_shell->line.line_position = 0;
            cli_shell->line.history_position = 0;
            printf("\r\n");
            if(!furi_string_empty(command)) cli_shell_execute_command(cli_shell, command);
            furi_string_free(command);
            cli_shell_line_prompt(&cli_shell->line);
        }

    } else if(key_combo.modifiers == 0 && (key_combo.key == CliKeyUp || key_combo.key == CliKeyDown)) {
        if(cli_shell->completions.is_displaying) {
            cli_shell_completions_render(
                &cli_shell->completions,
                &cli_shell->line,
                cli_shell->cli,
                (key_combo.key == CliKeyUp) ? CliShellCompletionsActionUp :
                                              CliShellCompletionsActionDown);
        } else {
            // go up and down in history
            int increment = (key_combo.key == CliKeyUp) ? 1 : -1;
            size_t new_pos = CLAMP(
                (int)cli_shell->line.history_position + increment,
                (int)ShellHistory_size(cli_shell->line.history) - 1,
                0);

            // print prompt with selected command
            if(new_pos != cli_shell->line.history_position) {
                cli_shell->line.history_position = new_pos;
                FuriString* command = cli_shell_line_get_selected(&cli_shell->line);
                printf(
                    ANSI_CURSOR_HOR_POS("1") ">: %s" ANSI_ERASE_LINE(
                        ANSI_ERASE_FROM_CURSOR_TO_END),
                    furi_string_get_cstr(command));
                fflush(stdout);
                cli_shell->line.line_position = furi_string_size(command);
            }
        }

    } else if(
        key_combo.modifiers == 0 &&
        (key_combo.key == CliKeyLeft || key_combo.key == CliKeyRight)) {
        if(cli_shell->completions.is_displaying) {
            cli_shell_completions_render(
                &cli_shell->completions,
                &cli_shell->line,
                cli_shell->cli,
                (key_combo.key == CliKeyLeft) ? CliShellCompletionsActionLeft :
                                                CliShellCompletionsActionRight);

        } else {
            // go left and right in the current line
            FuriString* command = cli_shell_line_get_selected(&cli_shell->line);
            int increment = (key_combo.key == CliKeyRight) ? 1 : -1;
            size_t new_pos = CLAMP(
                (int)cli_shell->line.line_position + increment, (int)furi_string_size(command), 0);

            // move cursor
            if(new_pos != cli_shell->line.line_position) {
                cli_shell->line.line_position = new_pos;
                printf(
                    "%s", (increment == 1) ? ANSI_CURSOR_RIGHT_BY("1") : ANSI_CURSOR_LEFT_BY("1"));
                fflush(stdout);
            }
        }

    } else if(key_combo.modifiers == 0 && key_combo.key == CliKeyHome) {
        // go to the start
        if(cli_shell->completions.is_displaying)
            cli_shell_completions_render(
                &cli_shell->completions,
                &cli_shell->line,
                cli_shell->cli,
                CliShellCompletionsActionClose);
        cli_shell->line.line_position = 0;
        printf(ANSI_CURSOR_HOR_POS("%zu"), cli_shell_line_prompt_length(&cli_shell->line) + 1);
        fflush(stdout);

    } else if(key_combo.modifiers == 0 && key_combo.key == CliKeyEnd) {
        // go to the end
        if(cli_shell->completions.is_displaying)
            cli_shell_completions_render(
                &cli_shell->completions,
                &cli_shell->line,
                cli_shell->cli,
                CliShellCompletionsActionClose);
        FuriString* line = cli_shell_line_get_selected(&cli_shell->line);
        cli_shell->line.line_position = furi_string_size(line);
        printf(
            ANSI_CURSOR_HOR_POS("%zu"),
            cli_shell_line_prompt_length(&cli_shell->line) + cli_shell->line.line_position + 1);
        fflush(stdout);

    } else if(
        key_combo.modifiers == 0 &&
        (key_combo.key == CliKeyBackspace || key_combo.key == CliKeyDEL)) {
        // erase one character
        cli_shell_line_ensure_not_overwriting_history(&cli_shell->line);
        FuriString* line = cli_shell_line_get_editing(&cli_shell->line);
        if(cli_shell->line.line_position == 0) {
            putc(CliKeyBell, stdout);
            fflush(stdout);
            return;
        }
        cli_shell->line.line_position--;
        furi_string_replace_at(line, cli_shell->line.line_position, 1, "");

        // move cursor, print the rest of the line, restore cursor
        printf(
            ANSI_CURSOR_LEFT_BY("1") "%s" ANSI_ERASE_LINE(ANSI_ERASE_FROM_CURSOR_TO_END),
            furi_string_get_cstr(line) + cli_shell->line.line_position);
        size_t left_by = furi_string_size(line) - cli_shell->line.line_position;
        if(left_by) // apparently LEFT_BY("0") still shifts left by one ._ .
            printf(ANSI_CURSOR_LEFT_BY("%zu"), left_by);
        fflush(stdout);

        if(cli_shell->completions.is_displaying)
            cli_shell_completions_render(
                &cli_shell->completions,
                &cli_shell->line,
                cli_shell->cli,
                CliShellCompletionsActionInput);

    } else if(
        key_combo.modifiers == CliModKeyCtrl &&
        (key_combo.key == CliKeyLeft || key_combo.key == CliKeyRight)) {
        // skip run of similar chars to the left or right
        if(cli_shell->completions.is_displaying)
            cli_shell_completions_render(
                &cli_shell->completions,
                &cli_shell->line,
                cli_shell->cli,
                CliShellCompletionsActionClose);
        FuriString* line = cli_shell_line_get_selected(&cli_shell->line);
        CliSkipDirection direction = (key_combo.key == CliKeyLeft) ? CliSkipDirectionLeft :
                                                                     CliSkipDirectionRight;
        cli_shell->line.line_position =
            cli_shell_line_skip_run(line, cli_shell->line.line_position, direction);
        printf(
            ANSI_CURSOR_HOR_POS("%zu"),
            cli_shell_line_prompt_length(&cli_shell->line) + cli_shell->line.line_position + 1);
        fflush(stdout);

    } else if(key_combo.modifiers == 0 && key_combo.key == CliKeyETB) {
        // delete run of similar chars to the left
        cli_shell_line_ensure_not_overwriting_history(&cli_shell->line);
        FuriString* line = cli_shell_line_get_selected(&cli_shell->line);
        size_t run_start =
            cli_shell_line_skip_run(line, cli_shell->line.line_position, CliSkipDirectionLeft);
        furi_string_replace_at(line, run_start, cli_shell->line.line_position - run_start, "");
        cli_shell->line.line_position = run_start;
        printf(
            ANSI_CURSOR_HOR_POS("%zu") "%s" ANSI_ERASE_LINE(ANSI_ERASE_FROM_CURSOR_TO_END)
                ANSI_CURSOR_HOR_POS("%zu"),
            cli_shell_line_prompt_length(&cli_shell->line) + cli_shell->line.line_position + 1,
            furi_string_get_cstr(line) + run_start,
            cli_shell_line_prompt_length(&cli_shell->line) + run_start + 1);
        fflush(stdout);

        if(cli_shell->completions.is_displaying)
            cli_shell_completions_render(
                &cli_shell->completions,
                &cli_shell->line,
                cli_shell->cli,
                CliShellCompletionsActionInput);

    } else if(key_combo.modifiers == 0 && key_combo.key == CliKeyTab) {
        // display completions
        cli_shell_line_ensure_not_overwriting_history(&cli_shell->line);
        cli_shell_completions_render(
            &cli_shell->completions,
            &cli_shell->line,
            cli_shell->cli,
            cli_shell->completions.is_displaying ? CliShellCompletionsActionRight :
                                                   CliShellCompletionsActionOpen);

    } else if(key_combo.modifiers == 0 && key_combo.key == CliKeyEsc) {
        // close completions menu
        if(cli_shell->completions.is_displaying)
            cli_shell_completions_render(
                &cli_shell->completions,
                &cli_shell->line,
                cli_shell->cli,
                CliShellCompletionsActionClose);

    } else if(key_combo.modifiers == 0 && key_combo.key >= CliKeySpace && key_combo.key < CliKeyDEL) {
        // insert character
        cli_shell_line_ensure_not_overwriting_history(&cli_shell->line);
        FuriString* line = cli_shell_line_get_editing(&cli_shell->line);
        if(cli_shell->line.line_position == furi_string_size(line)) {
            furi_string_push_back(line, key_combo.key);
            printf("%c", key_combo.key);
        } else {
            const char in_str[2] = {key_combo.key, 0};
            furi_string_replace_at(line, cli_shell->line.line_position, 0, in_str);
            printf(ANSI_INSERT_MODE_ENABLE "%c" ANSI_INSERT_MODE_DISABLE, key_combo.key);
        }
        fflush(stdout);
        cli_shell->line.line_position++;

        if(cli_shell->completions.is_displaying)
            cli_shell_completions_render(
                &cli_shell->completions,
                &cli_shell->line,
                cli_shell->cli,
                CliShellCompletionsActionInput);
    }
}

static void cli_shell_pipe_broken(PipeSide* pipe, void* context) {
    // allow commands to be processed before we stop the shell
    if(pipe_bytes_available(pipe)) return;

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
    ShellHistory_init(cli_shell->line.history);
    FuriString* new_command = furi_string_alloc();
    ShellHistory_push_at(cli_shell->line.history, 0, new_command);
    furi_string_free(new_command);

    CommandCompletions_init(cli_shell->completions.variants);
    cli_shell->completions.selected = 0;

    cli_shell->ansi_parsing_timer = furi_event_loop_timer_alloc(
        cli_shell->event_loop, cli_shell_timer_expired, FuriEventLoopTimerTypeOnce, cli_shell);

    cli_shell->pipe = pipe;
    pipe_install_as_stdio(cli_shell->pipe);
    pipe_attach_to_event_loop(cli_shell->pipe, cli_shell->event_loop);
    pipe_set_callback_context(cli_shell->pipe, cli_shell);
    pipe_set_data_arrived_callback(cli_shell->pipe, cli_shell_data_available, 0);
    pipe_set_broken_callback(cli_shell->pipe, cli_shell_pipe_broken, 0);

    return cli_shell;
}

static void cli_shell_free(CliShell* cli_shell) {
    CommandCompletions_clear(cli_shell->completions.variants);
    furi_event_loop_timer_free(cli_shell->ansi_parsing_timer);
    pipe_detach_from_event_loop(cli_shell->pipe);
    pipe_free(cli_shell->pipe);
    ShellHistory_clear(cli_shell->line.history);
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
    cli_shell_line_prompt(&cli_shell->line);
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
