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

#include    "uart_console.h"

// LOG表示用TAG(関数名にしておく)
#define     TAG             __func__

// マニファクチャデータ
uint8_t   manufacturer_data[3] = {'E', 'S', 'P'};   // 最初の2バイトがCompanyId。以下マニファクチャ固有データ
                                                    // この設定値は例としてあまり良くないかも。

// GATTインタフェース-コールバック関数対応付け用テーブル
struct gatts_profile_inst profile_tab[PROFILE_NUM] = {
    [PARAM_CONFIG_PROFILE_APP_IDX] = {
        .gatts_cb = param_config_event_handler,         // コールバック関数
        .gatts_if = ESP_GATT_IF_NONE,                   // 初期化前は ESP_GATT_IF_NONE を設定しておく
        .app_id   = ESP_PARAM_CONFIG_APP_ID,            // アプリケーションID(未使用)
    },
};

// ================================================================================================
// メインルーチン
// ================================================================================================
void ble_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "==== init bluetooth ====================");

    // Bluetooth classicモードのメモリ解放
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // コントローラ初期化
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();        // コンフィグレーション構造体の初期化
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s init controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // コントローラの有効化
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // プロトコルスタックの初期化
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // プロトコルスタックの有効化
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // GATTサーバのコールバックの登録
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(TAG, "gatts register error, error code = %x", ret);
        return;
    }

    // GAPのコールバックの登録
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(TAG, "gap register error, error code = %x", ret);
        return;
    }

    // アプリケーションIDの登録
    ret = esp_ble_gatts_app_register(ESP_PARAM_CONFIG_APP_ID);
    if (ret){
        ESP_LOGE(TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    // ============================================================================================
    // ここから secure connection の設定

    // 認証リクエストモードの設定
    /*  設定可能値
          ESP_LE_AUTH_NO_BOND           No bonding.                                                         暗号化はするが、鍵の保存はしない
          ESP_LE_AUTH_BOND              Bonding is performed.
          ESP_LE_AUTH_REQ_MITM          MITM Protection is enabled.
          ESP_LE_AUTH_REQ_SC_ONLY       Secure Connections without bonding enabled.                         暗号化はするが、鍵の保存はしない
          ESP_LE_AUTH_REQ_SC_BOND       Secure Connections with bonding enabled.
          ESP_LE_AUTH_REQ_SC_MITM:      Secure Connections with MITM Protection and no bonding enabled.     暗号化はするが、鍵の保存はしない
          ESP_LE_AUTH_REQ_SC_MITM_BOND  Secure Connections with MITM Protection and bonding enabled.        デフォルト？
    */
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(esp_ble_auth_req_t));

    // ペアリング時の動作＝入力なし、出力なし
    /*  設定可能値
          ESP_IO_CAP_OUT         DisplayOnly
          ESP_IO_CAP_IO          DisplayYesNo
          ESP_IO_CAP_IN          KeyboardOnly
          ESP_IO_CAP_NONE        NoInputNoOutput    大抵これかな?
          ESP_IO_CAP_KBDISP      Keyboard display   デフォルト?
    */
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(esp_ble_io_cap_t));

    // 最大Encryption Key Sizeの設定(16byte=128bit)   設定可能値： 7～16、デフォルト=16
    uint8_t key_size = 16;
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));

    // Initiator Key の生成/配布
    /* 設定可能値
      ESP_BLE_ENC_KEY_MASK:    Used to exchange the encryption key in the init key & response key.
      ESP_BLE_ID_KEY_MASK:     Used to exchange the IRK(Identity Resolving Key) key in the init key & response key.
      ESP_BLE_CSR_KEY_MASK     Used to exchange the CSRK key in the init key & response key 
      ESP_BLE_LINK_KEY_MASK    Used to exchange the link key(this key just used in the BLE & BR/EDR coexist mode) in the init key & response key
    */
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));

    // Responder Key の生成/配布
    /* 設定可能値 同上 */
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    // 静的パスキーの設定
    uint32_t passkey = STATIC_PASSKEY;
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));

    // 認証オプションの設定
    /* 設定可能値
      ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE
      ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_ENABLE
    */
    uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));

    // OOBサポートの設定   デフォルトDISABLE
    // OOB(Out of Band)：NFC等の他の手段で鍵を交換するもの
    /* 設定可能値
      ESP_BLE_OOB_DISABLE
      ESP_BLE_OOB_ENABLE 
    */
    uint8_t oob_support = ESP_BLE_OOB_DISABLE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(uint8_t));

    // ここまで secure connection の設定
    // ============================================================================================
    ESP_LOGI(TAG, "==== end of BLE setting ====================");

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);          // 1秒待つ
        int in_key = uart_getchar_nowait();
        if (in_key == 'q') {
            // qが入力されたらループを抜ける
            break;
        }
        else if (in_key == 'd') {
            // dが入力されたら切断する(接続されているかはcall先でチェック)
            param_config_disconnect();
        }
        else if (in_key == 'p') {
            // pが入力されたら変数一覧を表示
            DispParam(&AppParam);
        }
    }
    ESP_LOGI(TAG, "==== Escaped from the loop ====================");

    // 切断する(接続されているかはcall先でチェック)
    param_config_disconnect();

    // Advertising 停止
    ESP_LOGI(TAG, "==== Stop advertising ====================");
    stop_advertising();

    // 後処理
    printf("Hit 'L' key for list bonded devices, \n");
    printf("Hit 'C' key for clear bonded devices, \n");
    printf("Hit 'p' key for print AppParam, \n");
    printf("Hit 's' key for Save  AppParam, \n");
    printf("Hit 'c' key for Clear AppParam, \n");
    printf("Hit 'r' key for system reboot... \n");
    printf("Hit 'q' key for go to main processing \n");

    bool    term_flag = false;
    while (1) {
        // 終了後の無限ループ
        int in_key = uart_getchar_nowait();
        switch (in_key) {
          case 'q' :
            // qが入力されたらループを抜ける
            term_flag = true;
            break;
          case 'r' :
            // rが入力されたらreboot
            esp_restart();
            break;
          case 'p' :
            // pが入力されたら変数一覧を表示
            DispParam(&AppParam);
            break;
          case 's' :
            // sが入力されたら変数をnvsに保存
            SaveParam(&AppParam);
            break;
          case 'c' :
            // cが入力されたらnvs上の変数を削除
            ClearParam();
            LoadParam(&AppParam);
            break;
          case 'L' :
            // ボンディング済みデバイスを表示
            show_bonded_devices();
            break;
          case 'C' :
            // ボンディング済みデバイスをすべて削除
            remove_all_bonded_devices();
            break;
        }
        if (term_flag) {
            // 終了フラグ
            break;
        }

        // Task watchdog のトリガを防止するため、vTaskDelay()をコールしておく
        vTaskDelay(1000 / portTICK_PERIOD_MS);          // 1秒待つ
    }

    return;
}
