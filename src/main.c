/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"

#include "BLE_PARAM_CONFIG.h"

#include "wifi_common.h"

#include "uart_console.h"

// LOG表示用TAG(関数名にしておく)
#define     TAG             __func__

// ================================================================================================
// メインルーチン
// ================================================================================================
void app_main(void)
{
    esp_err_t err;

    ESP_LOGI(TAG, "==== application start ====================");
    // NVS初期化
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    
    printf("==== Loading Params ====================\n");
    bool param_available = LoadParam(&AppParam);
    DispParam(&AppParam);

    bool    enter_ble_main = false;
    if (!param_available) {            // パラメータロードが失敗した or 5秒以内にキー入力があればBLEによる設定モードで動作
        printf("    ==== parameter load failed! ====\n");
        enter_ble_main = true;
    }
    else {
        printf("    ==== enter to setting mode? ====\n");
        if (uart_checkkey(50)) {            // パラメータロードが失敗した or 5秒以内にキー入力があればBLEによる設定モードで動作
            enter_ble_main = true;
        }
    }

    if (enter_ble_main) {
        // BLE処理
        ble_main();
    }

    // 本来のmain処理
    DispParam(&AppParam);

    // Wi-Fi 接続
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    err = wifi_init_sta(AppParam.ssid_name, AppParam.ssid_pass);
    if (err != ESP_OK) {
        // Wi-Fi初期化失敗
        ESP_LOGE(TAG, "wifi_init_sta failed.");
        return;
    }
    // 接続待ち
    wait_wifi_connect();

    // 必要ならwait_wifi_connect()の戻り値で接続できたか確認する

    // 本来の接続後の処理
    // 今回は何もやることがないので、リブート待ちしておく
    printf("Hit 'r' key for system reboot... \n");
    while (1) {
        int in_key = uart_getchar_nowait();
        switch (in_key) {
          case 'r' :
            // rが入力されたらreboot
            esp_restart();
            break;
        }
        
        // Task watchdog のトリガを防止するため、vTaskDelay()をコールしておく
        vTaskDelay(1000 / portTICK_PERIOD_MS);          // 1秒待つ
    }

    return;
}
