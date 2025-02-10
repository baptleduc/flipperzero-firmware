#pragma once

#include <furi.h>
#include <m-array.h>

#ifdef __cplusplus
extern "C" {
#endif

ARRAY_DEF(ShellHistory, FuriString*, FURI_STRING_OPLIST); // -V524
#define M_OPL_ShellHistory_t() ARRAY_OPLIST(ShellHistory)

typedef struct {
    size_t history_position;
    size_t line_position;
    ShellHistory_t history;
} CliShellLine;

typedef enum {
    CliCharClassWord,
    CliCharClassSpace,
    CliCharClassOther,
} CliCharClass;

typedef enum {
    CliSkipDirectionLeft,
    CliSkipDirectionRight,
} CliSkipDirection;

FuriString* cli_shell_line_get_selected(CliShellLine* line);

FuriString* cli_shell_line_get_editing(CliShellLine* line);

size_t cli_shell_line_prompt_length(CliShellLine* line);

void cli_shell_line_format_prompt(CliShellLine* line, char* buf, size_t length);

void cli_shell_line_prompt(CliShellLine* line);

/**
 * @brief If a line from history has been selected, moves it into the active line
 */
void cli_shell_line_ensure_not_overwriting_history(CliShellLine* line);

/**
 * @brief Determines the class that a character belongs to
 * 
 * The return value of this function should not be used on its own; it should
 * only be used for comparing it with other values returned by this function.
 * This function is used internally in `cli_skip_run`.
 */
CliCharClass cli_shell_line_char_class(char c);

/**
 * @brief Skips a run of characters of the same class
 * 
 * @param string Input string
 * @param original_pos Position to start the search at
 * @param direction Direction in which to perform the search
 * @returns The position at which the run ends
 */
size_t
    cli_shell_line_skip_run(FuriString* string, size_t original_pos, CliSkipDirection direction);

#ifdef __cplusplus
}
#endif
