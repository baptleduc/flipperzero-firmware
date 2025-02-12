#pragma once
#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_LOADER            "loader"
#define LOADER_APPLICATIONS_NAME "Apps"

typedef struct Loader Loader;

typedef enum {
    LoaderStatusOk,
    LoaderStatusErrorAppStarted,
    LoaderStatusErrorUnknownApp,
    LoaderStatusErrorInternal,
} LoaderStatus;

typedef enum {
    LoaderEventTypeApplicationBeforeLoad,
    LoaderEventTypeApplicationLoadFailed,
    LoaderEventTypeApplicationStopped
} LoaderEventType;

typedef struct {
    LoaderEventType type;
} LoaderEvent;

/**
 * Error reporting mechanism selector for `loader_launch_app_after_current`:
 *   - `Gui`: Report errors to the user via a dialog
 *   - `Args`: Report errors to the referring app via args
 * 
 * The argument used to report an error takes the form of
 * `loader:deferred_launch_err:Message goes here`
 */
typedef enum {
    LoaderDeferredLaunchErrorReportDiscard = 0, //<! Silently ignore any errors
    LoaderDeferredLaunchErrorReportGui = (1 << 1), //<! Report errors to the user via a dialog
    LoaderDeferredLaunchErrorReportArgs =
        (1 << 2), //<! Report errors to the referring app via args
} LoaderDeferredLaunchErrorReport;

/**
 * @brief Start application
 * @param[in] instance loader instance
 * @param[in] name application name or id
 * @param[in] args application arguments
 * @param[out] error_message detailed error message, can be NULL
 * @return LoaderStatus
 */
LoaderStatus
    loader_start(Loader* instance, const char* name, const char* args, FuriString* error_message);

/**
 * @brief Start application with GUI error message
 * @param[in] instance loader instance
 * @param[in] name application name or id
 * @param[in] args application arguments
 * @return LoaderStatus
 */
LoaderStatus loader_start_with_gui_error(Loader* loader, const char* name, const char* args);

/**
 * @brief Start application detached with GUI error message
 * @param[in] instance loader instance
 * @param[in] name application name or id
 * @param[in] args application arguments
 */
void loader_start_detached_with_gui_error(Loader* loader, const char* name, const char* args);

/**
 * @brief Lock application start
 * @param[in] instance loader instance
 * @return true on success
 */
bool loader_lock(Loader* instance);

/**
 * @brief Unlock application start
 * @param[in] instance loader instance
 */
void loader_unlock(Loader* instance);

/**
 * @brief Check if loader is locked
 * @param[in] instance loader instance
 * @return true if locked
 */
bool loader_is_locked(Loader* instance);

/**
 * @brief Show loader menu
 * @param[in] instance loader instance
 */
void loader_show_menu(Loader* instance);

/**
 * @brief Get loader pubsub
 * @param[in] instance loader instance
 * @return FuriPubSub* 
 */
FuriPubSub* loader_get_pubsub(Loader* instance);

/**
 * @brief Send a signal to the currently running application
 *
 * @param[in] instance pointer to the loader instance
 * @param[in] signal signal value to be sent
 * @param[inout] arg optional argument (can be of any value, including NULL)
 *
 * @return true if the signal was handled by the application, false otherwise
 */
bool loader_signal(Loader* instance, uint32_t signal, void* arg);

/**
 * @brief Get the name of the currently running application
 *
 * @param[in] instance pointer to the loader instance
 * @param[inout] name pointer to the string to contain the name (must be allocated)
 * @return true if it was possible to get an application name, false otherwise
 */
bool loader_get_application_name(Loader* instance, FuriString* name);

/**
 * @brief Sets the application to launch after the calling application exits
 * 
 * @param[in] instance pointer to the loader instance
 * @param[in] name pointer to the name or path of the application, or NULL to
 *                 cancel a previous request
 * @param[in] args pointer to argument to provide to the next app
 * @param[in] error_report which way(s) to report launch errors in. see enum
 *                         documentation for more info
 * @return true if the operation succeeded, false otherwise
 */
bool loader_launch_app_after_current(
    Loader* instance,
    const char* name,
    const char* args,
    LoaderDeferredLaunchErrorReport error_report);

/**
 * @brief Enqueues a request to launch the current application after the
 * deferred one (as set via `loader_launch_app_after_current`) exits
 * 
 * @param[in] instance pointer to the loader instance
 * @param[in] args args to provide to the current application when it is
 *                 next launched via this mechanism
 * @param[in] error_report see enum documentation. only the `Gui` flag is valid.
 * @return true if the operation succeeded, false otherwise
 */
bool loader_launch_current_app_after_deferred(
    Loader* instance,
    const char* args,
    LoaderDeferredLaunchErrorReport error_report);

#ifdef __cplusplus
}
#endif
