#include "lora_scene.h"
#include "lora_app.h"




void lora_scene_receive_mode_on_enter(void *context)
{
    // UNUSED(context);
    FURI_LOG_D("LoraSceneReceiveMode", "Entering Lora Scene Receive Mode");
    LoraApp *app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher,
                                   LoraAppSubMenuView);
}



bool lora_scene_receive_mode_on_event(void *context,
                                      SceneManagerEvent event)
{
    UNUSED(context);
    UNUSED(event);
    return false;
}

void lora_scene_receive_mode_on_exit(void *context)
{
    UNUSED(context);
    // LoraApp *app = context;
    // scene_manager_previous_scene(app->scene_manager);
}
