#pragma once

#include <furi.h>
#include <m-array.h>
#include "cli_shell_line.h"
#include "../cli.h"
#include "../cli_i.h"

#ifdef __cplusplus
extern "C" {
#endif

ARRAY_DEF(CommandCompletions, FuriString*, FURI_STRING_OPLIST); // -V524
#define M_OPL_CommandCompletions_t() ARRAY_OPLIST(CommandCompletions)

#define COMPLETION_COLUMNS        3
#define COMPLETION_COLUMN_WIDTH   "30"
#define COMPLETION_COLUMN_WIDTH_I 30

typedef struct {
    CommandCompletions_t variants;
    size_t selected;
    bool is_displaying;
} CliShellCompletions;

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

CliShellCompletionSegment
    cli_shell_completions_segment(CliShellCompletions* completions, CliShellLine* line);

void cli_shell_completions_fill_variants(
    CliShellCompletions* completions,
    CliShellLine* line,
    Cli* cli);

void cli_shell_completions_render(
    CliShellCompletions* completions,
    CliShellLine* line,
    Cli* cli,
    CliShellCompletionsAction action);

#ifdef __cplusplus
}
#endif
