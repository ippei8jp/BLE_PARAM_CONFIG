/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

// ==== extern 宣言 ===========================================================================================
extern void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
extern void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
extern esp_err_t start_advertising(void);
extern esp_err_t stop_advertising(void);
extern void remove_all_bonded_devices(void);
extern void show_bonded_devices(void);

extern char *addr_type_to_str(uint8_t addr_type);
extern esp_ble_adv_params_t adv_params;
