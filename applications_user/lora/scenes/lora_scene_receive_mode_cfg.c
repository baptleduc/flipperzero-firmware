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

static void line_bw_cb(VariableItem *item)
{
    LoraApp *app = variable_item_get_context(item);
    furi_assert(app);
    char temp[4];
    uint32_t new_bw =
        bandwidth_list[variable_item_get_current_value_index(item)];
    snprintf(temp, sizeof(temp), "%lu", new_bw);
    variable_item_set_current_value_text(item, temp);
    with_view_model(app->receiver->view, LoraReceiverModel * model, {
                    model->config.bw_idx = new_bw;}
                    , false);
}

static void line_bw(LoraApp *app, LoraReceiverModel *model)
{
    VariableItem *item;
    item = variable_item_list_add(app->var_item_list,
                                  "Band width",
                                  BANDWIDTH_LIST_SIZE, line_bw_cb, app);
    variable_item_set_current_value_index(item, model->config.bw_idx);
    char temp[4];
    snprintf(temp, sizeof(temp), "%lu",
             bandwidth_list[model->config.bw_idx]);
    variable_item_set_current_value_text(item, temp);
}

static void line_tx_preamble_cb(VariableItem *item)
{
    LoraApp *app = variable_item_get_context(item);
    furi_assert(app);
    char temp[4];
    uint8_t new_preamble =
        variable_item_get_current_value_index(item) + MIN_PREAMBLE;
    snprintf(temp, sizeof(temp), "%u", new_preamble);
    variable_item_set_current_value_text(item, temp);
    with_view_model(app->receiver->view, LoraReceiverModel * model, {
                    model->config.tx_preamble = new_preamble;}
                    , false);
}

static void line_tx_preamble(LoraApp *app, LoraReceiverModel *model)
{
    VariableItem *item;
    item = variable_item_list_add(app->var_item_list, "TX Preamble",
                                  MAX_PREAMBLE - MIN_PREAMBLE + 1,
                                  line_tx_preamble_cb, app);
    variable_item_set_current_value_index(item,
                                          model->config.tx_preamble -
                                          MIN_PREAMBLE);
    char temp[4];
    snprintf(temp, sizeof(temp), "%u", model->config.tx_preamble);
    variable_item_set_current_value_text(item, temp);
}

static void line_rx_preamble_cb(VariableItem *item)
{
    LoraApp *app = variable_item_get_context(item);
    furi_assert(app);
    char temp[4];
    uint8_t new_preamble =
        variable_item_get_current_value_index(item) + MIN_PREAMBLE;
    snprintf(temp, sizeof(temp), "%u", new_preamble);
    variable_item_set_current_value_text(item, temp);
    with_view_model(app->receiver->view, LoraReceiverModel * model, {
                    model->config.rx_preamble = new_preamble;}
                    , false);
}

static void line_rx_preamble(LoraApp *app, LoraReceiverModel *model)
{
    VariableItem *item;
    item = variable_item_list_add(app->var_item_list, "RX Preamble",
                                  MAX_PREAMBLE - MIN_PREAMBLE + 1,
                                  line_rx_preamble_cb, app);
    variable_item_set_current_value_index(item,
                                          model->config.rx_preamble -
                                          MIN_PREAMBLE);
    char temp[4];
    snprintf(temp, sizeof(temp), "%u", model->config.rx_preamble);
    variable_item_set_current_value_text(item, temp);
}

static void line_power_cb(VariableItem *item)
{
    LoraApp *app = variable_item_get_context(item);
    furi_assert(app);
    char temp[4];
    uint8_t new_power =
        variable_item_get_current_value_index(item) + MIN_POWER;
    snprintf(temp, sizeof(temp), "%u", new_power);
    variable_item_set_current_value_text(item, temp);
    with_view_model(app->receiver->view, LoraReceiverModel * model, {
                    model->config.power = new_power;}
                    , false);
}

static void line_power(LoraApp *app, LoraReceiverModel *model)
{
    VariableItem *item;
    item = variable_item_list_add(app->var_item_list, "Power",
                                  MAX_POWER - MIN_POWER + 1,
                                  line_power_cb, app);
    variable_item_set_current_value_index(item,
                                          model->config.power - MIN_POWER);
    char temp[4];
    snprintf(temp, sizeof(temp), "%u", model->config.power);
    variable_item_set_current_value_text(item, temp);
}

static void line_crc_cb(VariableItem *item)
{
    LoraApp *app = variable_item_get_context(item);
    furi_assert(app);
    char temp[10];
    bool new_crc = variable_item_get_current_value_index(item);
    snprintf(temp, sizeof(temp), "%s", new_crc ? "Enabled" : "Disabled");
    variable_item_set_current_value_text(item, temp);
    with_view_model(app->receiver->view, LoraReceiverModel * model, {
                    model->config.with_crc = new_crc;}
                    , false);
}

static void line_crc(LoraApp *app, LoraReceiverModel *model)
{
    VariableItem *item;
    item = variable_item_list_add(app->var_item_list, "CRC",
                                  2, line_crc_cb, app);
    variable_item_set_current_value_index(item, model->config.with_crc);
    char temp[10];
    snprintf(temp, sizeof(temp), "%s",
             model->config.with_crc ? "Enabled" : "Disabled");
    variable_item_set_current_value_text(item, temp);
}


static void line_iq_inverted_cb(VariableItem *item)
{
    LoraApp *app = variable_item_get_context(item);
    furi_assert(app);
    char temp[10];
    bool new_iq_inverted = variable_item_get_current_value_index(item);
    snprintf(temp, sizeof(temp), "%s",
             new_iq_inverted ? "Enabled" : "Disabled");
    variable_item_set_current_value_text(item, temp);
    with_view_model(app->receiver->view, LoraReceiverModel * model, {
                    model->config.is_iq_inverted = new_iq_inverted;}
                    , false);
}

static void line_iq_inverted(LoraApp *app, LoraReceiverModel *model)
{
    VariableItem *item;
    item = variable_item_list_add(app->var_item_list, "IQ Inverted",
                                  2, line_iq_inverted_cb, app);
    variable_item_set_current_value_index(item,
                                          model->config.is_iq_inverted);
    char temp[10];
    snprintf(temp, sizeof(temp), "%s",
             model->config.is_iq_inverted ? "Enabled" : "Disabled");
    variable_item_set_current_value_text(item, temp);
}

static void line_public_lorawan_cb(VariableItem *item)
{
    LoraApp *app = variable_item_get_context(item);
    furi_assert(app);
    char temp[10];
    bool new_with_public_lorawan =
        variable_item_get_current_value_index(item);
    snprintf(temp, sizeof(temp), "%s",
             new_with_public_lorawan ? "Enabled" : "Disabled");
    variable_item_set_current_value_text(item, temp);
    with_view_model(app->receiver->view, LoraReceiverModel * model, {
                    model->config.with_public_lorawan =
                    new_with_public_lorawan;}
                    , false);
}

static void line_public_lorawan(LoraApp *app, LoraReceiverModel *model)
{
    VariableItem *item;
    item = variable_item_list_add(app->var_item_list, "Public LoRaWan",
                                  2, line_public_lorawan_cb, app);
    variable_item_set_current_value_index(item,
                                          model->config.
                                          with_public_lorawan);
    char temp[10];
    snprintf(temp, sizeof(temp), "%s",
             model->config.with_public_lorawan ? "Enabled" : "Disabled");
    variable_item_set_current_value_text(item, temp);
}


void lora_scene_receive_mode_cfg_on_enter(void *context)
{
    FURI_LOG_D("LoraSceneReceiveModeCfg",
               "Entering Lora Scene Receive Mode");
    LoraApp *app = context;
    with_view_model(app->receiver->view, LoraReceiverModel * model, {
                    line_sf(app, model);
                    line_bw(app, model);
                    line_tx_preamble(app, model);
                    line_rx_preamble(app, model);
                    line_power(app, model);
                    line_crc(app, model);
                    line_iq_inverted(app, model);
                    line_public_lorawan(app, model);}, false);
    view_dispatcher_switch_to_view(app->view_dispatcher,
                                   LoraAppReceiverCfgView);
}

void lora_scene_receive_mode_cfg_on_exit(void *context)
{
    LoraApp *app = context;
    variable_item_list_reset(app->var_item_list);
}

bool lora_scene_receive_mode_cfg_on_event(void *context,
                                          SceneManagerEvent event)
{
    UNUSED(context);
    UNUSED(event);
    return false;
}
