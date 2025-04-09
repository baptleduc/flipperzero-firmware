#include "lora_scene.h"
#include "lora_app.h"

/**
 * This callback function is called when a submenu item is clicked.
 * 
 * @param context The context passed to the submenu.
 * @param index   The index of the submenu item that was clicked.
*/
static void lora_submenu_item_callback(void *context, uint32_t index)
{
    LoraApp *app = context;
    switch (index) {
    case 0:
        setup_lora_connexion(app);
        otaa_join_procedure(app);
        // scene_manager_next_scene(app->scene_manager, LoraSceneLorawan);
        break;
    case 1:
        enter_rx_mode(app);
        // scene_manager_next_scene(app->scene_manager, LoraSceneReceiveMode);
        break;
    case 2:
        send_cmsg(app, "Hello World");
        // scene_manager_next_scene(app->scene_manager, LoraSceneLorawan);
        break;
    default:
        break;
    }
}

/**
 * Adds the default submenu entries.
 * 
 * @param submenu The submenu.
 * @param context The context to pass to the submenu item callback function.
*/
static void lora_submenu_add_default_entries(Submenu *submenu,
                                             void *context)
{
    LoraApp *app = context;
    submenu_reset(submenu);
    submenu_add_item(submenu, "OTAA join", 0, lora_submenu_item_callback,
                     context);
    submenu_add_item(submenu, "Reiceive Mode", 1,
                     lora_submenu_item_callback, context);
    submenu_add_item(submenu, "Send Msg", 2, lora_submenu_item_callback,
                     context);

    app->index = 3;
}

void lora_scene_start_on_enter(void *context)
{
    LoraApp *app = context;
    lora_submenu_add_default_entries(app->submenu, app);
    view_dispatcher_switch_to_view(app->view_dispatcher,
                                   LoraAppSubMenuView);
}

void lora_scene_start_on_exit(void *context)
{
    LoraApp *app = context;
    submenu_reset(app->submenu);
}

bool lora_scene_start_on_event(void *context, SceneManagerEvent event)
{
    UNUSED(context);
    bool consumed = false;
    if (event.type == SceneManagerEventTypeCustom) {
        // Handle custom events here

    }
    return consumed;
}
