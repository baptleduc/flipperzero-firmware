#include "lora_scene.h"
#include "lora_app.h"
#include "../views/lora_receiver_i.h"
#include <gui/modules/variable_item_list.h>

static void line_sf_cb(VariableItem *item)
{
    LoraApp *app = variable_item_get_context(item);
    furi_assert(app);
    char temp[4];
    uint8_t new_sf = variable_item_get_current_value_index(item) + MIN_SF;
    snprintf(temp, sizeof(temp), "%u", new_sf);
    variable_item_set_current_value_text(item, temp);
    with_view_model(app->receiver->view, LoraReceiverModel * model, {
                    model->config.sf = new_sf;}
                    , false);
}

static void line_sf(LoraApp *app, LoraReceiverModel *model)
{
    VariableItem *item;
    item =
        variable_item_list_add(app->var_item_list, "SF",
                               MAX_SF - MIN_SF + 1, line_sf_cb, app);
    variable_item_set_current_value_index(item, model->config.sf - MIN_SF); // config.sf - MIN_SF because we map the SF to the index
    char temp[4];
    snprintf(temp, sizeof(temp), "%u", model->config.sf);
    variable_item_set_current_value_text(item, temp);
}


void lora_scene_receive_mode_cfg_on_enter(void *context)
{
    FURI_LOG_D("LoraSceneReceiveModeCfg",
               "Entering Lora Scene Receive Mode");
    LoraApp *app = context;
    with_view_model(app->receiver->view, LoraReceiverModel * model, {
                    line_sf(app, model);}, false)
        view_dispatcher_switch_to_view(app->view_dispatcher,
                                       LoraAppReceiverCfgView);
}

void lora_scene_receive_mode_cfg_on_exit(void *context)
{
    LoraApp *app = context;
    UNUSED(app);
}

bool lora_scene_receive_mode_cfg_on_event(void *context,
                                          SceneManagerEvent event)
{
    UNUSED(context);
    UNUSED(event);
    return false;
}
