#include <furi.h>
#include <dialogs/dialogs.h>
#include <loader/loader.h>

int32_t chaining_test_app_b(const char* arg) {
    if(!arg) return 0;

    Loader* loader = furi_record_open(RECORD_LOADER);
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);

    FuriString* referrer = furi_string_alloc();
    loader_get_referring_application(loader, referrer);
    furi_check(furi_string_cmp_str(referrer, "Loader Chaining Test: App A") == 0);
    furi_string_free(referrer);

    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, "Hi, I am B", 64, 0, AlignCenter, AlignTop);
    FuriString* text = furi_string_alloc_printf("And A told me:\n%s", arg);
    dialog_message_set_text(message, furi_string_get_cstr(text), 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, "Launch A", NULL, NULL);
    dialog_message_show(dialogs, message);
    dialog_message_free(message);
    furi_string_free(text);

    furi_check(loader_launch_app_after_current(
        loader,
        "/ext/apps/Debug/loader_chaining_a.fap",
        NULL,
        LoaderDeferredLaunchErrorReportDiscard));

    furi_record_close(RECORD_LOADER);
    furi_record_close(RECORD_DIALOGS);
    return 0;
}
