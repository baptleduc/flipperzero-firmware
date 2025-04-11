#include "lora_scene.h"
#include "lora_app.h"




void lora_scene_receive_mode_on_enter(void *context)
{
    FURI_LOG_D("LoraSceneReceiveMode", "Entering Lora Scene Receive Mode");
    LoraApp *app = context;
    text_box_set_font(app->text_box, TextBoxFontHex);
    view_dispatcher_switch_to_view(app->view_dispatcher,
                                   LoraAppTextBoxView);
}



bool lora_scene_receive_mode_on_event(void *context,
                                      SceneManagerEvent event)
{
    LoraApp *app = context;
    bool consumed = false;
    FURI_LOG_D("lora_scene_receive_mode_on_event", "Event type: %d",
               event.type);
    FURI_LOG_D("lora_scene_receive_mode_on_event", "Event: %ld",
               event.event);
    if (event.type == SceneManagerEventTypeCustom) {
        if (event.event == LoraCustomEventRxResponse) {
            // Display the received message
            text_box_set_text(app->text_box,
                              app->msg_response->decoded_data);
        }
    }

    return consumed;
}

void lora_scene_receive_mode_on_exit(void *context)
{
    LoraApp *app = context;
    text_box_reset(app->text_box);
}
