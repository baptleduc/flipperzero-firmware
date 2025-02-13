#pragma once
#include <furi.h>
#include <toolbox/api_lock.h>
#include <flipper_application/flipper_application.h>

#include <gui/gui.h>
#include <gui/view_holder.h>
#include <gui/modules/loading.h>

#include <m-array.h>

#include "loader.h"
#include "loader_menu.h"
#include "loader_applications.h"

#define LAUNCH_QUEUE_MAX_SIZE 5

typedef struct {
    FuriString* launch_path;
    char* args;
    FuriThread* thread;
    bool insomniac;
    FlipperApplication* fap;
} LoaderAppData;

typedef struct {
    FuriString* name_or_path;
    FuriString* args;
    LoaderDeferredLaunchFlag flags;
} LoaderDeferredLaunchRecord;

static inline void loader_dl_record_init(LoaderDeferredLaunchRecord* record) {
    record->name_or_path = furi_string_alloc();
    record->args = furi_string_alloc();
    record->flags = LoaderDeferredLaunchFlagNone;
}
#define LOADER_DL_RECORD_INIT(r) (loader_dl_record_init(&(r)))

static inline void loader_dl_record_init_set(
    LoaderDeferredLaunchRecord* dest,
    const LoaderDeferredLaunchRecord* src) {
    dest->name_or_path = furi_string_alloc_set(src->name_or_path);
    dest->args = furi_string_alloc_set(src->args);
    dest->flags = src->flags;
}
#define LOADER_DL_RECORD_INIT_SET(d, s) (loader_dl_record_init_set(&(d), &(s)))

static inline void
    loader_dl_record_set(LoaderDeferredLaunchRecord* dest, const LoaderDeferredLaunchRecord* src) {
    furi_string_set(dest->name_or_path, src->name_or_path);
    furi_string_set(dest->args, src->args);
    dest->flags = src->flags;
}
#define LOADER_DL_RECORD_SET(d, s) (loader_dl_record_set(&(d), &(s)))

static inline void loader_dl_record_clear(LoaderDeferredLaunchRecord* record) {
    furi_string_free(record->name_or_path);
    furi_string_free(record->args);
}
#define LOADER_DL_RECORD_CLEAR(r) (loader_dl_record_clear(&(r)))

#define LOADER_DL_RECORD_OPLIST           \
    (INIT(LOADER_DL_RECORD_INIT),         \
     INIT_SET(LOADER_DL_RECORD_INIT_SET), \
     SET(LOADER_DL_RECORD_SET),           \
     CLEAR(LOADER_DL_RECORD_CLEAR))

ARRAY_DEF(LoaderDeferredLaunchRecordArray, LoaderDeferredLaunchRecord, LOADER_DL_RECORD_OPLIST);

struct Loader {
    FuriPubSub* pubsub;
    FuriMessageQueue* queue;
    LoaderMenu* loader_menu;
    LoaderApplications* loader_applications;
    LoaderAppData app;
    LoaderDeferredLaunchRecordArray_t launch_queue;

    Gui* gui;
    ViewHolder* view_holder;
    Loading* loading;
};

typedef enum {
    LoaderMessageTypeStartByName,
    LoaderMessageTypeAppClosed,
    LoaderMessageTypeShowMenu,
    LoaderMessageTypeMenuClosed,
    LoaderMessageTypeApplicationsClosed,
    LoaderMessageTypeLock,
    LoaderMessageTypeUnlock,
    LoaderMessageTypeIsLocked,
    LoaderMessageTypeStartByNameDetachedWithGuiError,
    LoaderMessageTypeSignal,
    LoaderMessageTypeGetApplicationName,
    LoaderMessageTypeGetApplicationLaunchPath,
    LoaderMessageTypeEnqueueLaunch,
    LoaderMessageTypeClearLaunchQueue,
} LoaderMessageType;

typedef struct {
    const char* name;
    const char* args;
    FuriString* error_message;
} LoaderMessageStartByName;

typedef struct {
    const char* name;
    const char* args;
    LoaderDeferredLaunchFlag flags;
} LoaderMessageDeferStart;

typedef struct {
    uint32_t signal;
    void* arg;
} LoaderMessageSignal;

typedef enum {
    LoaderStatusErrorUnknown,
    LoaderStatusErrorInvalidFile,
    LoaderStatusErrorInvalidManifest,
    LoaderStatusErrorMissingImports,
    LoaderStatusErrorHWMismatch,
    LoaderStatusErrorOutdatedApp,
    LoaderStatusErrorOutOfMemory,
    LoaderStatusErrorOutdatedFirmware,
} LoaderStatusError;

typedef struct {
    LoaderStatus value;
    LoaderStatusError error;
} LoaderMessageLoaderStatusResult;

typedef struct {
    bool value;
} LoaderMessageBoolResult;

typedef struct {
    FuriApiLock api_lock;
    LoaderMessageType type;

    union {
        LoaderMessageStartByName start;
        LoaderMessageDeferStart defer_start;
        LoaderMessageSignal signal;
        FuriString* application_name;
    };

    union {
        LoaderMessageLoaderStatusResult* status_value;
        LoaderMessageBoolResult* bool_value;
    };
} LoaderMessage;
