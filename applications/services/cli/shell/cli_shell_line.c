#include "cli_shell_line.h"

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
