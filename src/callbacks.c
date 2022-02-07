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
#include "esp_bt_device.h"              // esp_bt_dev_get_address()使用のため

#include "BLE_PARAM_CONFIG.h"

#include    "uart_console.h"

// LOG表示用TAG(関数名にしておく)
#define     TAG             __func__

// ==== プロトタイプ宣言 ======================================================================================
static char *esp_key_type_to_str(esp_ble_key_type_t key_type);
static char *esp_auth_req_to_str(esp_ble_auth_req_t auth_req);
static char *esp_bt_gap_event_to_str(esp_gap_ble_cb_event_t event);
static char *esp_bt_gatts_event_to_str(esp_gatts_cb_event_t event);

// ==== 外部変数 ======================================================================================
// コンフィギュレーション済みフラグ
static bool scan_rsp_config_done    = false;
static bool adv_config_done         = false;

// advertising configuration データ
static esp_ble_adv_data_t adv_config = {
    .set_scan_rsp           = false,                // advertising configuration データ
    .include_txpower        = true,                 // TX power を含む
    .min_interval           = 0x0006,               // advertising data最小送信間隔  Time = min_interval * 1.25 msec
    .max_interval           = 0x0010,               // advertising data最大送信間隔  Time = max_interval * 1.25 msec
    .appearance             = 0x00,                 // appearance value 設定するときは↓を参照
                                                    // https://specificationrefs.bluetooth.com/assigned-values/Appearance%20Values.pdf
    .manufacturer_len       = 0,                    // マニファクチャデータ長
    .p_manufacturer_data    =  NULL,                // マニファクチャデータへのポインタ
    .service_data_len       = 0,                    // サービスデータ長
    .p_service_data         = NULL,                 // サービスデータへのポインタ (たぶん、iBeacon等のデータ設定で使う)
    .service_uuid_len       = sizeof(service_uuid),     // サービスUUID長
    .p_service_uuid         = (uint8_t*)service_uuid,   // サービスUUIDへのポインタ
    .flag                   = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
                                                    // advertising flag設定値(advertisingパットに含まれて送信される)
};

// scan response configuration データ
static esp_ble_adv_data_t scan_rsp_config = {
    .set_scan_rsp           = true,                         // scan response configuration データ
    .include_name           = true,                         // パケットにDeviceNameを含める (DeviceNameはesp_ble_gap_set_device_name()で設定)
    .manufacturer_len       = sizeof(manufacturer_data),    // マニファクチャデータ長
    .p_manufacturer_data    = manufacturer_data,            // マニファクチャデータへのポインタ
};

// ？？？ =============================================
// advertising と scan response どっちに割り当てるかはどうやって決める？
// ====================================================


// advertisingパラメータ
esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x100,                    // advertising インターバル(最小)  Time = N * 0.625 msec
                                                    // 設定可能値： 0x0020～0x4000, デフォルト値：0x0800
    .adv_int_max        = 0x100,                    // advertising インターバル(最大)  Time = N * 0.625 msec
                                                    // 設定可能値： 0x0020～0x4000, デフォルト値：0x0800
    .adv_type           = ADV_TYPE_IND,             // advertising タイプ
                                                    // 設定可能値： 
                                                    //  ADV_TYPE_IND                大抵 コレを設定
                                                    //  ADV_TYPE_DIRECT_IND_HIGH
                                                    //  ADV_TYPE_SCAN_IND
                                                    //  ADV_TYPE_NONCONN_IND
                                                    //  ADV_TYPE_DIRECT_IND_LOW
    .own_addr_type      = BLE_ADDR_TYPE_RANDOM,     // Owner bluetooth デバイスアドレスタイプ
                                                    // 設定可能値: 以下のいずれか
                                                    //  BLE_ADDR_TYPE_PUBLIC        パブリックアドレスを使用
                                                    //  BLE_ADDR_TYPE_RANDOM        ランダムアドレスを使用
                                                    //  BLE_ADDR_TYPE_RPA_PUBLIC    ？
                                                    //  BLE_ADDR_TYPE_RPA_RANDOM    ？
    .channel_map        = ADV_CHNL_ALL,             // Advertising チャネルマップ(advertisingにどのチャネルを使うか)
                                                    // 設定可能値: 以下の値の論理和
                                                    //  ADV_CHNL_37     アドバータイズチャネル37chを使用
                                                    //  ADV_CHNL_38     アドバータイズチャネル38chを使用
                                                    //  ADV_CHNL_39     アドバータイズチャネル39chを使用
                                                    //  ADV_CHNL_ALL    上記すべての論理和と同じ                大抵コレかな？
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,     // Advertising フィルタポリシー
                                                    // 設定可能値
                                                    //  ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY       すべてのスキャンリクエストと接続要求を許可    大抵 コレかな？
                                                    //  ADV_FILTER_ALLOW_SCAN_WLST_CON_ANY      ホワイトリストデバイスからのスキャンリクエストを許可、すべての接続要求を許可
                                                    //  ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST      すべてのスキャンリクエストを許可、ホワイトリストデバイスからの接続要求を許可
                                                    //  ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST     ホワイトリストデバイスからのスキャンリクエストと接続要求を許可
};


// ================================================================================================
// GAP(Generic Access Profile)のコールバック
// (大雑把に言うと、advertisingまわりの処理)
// ================================================================================================
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    ESP_LOGV(TAG, "* GAP_EVT: %s(%d)", esp_bt_gap_event_to_str(event), event);

    switch (event) {
      case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:  // scan response data 設定完了
        scan_rsp_config_done = true;
        if (scan_rsp_config_done &&  adv_config_done) { // scan response data と advertising data の両方が設定完了している?
            start_advertising();                        // advertising 開始
        }
        break;
      case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:       // advertising data 設定完了
        adv_config_done = true;
        if (scan_rsp_config_done &&  adv_config_done) { // scan response data と advertising data の両方が設定完了している?
            start_advertising();                        // advertising 開始
        }
        break;
      case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:          // advertising 開始完了
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {    // 正常に開始出来ていない?
            ESP_LOGE(TAG, "    advertising start failed, error status = %x", param->adv_start_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "    advertising start success");
        {
            esp_bd_addr_t bd_addr;
            uint8_t       addr_type;
            if (ESP_OK  == esp_ble_gap_get_local_used_addr(bd_addr, &addr_type)) {
                char*   addr_type_str = addr_type_to_str(addr_type);
                ESP_LOGI(TAG, "    local BD_ADDR: %02x:%02x:%02x:%02x:%02x:%02x, %s",                  // 自分のBDアドレスの表示
                        bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3], bd_addr[4], bd_addr[5], addr_type_str);
            }
        }
        // debug： public address と比較してランダムアドレスが使われていることを確認してみる
        // const uint8_t*    pub_addr = esp_bt_dev_get_address();
        // ESP_LOGI(TAG, "    public BD_ADDR: %02x:%02x:%02x:%02x:%02x:%02x",                      // 自分のBD publicアドレスの表示
        //         pub_addr[0], pub_addr[1], pub_addr[2], pub_addr[3], pub_addr[4], pub_addr[5]);
        break;
      case ESP_GAP_BLE_PASSKEY_REQ_EVT:                 // passkey 要求
        // KeyboardOnly(ESP_IO_CAP_IN)のときに発生する
        ESP_LOGI(TAG, "    ==== ESP_GAP_BLE_PASSKEY_REQ_EVT ====");
        // 以下の関数で相手側に表示されたパスキーを返す
        uint32_t    passkey;
        char        passkey_buff[16];
        int         passkey_len;
        do {
            printf("**** input paskey : ");
            fflush(stdout);
            passkey_len = uart_gets(passkey_buff, sizeof(passkey_buff));
        } while (passkey_len == 0);
        passkey = (uint32_t)strtol(passkey_buff, NULL, 10);
        esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, passkey);
        break;
      case ESP_GAP_BLE_OOB_REQ_EVT:                     // OOB(Out of Band) 要求
        // 今回はここに来ないはず
        ESP_LOGI(TAG, "    ==== ESP_GAP_BLE_OOB_REQ_EVT ====");
        // uint8_t tk[16];
        // tkにNFC等の他の手段で交換した鍵を格納
        // esp_ble_oob_req_reply(param->ble_security.ble_req.bd_addr, tk, sizeof(tk));
        break;
      case ESP_GAP_BLE_LOCAL_IR_EVT:                    // IRK(Identity Resolving Key;共有鍵)に関するeventらしい
        ESP_LOGI(TAG, "    ==== ESP_GAP_BLE_LOCAL_IR_EVT ====");
        break;
      case ESP_GAP_BLE_LOCAL_ER_EVT:                    // LTK(Encryption key)に関するeventらしい
        ESP_LOGI(TAG, "    ==== ESP_GAP_BLE_LOCAL_ER_EVT ====");
        break;
      case ESP_GAP_BLE_NC_REQ_EVT:                      // 数値比較リクエスト イベント
        // DisplayYesNo(ESP_IO_CAP_IO)のときに発生する
        ESP_LOGI(TAG, "    ==== ESP_GAP_BLE_NC_REQ_EVT ====");
        printf("**** the passkey Notify number:%d\n", param->ble_security.key_notif.passkey);
        printf("**** Accept? (y/n) : ");
        fflush(stdout);
        int kb_key = uart_getchar();
        printf("%c\n", kb_key);
        if (kb_key == 'y' || kb_key == 'Y') {
            // 接続受け入れ
            esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
        }
        else {
            // 接続拒否
            esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, false);
        }
        break;
      case ESP_GAP_BLE_SEC_REQ_EVT:                     // セキュリティリクエスト イベント
        // 相手側から暗号化開始要求が送られてきた？
        ESP_LOGI(TAG, "    ==== ESP_GAP_BLE_SEC_REQ_EVT ====");
        // acceptする(拒否する場合はパラメータにfalseを指定する)
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
      case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:               // passkey通知要求イベント
        // 自身がDisplayOnly(ESP_IO_CAP_OUT)のときに発生する
        ESP_LOGI(TAG, "    ==== ESP_GAP_BLE_PASSKEY_NOTIF_EVT ====");
        // passkeyの表示
        ESP_LOGI(TAG, "**** The passkey Notify number:%06d\n", param->ble_security.key_notif.passkey);
        break;
      case ESP_GAP_BLE_KEY_EVT:                         // 相手側からのキーイベント
        // キータイプの表示
        ESP_LOGI(TAG, "    key type = %s", esp_key_type_to_str(param->ble_security.ble_key.key_type));
        break;
      case     ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:      // 接続パラメータ更新完了イベント
        {   // caseブロック内の先頭で変数宣言できないので、{}でブロック化
            uint8_t* bd_addr = param->update_conn_params.bda;
            ESP_LOGI(TAG, "    status = %x"
                              "    bda    = %02x:%02x:%02x:%02x:%02x:%02x"
                              "    min_int = %d"
                              "    max_int = %d"
                              "    latency = %d"
                              "    conn_int = %d"
                              "    timeout = %d",
                        param->update_conn_params.status,
                        bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3], bd_addr[4], bd_addr[5],
                        param->update_conn_params.min_int, 
                        param->update_conn_params.max_int, 
                        param->update_conn_params.latency, 
                        param->update_conn_params.conn_int, 
                        param->update_conn_params.timeout  );
            break;
        }
      case ESP_GAP_BLE_AUTH_CMPL_EVT:                   // 認証完了イベント
        {   // caseブロック内の先頭で変数宣言できないので、{}でブロック化
            uint8_t* bd_addr = param->ble_security.auth_cmpl.bd_addr;
            ESP_LOGI(TAG, "    remote BD_ADDR: %02x:%02x:%02x:%02x:%02x:%02x",                  // 相手側のBDアドレスの表示
                    bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3], bd_addr[4], bd_addr[5]);
            ESP_LOGI(TAG, "    address type = %d", param->ble_security.auth_cmpl.addr_type);    // 相手側のアドレスタイプ
            // 接続ステータス
            if(!param->ble_security.auth_cmpl.success) {
                ESP_LOGI(TAG, "    pair status = fail");
                ESP_LOGI(TAG, "    reason = 0x%x",param->ble_security.auth_cmpl.fail_reason);
            } else {
                ESP_LOGI(TAG, "    pair status = success");
                ESP_LOGI(TAG, "    auth mode = %s",esp_auth_req_to_str(param->ble_security.auth_cmpl.auth_mode));
            }
            // ボンディング済みデバイスの表示
            show_bonded_devices();
            break;
        }
      case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT:    // ボンディング済みデバイスの削除完了イベント
        {
            ESP_LOGD(TAG, "    status = %d", param->remove_bond_dev_cmpl.status);
            uint8_t* bd_addr = param->remove_bond_dev_cmpl.bd_addr;
            ESP_LOGI(TAG, "    remove BD_ADDR: %02x:%02x:%02x:%02x:%02x:%02x",                  // 削除するのBDアドレスの表示
                    bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3], bd_addr[4], bd_addr[5]);
        }
        break;
    case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT:    // プライバシー有効化/無効化完了イベント
        if (param->local_privacy_cmpl.status != ESP_BT_STATUS_SUCCESS){     // 処理本体がエラーだったら終了
            ESP_LOGE(TAG, "    config local privacy failed, error status = %x", param->local_privacy_cmpl.status);
            break;
        }
        esp_err_t ret;
        // advertising data の設定
        ret = esp_ble_gap_config_adv_data(&adv_config);
        if (ret) {
            ESP_LOGE(TAG, "    config adv data failed, error code = %x", ret);
        } else {
            adv_config_done = false;
        }
        // scan response の設定
        ret = esp_ble_gap_config_adv_data(&scan_rsp_config);
        if (ret) {
            ESP_LOGE(TAG, "    config adv data failed, error code = %x", ret);
        } else {
            scan_rsp_config_done = false;
        }
        // 両方の設定が正常終了した
        ESP_LOGI(TAG, "    success");
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:    // Advertising停止完了
        ESP_LOGI(TAG, "Advertising stop completed");
        break;
      default:
            ESP_LOGI(TAG, "    event not handled");
        break;
    }
}

// ================================================================================================
// GATT(General Agreement on Tariffs and Trade)サーバのコールバック
// ================================================================================================
void gatts_event_handler(esp_gatts_cb_event_t        event, 
                         esp_gatt_if_t               gatts_if,
                         esp_ble_gatts_cb_param_t*   param)
{
    ESP_LOGV(TAG, "- GATT_EVT: %s(%d)", esp_bt_gatts_event_to_str(event), event);

    if (event == ESP_GATTS_REG_EVT) {           // 登録イベント
        if (param->reg.status == ESP_GATT_OK) {
            // 登録処理成功
            // profile_tab[HEART_PROFILE_APP_IDX].gatts_if = gatts_if;
            profile_tab[param->reg.app_id].gatts_if = gatts_if;      // プロファイルテーブルにインタフェースを登録する
                                                                     // app_idはプロファイルテーブルのindexと同じにしてある
                                                                     // マルチプロファイルを意識して決め打ちはやらない
            ESP_LOGI(TAG, "    Reg app success,");
        } else {
            // 登録失敗
            ESP_LOGI(TAG, "    Reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }
    }

    // 各プロファイルについてループして対象のコールバックを探す
    for (int idx = 0; idx < PROFILE_NUM; idx++) {
        if (gatts_if == ESP_GATT_IF_NONE || //  ESP_GATT_IF_NONE 時は全てのプロファイルに対するコールバック呼び出し
                gatts_if == profile_tab[idx].gatts_if) {     // gatts_ifが登録済みのインタフェースと一致
            if (profile_tab[idx].gatts_cb) {                 // コールバック登録済み？
                profile_tab[idx].gatts_cb(event, gatts_if, param);   // コールバック呼び出し
            }
        }
    }
}

// ================================================================================================
// Advertising start
// ================================================================================================
esp_err_t start_advertising(void)
{
    return esp_ble_gap_start_advertising(&adv_params);  // advertising 開始
}

// ================================================================================================
// Advertising stop
// ================================================================================================
esp_err_t stop_advertising(void)
{
    return esp_ble_gap_stop_advertising();              // advertising 停止
}

// ================================================================================================
// 全ボンディング済みデバイスの接続切断
// ================================================================================================
void remove_all_bonded_devices(void)
{
    ESP_LOGI(TAG, "    -----------------------------------");
    ESP_LOGI(TAG, "    Removing all bonded devices...");

    // ボンディングデバイス数
    int dev_num = esp_ble_get_bond_device_num();

    // 情報取得用領域を確保
    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);

    // ボンディングデバイスリストを取得
    esp_ble_get_bond_device_list(&dev_num, dev_list);

    for (int i = 0; i < dev_num; i++) {
        // 各デバイスの接続切断
        esp_ble_remove_bond_device(dev_list[i].bd_addr);
    }
    ESP_LOGI(TAG, "    Done");
    ESP_LOGI(TAG, "    -----------------------------------");
    // バッファ解放
    free(dev_list);
}

// ================================================================================================
// ボンディング済みデバイスの表示(デバッグ用)
// ================================================================================================
void show_bonded_devices(void)
{
    // ボンディングデバイス数
    int dev_num = esp_ble_get_bond_device_num();

    // 情報取得用領域を確保
    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);

    // ボンディングデバイスリストを取得
    esp_ble_get_bond_device_list(&dev_num, dev_list);

    printf("    -----------------------------------\n");
    printf("    Bonded devices number : %d\n", dev_num);
    printf("    Bonded devices list :\n");
    for (int i = 0; i < dev_num; i++) {
        uint8_t* bd_addr = dev_list[i].bd_addr;
        printf("           %d :   %02x:%02x:%02x:%02x:%02x:%02x\n",    // BDアドレスの表示
                i, bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3], bd_addr[4], bd_addr[5]);
    }
    printf("    -----------------------------------\n");
    // バッファ解放
    free(dev_list);

    return;
}

// ================================================================================================
// キータイプ→文字列変換(デバッグ用)
// ================================================================================================
static char *esp_key_type_to_str(esp_ble_key_type_t key_type)
{
    char *key_str = NULL;
    switch(key_type) {
        case ESP_LE_KEY_NONE:   key_str = "ESP_LE_KEY_NONE";        break;
        case ESP_LE_KEY_PENC:   key_str = "ESP_LE_KEY_PENC";        break;
        case ESP_LE_KEY_PID:    key_str = "ESP_LE_KEY_PID";         break;
        case ESP_LE_KEY_PCSRK:  key_str = "ESP_LE_KEY_PCSRK";       break;
        case ESP_LE_KEY_PLK:    key_str = "ESP_LE_KEY_PLK";         break;
        case ESP_LE_KEY_LLK:    key_str = "ESP_LE_KEY_LLK";         break;
        case ESP_LE_KEY_LENC:   key_str = "ESP_LE_KEY_LENC";        break;
        case ESP_LE_KEY_LID:    key_str = "ESP_LE_KEY_LID";         break;
        case ESP_LE_KEY_LCSRK:  key_str = "ESP_LE_KEY_LCSRK";       break;
        default:                key_str = "INVALID BLE KEY TYPE";   break;
    }
    return key_str;
}

// ================================================================================================
// 認証リクエスト→文字列変換(デバッグ用)
// ================================================================================================
static char *esp_auth_req_to_str(esp_ble_auth_req_t auth_req)
{
    char *auth_str = NULL;
    switch(auth_req) {
        case ESP_LE_AUTH_NO_BOND:           auth_str = "ESP_LE_AUTH_NO_BOND";           break;
        case ESP_LE_AUTH_BOND:              auth_str = "ESP_LE_AUTH_BOND";              break;
        case ESP_LE_AUTH_REQ_MITM:          auth_str = "ESP_LE_AUTH_REQ_MITM";          break;
        case ESP_LE_AUTH_REQ_BOND_MITM:     auth_str = "ESP_LE_AUTH_REQ_BOND_MITM";     break;
        case ESP_LE_AUTH_REQ_SC_ONLY:       auth_str = "ESP_LE_AUTH_REQ_SC_ONLY";       break;
        case ESP_LE_AUTH_REQ_SC_BOND:       auth_str = "ESP_LE_AUTH_REQ_SC_BOND";       break;
        case ESP_LE_AUTH_REQ_SC_MITM:       auth_str = "ESP_LE_AUTH_REQ_SC_MITM";       break;
        case ESP_LE_AUTH_REQ_SC_MITM_BOND:  auth_str = "ESP_LE_AUTH_REQ_SC_MITM_BOND";  break;
        default:                            auth_str = "INVALID BLE AUTH REQ";          break;
   }
   return auth_str;
}

// ================================================================================================
// Bluetooth GAP event→文字列変換(デバッグ用)
// ================================================================================================
static char *esp_bt_gap_event_to_str(esp_gap_ble_cb_event_t event)
{
    switch(event) {
#if (BLE_42_FEATURE_SUPPORT == TRUE)
      case     ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:                        return "ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT";
      case     ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:                   return "ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT";
      case     ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:                      return "ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT";
      case     ESP_GAP_BLE_SCAN_RESULT_EVT:                                  return "ESP_GAP_BLE_SCAN_RESULT_EVT";
      case     ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:                    return "ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT";
      case     ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:               return "ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT";
      case     ESP_GAP_BLE_ADV_START_COMPLETE_EVT:                           return "ESP_GAP_BLE_ADV_START_COMPLETE_EVT";
      case     ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:                          return "ESP_GAP_BLE_SCAN_START_COMPLETE_EVT";
#endif // #if (BLE_42_FEATURE_SUPPORT == TRUE)
      case     ESP_GAP_BLE_AUTH_CMPL_EVT:                                    return "ESP_GAP_BLE_AUTH_CMPL_EVT";
      case     ESP_GAP_BLE_KEY_EVT:                                          return "ESP_GAP_BLE_KEY_EVT";
      case     ESP_GAP_BLE_SEC_REQ_EVT:                                      return "ESP_GAP_BLE_SEC_REQ_EVT";
      case     ESP_GAP_BLE_PASSKEY_NOTIF_EVT:                                return "ESP_GAP_BLE_PASSKEY_NOTIF_EVT";
      case     ESP_GAP_BLE_PASSKEY_REQ_EVT:                                  return "ESP_GAP_BLE_PASSKEY_REQ_EVT";
      case     ESP_GAP_BLE_OOB_REQ_EVT:                                      return "ESP_GAP_BLE_OOB_REQ_EVT";
      case     ESP_GAP_BLE_LOCAL_IR_EVT:                                     return "ESP_GAP_BLE_LOCAL_IR_EVT";
      case     ESP_GAP_BLE_LOCAL_ER_EVT:                                     return "ESP_GAP_BLE_LOCAL_ER_EVT";
      case     ESP_GAP_BLE_NC_REQ_EVT:                                       return "ESP_GAP_BLE_NC_REQ_EVT";
#if (BLE_42_FEATURE_SUPPORT == TRUE)
      case     ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:                            return "ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT";
      case     ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:                           return "ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT";
#endif // #if (BLE_42_FEATURE_SUPPORT == TRUE)
      case     ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT:                         return "ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT";
      case     ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:                           return "ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT";
      case     ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:                      return "ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT";
      case     ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT:                   return "ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT";
      case     ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT:                     return "ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT";
      case     ESP_GAP_BLE_CLEAR_BOND_DEV_COMPLETE_EVT:                      return "ESP_GAP_BLE_CLEAR_BOND_DEV_COMPLETE_EVT";
      case     ESP_GAP_BLE_GET_BOND_DEV_COMPLETE_EVT:                        return "ESP_GAP_BLE_GET_BOND_DEV_COMPLETE_EVT";
      case     ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:                           return "ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT";
      case     ESP_GAP_BLE_UPDATE_WHITELIST_COMPLETE_EVT:                    return "ESP_GAP_BLE_UPDATE_WHITELIST_COMPLETE_EVT";
#if (BLE_42_FEATURE_SUPPORT == TRUE)
      case     ESP_GAP_BLE_UPDATE_DUPLICATE_EXCEPTIONAL_LIST_COMPLETE_EVT:   return "ESP_GAP_BLE_UPDATE_DUPLICATE_EXCEPTIONAL_LIST_COMPLETE_EVT";
#endif //#if (BLE_42_FEATURE_SUPPORT == TRUE)
      case     ESP_GAP_BLE_SET_CHANNELS_EVT:                                 return "ESP_GAP_BLE_SET_CHANNELS_EVT";
#if (BLE_50_FEATURE_SUPPORT == TRUE)
      case     ESP_GAP_BLE_READ_PHY_COMPLETE_EVT:                            return "ESP_GAP_BLE_READ_PHY_COMPLETE_EVT";
      case     ESP_GAP_BLE_SET_PREFERED_DEFAULT_PHY_COMPLETE_EVT:            return "ESP_GAP_BLE_SET_PREFERED_DEFAULT_PHY_COMPLETE_EVT";
      case     ESP_GAP_BLE_SET_PREFERED_PHY_COMPLETE_EVT:                    return "ESP_GAP_BLE_SET_PREFERED_PHY_COMPLETE_EVT";
      case     ESP_GAP_BLE_EXT_ADV_SET_RAND_ADDR_COMPLETE_EVT:               return "ESP_GAP_BLE_EXT_ADV_SET_RAND_ADDR_COMPLETE_EVT";
      case     ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT:                  return "ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT";
      case     ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT:                    return "ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT";
      case     ESP_GAP_BLE_EXT_SCAN_RSP_DATA_SET_COMPLETE_EVT:               return "ESP_GAP_BLE_EXT_SCAN_RSP_DATA_SET_COMPLETE_EVT";
      case     ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT:                       return "ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT";
      case     ESP_GAP_BLE_EXT_ADV_STOP_COMPLETE_EVT:                        return "ESP_GAP_BLE_EXT_ADV_STOP_COMPLETE_EVT";
      case     ESP_GAP_BLE_EXT_ADV_SET_REMOVE_COMPLETE_EVT:                  return "ESP_GAP_BLE_EXT_ADV_SET_REMOVE_COMPLETE_EVT";
      case     ESP_GAP_BLE_EXT_ADV_SET_CLEAR_COMPLETE_EVT:                   return "ESP_GAP_BLE_EXT_ADV_SET_CLEAR_COMPLETE_EVT";
      case     ESP_GAP_BLE_PERIODIC_ADV_SET_PARAMS_COMPLETE_EVT:             return "ESP_GAP_BLE_PERIODIC_ADV_SET_PARAMS_COMPLETE_EVT";
      case     ESP_GAP_BLE_PERIODIC_ADV_DATA_SET_COMPLETE_EVT:               return "ESP_GAP_BLE_PERIODIC_ADV_DATA_SET_COMPLETE_EVT";
      case     ESP_GAP_BLE_PERIODIC_ADV_START_COMPLETE_EVT:                  return "ESP_GAP_BLE_PERIODIC_ADV_START_COMPLETE_EVT";
      case     ESP_GAP_BLE_PERIODIC_ADV_STOP_COMPLETE_EVT:                   return "ESP_GAP_BLE_PERIODIC_ADV_STOP_COMPLETE_EVT";
      case     ESP_GAP_BLE_PERIODIC_ADV_CREATE_SYNC_COMPLETE_EVT:            return "ESP_GAP_BLE_PERIODIC_ADV_CREATE_SYNC_COMPLETE_EVT";
      case     ESP_GAP_BLE_PERIODIC_ADV_SYNC_CANCEL_COMPLETE_EVT:            return "ESP_GAP_BLE_PERIODIC_ADV_SYNC_CANCEL_COMPLETE_EVT";
      case     ESP_GAP_BLE_PERIODIC_ADV_SYNC_TERMINATE_COMPLETE_EVT:         return "ESP_GAP_BLE_PERIODIC_ADV_SYNC_TERMINATE_COMPLETE_EVT";
      case     ESP_GAP_BLE_PERIODIC_ADV_ADD_DEV_COMPLETE_EVT:                return "ESP_GAP_BLE_PERIODIC_ADV_ADD_DEV_COMPLETE_EVT";
      case     ESP_GAP_BLE_PERIODIC_ADV_REMOVE_DEV_COMPLETE_EVT:             return "ESP_GAP_BLE_PERIODIC_ADV_REMOVE_DEV_COMPLETE_EVT";
      case     ESP_GAP_BLE_PERIODIC_ADV_CLEAR_DEV_COMPLETE_EVT:              return "ESP_GAP_BLE_PERIODIC_ADV_CLEAR_DEV_COMPLETE_EVT";
      case     ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT:                 return "ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT";
      case     ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT:                      return "ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT";
      case     ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT:                       return "ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT";
      case     ESP_GAP_BLE_PREFER_EXT_CONN_PARAMS_SET_COMPLETE_EVT:          return "ESP_GAP_BLE_PREFER_EXT_CONN_PARAMS_SET_COMPLETE_EVT";
      case     ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT:                          return "ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT";
      case     ESP_GAP_BLE_EXT_ADV_REPORT_EVT:                               return "ESP_GAP_BLE_EXT_ADV_REPORT_EVT";
      case     ESP_GAP_BLE_SCAN_TIMEOUT_EVT:                                 return "ESP_GAP_BLE_SCAN_TIMEOUT_EVT";
      case     ESP_GAP_BLE_ADV_TERMINATED_EVT:                               return "ESP_GAP_BLE_ADV_TERMINATED_EVT";
      case     ESP_GAP_BLE_SCAN_REQ_RECEIVED_EVT:                            return "ESP_GAP_BLE_SCAN_REQ_RECEIVED_EVT";
      case     ESP_GAP_BLE_CHANNEL_SELETE_ALGORITHM_EVT:                     return "ESP_GAP_BLE_CHANNEL_SELETE_ALGORITHM_EVT";
      case     ESP_GAP_BLE_PERIODIC_ADV_REPORT_EVT:                          return "ESP_GAP_BLE_PERIODIC_ADV_REPORT_EVT";
      case     ESP_GAP_BLE_PERIODIC_ADV_SYNC_LOST_EVT:                       return "ESP_GAP_BLE_PERIODIC_ADV_SYNC_LOST_EVT";
      case     ESP_GAP_BLE_PERIODIC_ADV_SYNC_ESTAB_EVT:                      return "ESP_GAP_BLE_PERIODIC_ADV_SYNC_ESTAB_EVT";
#endif // #if (BLE_50_FEATURE_SUPPORT == TRUE)
      default:                                                               return "UNKNOWN EVENT";
    }
    return "UNKNOWN EVENT";
}

// ================================================================================================
// Bluetooth GATT event→文字列変換(デバッグ用)
// ================================================================================================
static char *esp_bt_gatts_event_to_str(esp_gatts_cb_event_t event)
{
    switch(event) {
      case     ESP_GATTS_REG_EVT:                       return "ESP_GATTS_REG_EVT";
      case     ESP_GATTS_READ_EVT:                      return "ESP_GATTS_READ_EVT";
      case     ESP_GATTS_WRITE_EVT:                     return "ESP_GATTS_WRITE_EVT";
      case     ESP_GATTS_EXEC_WRITE_EVT:                return "ESP_GATTS_EXEC_WRITE_EVT";
      case     ESP_GATTS_MTU_EVT:                       return "ESP_GATTS_MTU_EVT";
      case     ESP_GATTS_CONF_EVT:                      return "ESP_GATTS_CONF_EVT";
      case     ESP_GATTS_UNREG_EVT:                     return "ESP_GATTS_UNREG_EVT";
      case     ESP_GATTS_CREATE_EVT:                    return "ESP_GATTS_CREATE_EVT";
      case     ESP_GATTS_ADD_INCL_SRVC_EVT:             return "ESP_GATTS_ADD_INCL_SRVC_EVT";
      case     ESP_GATTS_ADD_CHAR_EVT:                  return "ESP_GATTS_ADD_CHAR_EVT";
      case     ESP_GATTS_ADD_CHAR_DESCR_EVT:            return "ESP_GATTS_ADD_CHAR_DESCR_EVT";
      case     ESP_GATTS_DELETE_EVT:                    return "ESP_GATTS_DELETE_EVT";
      case     ESP_GATTS_START_EVT:                     return "ESP_GATTS_START_EVT";
      case     ESP_GATTS_STOP_EVT:                      return "ESP_GATTS_STOP_EVT";
      case     ESP_GATTS_CONNECT_EVT:                   return "ESP_GATTS_CONNECT_EVT";
      case     ESP_GATTS_DISCONNECT_EVT:                return "ESP_GATTS_DISCONNECT_EVT";
      case     ESP_GATTS_OPEN_EVT:                      return "ESP_GATTS_OPEN_EVT";
      case     ESP_GATTS_CANCEL_OPEN_EVT:               return "ESP_GATTS_CANCEL_OPEN_EVT";
      case     ESP_GATTS_CLOSE_EVT:                     return "ESP_GATTS_CLOSE_EVT";
      case     ESP_GATTS_LISTEN_EVT:                    return "ESP_GATTS_LISTEN_EVT";
      case     ESP_GATTS_CONGEST_EVT:                   return "ESP_GATTS_CONGEST_EVT";
      case     ESP_GATTS_RESPONSE_EVT:                  return "ESP_GATTS_RESPONSE_EVT";
      case     ESP_GATTS_CREAT_ATTR_TAB_EVT:            return "ESP_GATTS_CREAT_ATTR_TAB_EVT";
      case     ESP_GATTS_SET_ATTR_VAL_EVT:              return "ESP_GATTS_SET_ATTR_VAL_EVT";
      case     ESP_GATTS_SEND_SERVICE_CHANGE_EVT:       return "ESP_GATTS_SEND_SERVICE_CHANGE_EVT";
      default:                                          return "UNKNOWN EVENT";
    }
    return "UNKNOWN EVENT";
}


char *addr_type_to_str(uint8_t addr_type)
{
    char*   addr_type_str;
    switch(addr_type) {
        case   0 : addr_type_str = "ADDR_TYPE_PUBLIC";          break;
        case   1 : addr_type_str = "ADDR_TYPE_RANDOM";          break;
        case   2 : addr_type_str = "ADDR_TYPE_RPA_PUBLIC";      break;
        case   3 : addr_type_str = "ADDR_TYPE_RPA_RANDOM";      break;
        default  : addr_type_str = "UNKNOWN";                   break;
    }
    return addr_type_str;
}
