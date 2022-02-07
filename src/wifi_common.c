/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>

#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi.h"

#include "wifi_common.h"

// LOG表示用TAG(関数名にしておく)
#define     TAG             __func__

// ================================================================================================
// AP接続最大リトライ回数
#define EXAMPLE_ESP_MAXIMUM_RETRY  5

// イベントビット
#define     WIFI_CONNECTED_BIT      BIT0
#define     WIFI_FAIL_BIT           BIT1


// ================================================================================================
// Wi-Fi接続時のイベントグループ
static EventGroupHandle_t s_wifi_event_group;

// イベントハンドラ
static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;


// 自身に割り当てられたIPアドレス
esp_ip4_addr_t my_ipaddr;


// ================================================================================================
// Wi-Fi/IPのイベントハンドラ
// ================================================================================================
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    // 接続リトライ回数
    static int s_retry_num = 0;

    if (event_base == WIFI_EVENT) {
        switch  (event_id) {
          case WIFI_EVENT_STA_START :                  // STARTイベント
            ESP_LOGI(TAG, "connect to the AP");
            esp_wifi_connect();         // 接続開始
            break;
          case WIFI_EVENT_STA_CONNECTED :               // CONNECTEDイベント
            {
                // wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*) event_data;      // SSID名などが得られる
                
                ESP_LOGI(TAG, "Wi-Fi connected");
                // ここではまだIPアドレスが取得できていないので何もしない
            }
            break;
          case WIFI_EVENT_STA_DISCONNECTED :            // DISCONNECTEDイベント
            {
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;      // SSID名などが得られる
                if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
                    // リトライ回数に達していない → リトライ
                    s_retry_num++;
                    ESP_LOGI(TAG, "retry to connect to the AP  (%d)   reason:0x%02x", s_retry_num, event->reason);      // reasonの型は enum wifi_err_reason_t かな?
                    esp_wifi_connect();         // 再度 接続開始
                } else {
                    // リトライ回数に達した → エラー終了
                    ESP_LOGI(TAG, "connect to the AP fail");
                    // 接続失敗を通知
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                }
            }
            break;
          default :
            ESP_LOGI(TAG, "UNKNOWN EVENT event_base: WIFI_EVENT   event_id: %d", event_id);
            break;
        }
    } else if (event_base == IP_EVENT) {
        switch  (event_id) {
          case IP_EVENT_STA_GOT_IP:                     // IPアドレス取得完了イベント
            {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
                my_ipaddr = event->ip_info.ip;                  // 自身に割り当てられたIPアドレスを記憶しておく
                s_retry_num = 0;
                // 接続成功を通知
                xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            }
            break;
          default :
            ESP_LOGI(TAG, "UNKNOWN EVENT event_base: IP_EVENT   event_id: %d", event_id);
            break;
        }
    }
    else {
        // それ以外は発生しないはずだが念のため
        ESP_LOGI(TAG, "UNKNOWN EVENT event_base: %s   event_id: %d", event_base, event_id);
    }

    return;
}

// ================================================================================================
// wi-fi 接続待ち
// ================================================================================================
esp_err_t wait_wifi_connect(void)
{
    esp_err_t   ret = ESP_FAIL;
    // Wi-Fi/IPのイベントハンドラで接続成功(WIFI_CONNECTED_BIT)/接続失敗(WIFI_FAIL_BIT)がセットされるのを待つ
    EventBits_t bits = xEventGroupWaitBits(             // 戻り値はフラグパターン
            s_wifi_event_group,                         // イベントグループ
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,         // 待ちパターン
            pdFALSE,                                    // 条件成立時にClearしない  (pdTRUEだとクリアする)
            pdFALSE,                                    // OR待ち                   (pdTRUEだとAND待ち)
            portMAX_DELAY);                             // 待ち時間最大値           (≒永久待ち)

    // フラグパターンから戻り値に変換
    if (bits & WIFI_CONNECTED_BIT) {
        ret = ESP_OK;
        ESP_LOGI(TAG, "success to connect");
    } else if (bits & WIFI_FAIL_BIT) {
        ret = ESP_FAIL;
        ESP_LOGI(TAG, "Failed to connect");
    } else {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    // イベントグループ/イベントハンドラの削除(接続後はもう使わないので)
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);

    return ret;

}

// ================================================================================================
// wi-fi ステーション(STA)モード(クライアント)   初期化
// ================================================================================================
esp_err_t wifi_init_sta(char* ssid_name, char* ssid_pass)
{
    esp_err_t       err = ESP_OK;

    // イベントグループの生成
    s_wifi_event_group = xEventGroupCreate();

    // NETインタフェース初期化
    ESP_ERROR_CHECK(esp_netif_init());

    // イベントループ/イベントハンドラの初期化
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Wi-Fi パラメータ初期化
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable    = true,
                .required   = false
            },
        },
    };
    strcpy((char*)wifi_config.sta.ssid, ssid_name);
    strcpy((char*)wifi_config.sta.password, ssid_pass);

    // Wi-Fi 設定
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    // Wi-Fi スタート
    ESP_ERROR_CHECK(esp_wifi_start() );

    // 初期化完了
    ESP_LOGI(TAG, "wifi_init_sta finished.");

    return err;
}
