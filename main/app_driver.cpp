/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "soc/gpio_num.h"
#include "esp_timer.h"
#include <esp_log.h>
#include "support/CodeUtils.h"

#include <esp_matter.h>

#include <app_priv.h>
#include <button_gpio.h>
#include <iot_button.h>

using namespace chip::app::Clusters;
using namespace esp_matter;

static const char *TAG = "app_driver";

static const gpio_num_t k_gpio_in = (gpio_num_t)CONFIG_COFFEE_MAKER_GPIO_IN;
static const gpio_num_t k_gpio_out = (gpio_num_t)CONFIG_COFFEE_MAKER_GPIO_OUT;

static esp_timer_handle_t s_pulse_off_timer = NULL;
static esp_timer_handle_t s_verify_timer = NULL;
static bool s_pulse_in_flight = false;
static bool s_last_commanded = false;

static void verify_timer_cb(void *arg)
{
    bool physical_state = gpio_get_level(k_gpio_in) != 0;
    if (physical_state != s_last_commanded) {
        ESP_LOGE(TAG, "Coffee maker did not respond to toggle pulse, correcting OnOff attribute to match GPIO IN (%d)",
                 physical_state);
        esp_matter_attr_val_t val = esp_matter_bool(physical_state);
        attribute::update(coffee_maker_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &val);
    }
}

static void pulse_off_timer_cb(void *arg)
{
    gpio_set_level(k_gpio_out, 0);
    s_pulse_in_flight = false;
    esp_timer_start_once(s_verify_timer, (uint64_t)CONFIG_COFFEE_MAKER_VERIFY_DELAY_MS * 1000ULL);
}

esp_err_t app_driver_coffee_maker_init()
{
    esp_err_t err = ESP_OK;

    gpio_reset_pin(k_gpio_out);
    err = gpio_set_direction(k_gpio_out, GPIO_MODE_OUTPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to set GPIO OUTPUT mode for GPIO OUT pin %d", k_gpio_out);
        return err;
    }
    err = gpio_set_level(k_gpio_out, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to set idle level for GPIO OUT pin %d", k_gpio_out);
        return err;
    }

    gpio_reset_pin(k_gpio_in);
    err = gpio_set_direction(k_gpio_in, GPIO_MODE_INPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to set GPIO INPUT mode for GPIO IN pin %d", k_gpio_in);
        return err;
    }

    const esp_timer_create_args_t pulse_off_args = {
        .callback = &pulse_off_timer_cb,
        .name = "coffee_maker_pulse_off",
    };
    err = esp_timer_create(&pulse_off_args, &s_pulse_off_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create pulse-off timer");
        return err;
    }

    const esp_timer_create_args_t verify_args = {
        .callback = &verify_timer_cb,
        .name = "coffee_maker_verify",
    };
    err = esp_timer_create(&verify_args, &s_verify_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create verify timer");
        return err;
    }

    return ESP_OK;
}

bool app_driver_coffee_maker_get_initial_state()
{
    return gpio_get_level(k_gpio_in) != 0;
}

static void status_input_cb(void *arg, void *data)
{
    bool state = (bool)(intptr_t)data;
    ESP_LOGI(TAG, "Coffee maker status input changed: %s", state ? "on" : "off");
    esp_matter_attr_val_t val = esp_matter_bool(state);
    attribute::update(coffee_maker_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &val);
}

app_driver_handle_t app_driver_status_input_init()
{
    button_handle_t handle = NULL;
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = k_gpio_in,
        .active_level = 1,
    };

    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create status input device");
        return NULL;
    }

    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, NULL, status_input_cb, (void *)(intptr_t)true);
    iot_button_register_cb(handle, BUTTON_PRESS_UP, NULL, status_input_cb, (void *)(intptr_t)false);
    return (app_driver_handle_t)handle;
}

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    if (cluster_id != OnOff::Id || attribute_id != OnOff::Attributes::OnOff::Id) {
        return ESP_OK;
    }

    bool physical_state = gpio_get_level(k_gpio_in) != 0;
    if (val->val.b == physical_state) {
        ESP_LOGI(TAG, "Coffee maker already %s, ignoring redundant command", physical_state ? "on" : "off");
        return ESP_OK;
    }

    if (s_pulse_in_flight) {
        ESP_LOGW(TAG, "Toggle pulse already in flight, dropping redundant command");
        return ESP_OK;
    }

    s_last_commanded = val->val.b;
    s_pulse_in_flight = true;
    gpio_set_level(k_gpio_out, 1);
    esp_timer_start_once(s_pulse_off_timer, (uint64_t)CONFIG_COFFEE_MAKER_TOGGLE_PULSE_MS * 1000ULL);
    return ESP_OK;
}

app_driver_handle_t app_driver_button_init(gpio_num_t * reset_gpio)
{
    VerifyOrReturnValue((reset_gpio), (app_driver_handle_t)NULL, ESP_LOGE(TAG, "reset_gpio cannot be NULL"));
    *reset_gpio = (gpio_num_t)CONFIG_USER_BUTTON_GPIO;
    ESP_LOGI(TAG, "Initializing reset button with gpio pin %d ...", (int)*reset_gpio);

    if (*reset_gpio == k_gpio_in || *reset_gpio == k_gpio_out) {
        ESP_LOGE(TAG, "Reset button gpio pin %d conflicts with coffee maker GPIO IN/OUT", (int)*reset_gpio);
        *reset_gpio = gpio_num_t::GPIO_NUM_NC;
        return (app_driver_handle_t)NULL;
    }

    button_handle_t reset_handle = NULL;
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = CONFIG_USER_BUTTON_GPIO,
        .active_level = CONFIG_USER_BUTTON_LEVEL,
    };

    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &reset_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
        return NULL;
    }

    if (!reset_handle) {
        *reset_gpio = gpio_num_t::GPIO_NUM_NC;
    }
    return (app_driver_handle_t)reset_handle;
}
