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

// LOG表示用TAG(関数名にしておく)
#define     TAG             __func__

// ==== static 変数 ===========================================================================================
// GATTサーバattributeテーブル
uint16_t         param_config_handle_table[PCONF_IDX_NUM];

// 接続情報
static bool            pconf_is_connected = false;                 // 接続中フラグ
static uint16_t        pconf_conn_id      = 0xffff;                // 接続ID
static esp_gatt_if_t   pconf_gatts_if     = ESP_GATT_IF_NONE;      // GATTインタフェース
static esp_bd_addr_t   pconf_remote_bda;                           // リモートのBDアドレス

// ==== プロファイルの設定 ======================================================================================
// characteristicのアクセス種別
// 未使用 static const uint8_t char_prop_notify               = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
// 未使用 static const uint8_t char_prop_read                 = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_read_write           = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ;

static const uint16_t primary_service_uuid          = ESP_GATT_UUID_PRI_SERVICE;        // プライマリサービス
static const uint16_t character_declaration_uuid    = ESP_GATT_UUID_CHAR_DECLARE;       // characteristic 宣言
// static const uint16_t character_client_config_uuid  = ESP_GATT_UUID_CHAR_CLIENT_CONFIG; // CCC (Client Characteristic Configuration Descriptor)

// パラメータ設定サービス
const uint8_t service_uuid[]         = UUID128_to_ARRAY(0xea7542b0, 0xbfae, 0x7587, 0xdc60, 0x45dbf29ca088);     // Service UUID     ea7542b0-bfae-7587-dc60-45dbf29ca088  https://uuid.doratool.com/ などで生成

// SSID名
const uint8_t    ssid_name_uuid[]    = UUID128_to_ARRAY(0xea7542b1, 0xbfae, 0x7587, 0xdc60, 0x45dbf29ca088);     // UUID             ea7542b1-bfae-7587-dc60-45dbf29ca088    ほんとは良くないけど、サービスUUIDから連続値を割り当てておく

// SSIDパスワード
const uint8_t    ssid_pass_uuid[]    = UUID128_to_ARRAY(0xea7542b2, 0xbfae, 0x7587, 0xdc60, 0x45dbf29ca088);     // UUID             ea7542b2-bfae-7587-dc60-45dbf29ca088

// ループインターバル(Heart Rate Control Point)
const uint8_t loop_interval_uuid[]   = UUID128_to_ARRAY(0xea7542b3, 0xbfae, 0x7587, 0xdc60, 0x45dbf29ca088);     // UUID             ea7542b3-bfae-7587-dc63-45dbf29ca088

/// Attribute データベース
static esp_gatts_attr_db_t param_config_gatt_db[PCONF_IDX_NUM] =
{
    // ==== サービス宣言 ====
    [PCONF_IDX_SVC] = {                                 // Parameter Configulation Service Declaration
        .attr_control = { .auto_rsp = ESP_GATT_AUTO_RSP }, 
        .att_desc = {
            .uuid_length    = ESP_UUID_LEN_16, 
            .uuid_p         = (uint8_t *)&primary_service_uuid,
            .perm           = ESP_GATT_PERM_READ,
            .max_length     = sizeof(service_uuid),
            .length         = sizeof(service_uuid),
            .value          = (uint8_t *)service_uuid
        }
    },
    // ==== SSID名 ====
    [PCONF_IDX_SSID_NAME_CHAR] = {                      // characteristic 宣言
        .attr_control = { .auto_rsp = ESP_GATT_AUTO_RSP }, 
        .att_desc = {
            .uuid_length    = ESP_UUID_LEN_16, 
            .uuid_p         = (uint8_t *)&character_declaration_uuid,
            .perm           = ESP_GATT_PERM_READ,
            .max_length     = sizeof(char_prop_read_write),
            .length         = sizeof(char_prop_read_write),
            .value          = (uint8_t *)&char_prop_read_write
        }
    },
    [PCONF_IDX_SSID_NAME_VAL] = {                       // characteristic 値
        .attr_control = { .auto_rsp = ESP_GATT_AUTO_RSP }, 
        .att_desc = {
            .uuid_length    = sizeof(ssid_name_uuid), 
            .uuid_p         = (uint8_t *)ssid_name_uuid,
            .perm           = ESP_GATT_PERM_WRITE_ENCRYPTED | ESP_GATT_PERM_READ_ENCRYPTED,
            .max_length     = sizeof(AppParam.ssid_name) - 1,            // NULL文字追加のため、1文字分減らしておく
            .length         = sizeof(AppParam.ssid_name) - 1,            // あとで書き換え
            .value          = (uint8_t*)AppParam.ssid_name
        }
    }, 
    // ==== SSIDパスワード ====
    [PCONF_IDX_SSID_PASS_CHAR] = {                      // characteristic 宣言
        .attr_control = { .auto_rsp = ESP_GATT_AUTO_RSP }, 
        .att_desc = {
            .uuid_length    = ESP_UUID_LEN_16, 
            .uuid_p         = (uint8_t *)&character_declaration_uuid,
            .perm           = ESP_GATT_PERM_READ,
            .max_length     = sizeof(char_prop_read_write),
            .length         = sizeof(char_prop_read_write),
            .value          = (uint8_t *)&char_prop_read_write
        }
    },
    [PCONF_IDX_SSID_PASS_VAL]  = {                      // characteristic 値
        .attr_control = { .auto_rsp = ESP_GATT_AUTO_RSP }, 
        .att_desc = {
            .uuid_length    = sizeof(ssid_pass_uuid), 
            .uuid_p         = (uint8_t *)ssid_pass_uuid,
            .perm           = ESP_GATT_PERM_WRITE_ENCRYPTED | ESP_GATT_PERM_READ_ENCRYPTED,
            .max_length     = sizeof(AppParam.ssid_pass) - 1,            // NULL文字追加のため、1文字分減らしておく
            .length         = sizeof(AppParam.ssid_pass) - 1,            // あとで書き換え
            .value          = (uint8_t *)AppParam.ssid_pass
        }
    },
    // ==== ループインターバル ====
    [PCONF_IDX_LOOP_IVAL_CHAR]      = {              // characteristic 宣言
        .attr_control = { .auto_rsp = ESP_GATT_AUTO_RSP }, 
        .att_desc = {
            .uuid_length    = ESP_UUID_LEN_16, 
            .uuid_p         = (uint8_t *)&character_declaration_uuid,
            .perm           = ESP_GATT_PERM_READ,
            .max_length     = sizeof(char_prop_read_write),
            .length         = sizeof(char_prop_read_write),
            .value          = (uint8_t *)&char_prop_read_write
        }
    },    [PCONF_IDX_LOOP_IVAL_VAL] = {              // characteristic 値
        .attr_control = { .auto_rsp = ESP_GATT_AUTO_RSP }, 
        .att_desc = {
            .uuid_length    = sizeof(loop_interval_uuid), 
            .uuid_p         = (uint8_t *)loop_interval_uuid,
            .perm           = ESP_GATT_PERM_WRITE_ENCRYPTED | ESP_GATT_PERM_READ_ENCRYPTED,
            .max_length     = sizeof(AppParam.loop_interval), 
            .length         = sizeof(AppParam.loop_interval),
            .value          = (uint8_t *)&AppParam.loop_interval
        }
    },
};

// characteristic - プログラム内変数対応テーブル
struct char_var_tab param_config_variable_table[PCONF_IDX_NUM] = {
    [PCONF_IDX_SVC]             = {NULL,                                                 0   },  // サービス宣言
    [PCONF_IDX_SSID_NAME_CHAR]  = {NULL,                                                 0   },  // SSID名 宣言
    [PCONF_IDX_SSID_NAME_VAL]   = {AppParam.ssid_name,       sizeof(AppParam.ssid_name)       },  // SSID名 (Read/Write)
    [PCONF_IDX_SSID_PASS_CHAR]  = {NULL,                                                  0   },  // SSIDパスワード 宣言
    [PCONF_IDX_SSID_PASS_VAL]   = {AppParam.ssid_pass,       sizeof(AppParam.ssid_pass)       },  // SSIDパスワード 値(Read/Write)
    [PCONF_IDX_LOOP_IVAL_CHAR]  = {NULL,                                                  0   },  // ループインターバル 宣言
    [PCONF_IDX_LOOP_IVAL_VAL]   = {&AppParam.loop_interval,  sizeof(AppParam.loop_interval)   },  // ループインターバル 値(Read/Write)
};

// ================================================================================================
// ハンドル→characteristic テーブルのインデックス
// ================================================================================================
static int handle_to_index(uint16_t handle)
{
    int                 idx;
    for (idx = 0; idx < PCONF_IDX_NUM; idx++) {    // 一致するハンドルを探す
        if ( handle == param_config_handle_table[idx]) {
            return idx;
        }
    }
    return -1;      // 見つからなかった
}

// ================================================================================================
// プロファイル イベントハンドラ
// ================================================================================================
void param_config_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT:                     // 登録イベント
            esp_ble_gap_set_device_name(PARAM_CONFIG_DEVICE_NAME);  // DeviceNameの登録
            esp_ble_gap_config_local_privacy(true);                 // ローカルデバイスでのプライバシー有効化

            // lengthフィールドの設定
            esp_gatts_attr_db_t* pAttr_db;
            pAttr_db = &param_config_gatt_db[PCONF_IDX_SSID_NAME_VAL];
            pAttr_db->att_desc.length = strlen((const char*)pAttr_db->att_desc.value);
            pAttr_db = &param_config_gatt_db[PCONF_IDX_SSID_PASS_VAL];
            pAttr_db->att_desc.length = strlen((const char*)pAttr_db->att_desc.value);

            esp_ble_gatts_create_attr_tab(param_config_gatt_db, gatts_if,
                                      PCONF_IDX_NUM, PARAM_CONFIG_SVC_INST_ID);  // Attribute テーブルの登録
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:          // Attribute テーブルの登録完了イベント
            if (param->add_attr_tab.status == ESP_GATT_OK){
                // 登録成功
                if(param->add_attr_tab.num_handle == PCONF_IDX_NUM) {
                    // Attribute数が想定した値に等しい
                    memcpy(param_config_handle_table, param->add_attr_tab.handles, sizeof(param_config_handle_table));
#if 0   // DEBUG
                    // Attributeテーブルの確認
                    ESP_LOGV(TAG, "    The number handle = %x",param->add_attr_tab.num_handle);
                    ESP_LOGV(TAG, "    Attribute table:");
                    for (int i = 0; i < PCONF_IDX_NUM; i++) {
                        ESP_LOGV(TAG, "        %d: 0x%04x", i, param_config_handle_table[i]);
                    }
#endif
                    // サービスの開始
                    esp_ble_gatts_start_service(param_config_handle_table[PCONF_IDX_SVC]);
                } else {
                    // Attribute数が想定した値と異なる
                    ESP_LOGE(TAG, "    Create attribute table abnormally, num_handle (%d) doesn't equal to PCONF_IDX_NUM(%d)",
                         param->add_attr_tab.num_handle, PCONF_IDX_NUM);
                }
            } else {
                // 登録失敗
                ESP_LOGE(TAG, "    Create attribute table failed, error code = %x", param->add_attr_tab.status);
            }
            break;
        case ESP_GATTS_START_EVT:                   // サーバ動作開始完了イベント
                ESP_LOGI(TAG, "    GATT server started!");
            break;
        case ESP_GATTS_READ_EVT:                    // Readイベント
            ESP_LOGI(TAG, "    read value");
            ESP_LOGI(TAG, "    handole : %04x", param->read.handle);
            break;
        case ESP_GATTS_WRITE_EVT:                   // writeイベント
            ESP_LOGI(TAG, "    write value:");
            ESP_LOGI(TAG, "    handole : %04x", param->write.handle);
            ESP_LOGI(TAG, "    offset : %d,    length : %d", param->write.offset, param->write.len);
            esp_log_buffer_hex(TAG, param->write.value, param->write.len);
            // 保持したい領域に自分でコピーする(自動でコピーしてくれない)
            {
                int     idx = handle_to_index(param->write.handle);
                if (idx >= 0) {                 // ハンドルが見つかった
                    uint8_t* var_ptr = param_config_variable_table[idx].value;
                    uint16_t var_len = param_config_variable_table[idx].length;
                    if (var_ptr) {                          // 領域が定義されている
                        const uint8_t*      char_ptr;
                        uint16_t            char_len;
                        // characteristicのデータを読み出して保存
                        esp_gatt_status_t ret = esp_ble_gatts_get_attr_value(param->write.handle, &char_len, &char_ptr);
                        if (ret == ESP_GATT_OK) {
                            ESP_LOGI(TAG, "====[esp_ble_gatts_get_attr_value] :");
                            esp_log_buffer_hex(TAG, char_ptr, char_len);
                            // データのコピー
                            memcpy(var_ptr, char_ptr, char_len);
                            if (char_len < var_len) {
                                for (int i = char_len; i < var_len; i++) {
                                    // 設定値の後ろをNULLで埋める(文字列のNULL Terminateのため)
                                    // (数値の場合も以前の上位バイトが残ってしまうのでクリア)
                                    var_ptr[i] = 0x00;
                                }
                            }
                        }
                        else {
                            ESP_LOGI(TAG, "    esp_ble_gatts_get_attr_value : ERROR!! %d", ret);
                        }
                    }
                    else {
                            ESP_LOGI(TAG, "    variable not defined");
                    }
                }
            }
            /* ---- NOTE ---------------------
                ここで記憶しておかなくても 任意の場所で
                    esp_gatt_status_t esp_ble_gatts_get_attr_value(uint16_t attr_handle, uint16_t *length, const uint8_t **value)
                を使えば読み出せるんだけど、都度読み出すのは面倒なのでここでやっとく。
               ------------------------------- */
            break;
        case ESP_GATTS_CONNECT_EVT:                 // 接続要求イベント
            ESP_LOGI(TAG, "    connection start");
            pconf_conn_id          = param->connect.conn_id;
            pconf_gatts_if         = gatts_if;
            pconf_is_connected     = true;
            memcpy(pconf_remote_bda, param->connect.remote_bda, sizeof(pconf_remote_bda));
            esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
            break;
        case ESP_GATTS_DISCONNECT_EVT:              // 切断要求イベント
            ESP_LOGI(TAG, "    disconnect reason 0x%x", param->disconnect.reason);
            pconf_conn_id      = 0xffff;
            pconf_gatts_if     = ESP_GATT_IF_NONE;
            pconf_is_connected = false;
            // advertising 再開
            start_advertising();
            break;
        case ESP_GATTS_CONF_EVT:                    // Notify送信イベント
            ESP_LOGI(TAG, "    status = %d", param->conf.status);
            if (param->conf.status == ESP_GATT_OK) {
                esp_log_buffer_hex(TAG, param->conf.value, param->conf.len);
            }
            break;
        case ESP_GATTS_SET_ATTR_VAL_EVT:            // SetValueイベント(esp_ble_gatts_set_attr_value()による書き込み)
            ESP_LOGI(TAG, "    status = %d", param->set_attr_val.status);
            break;
        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(TAG, "    conn_id = %d", param->mtu.conn_id);
            ESP_LOGI(TAG, "    mtu     = %d", param->mtu.mtu);
            break;
#if 0
        case ESP_GATTS_EXEC_WRITE_EVT:      // ロングattributeに対する書き込み時(今回はロングAttributeがないので発生しない)
            break;
        case ESP_GATTS_UNREG_EVT:
            break;
        case ESP_GATTS_DELETE_EVT:
            break;
        case ESP_GATTS_STOP_EVT:
            break;
        case ESP_GATTS_OPEN_EVT:
            break;
        case ESP_GATTS_CANCEL_OPEN_EVT:
            break;
        case ESP_GATTS_CLOSE_EVT:
            break;
        case ESP_GATTS_LISTEN_EVT:
            break;
        case ESP_GATTS_CONGEST_EVT:
            break;
#endif
        default:
            ESP_LOGI(TAG, "    event nor handled");
           break;
    }
}

void param_config_disconnect(void)
{
    // 接続されたままだったら切断する
    if (pconf_is_connected) {           // 接続されていたら
        uint8_t* bd_addr = pconf_remote_bda;
        ESP_LOGI(TAG, "    disconnect :   %02x:%02x:%02x:%02x:%02x:%02x\n",    // BDアドレスの表示
                bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3], bd_addr[4], bd_addr[5]);
        esp_ble_gap_disconnect(pconf_remote_bda);   // Disconnect
    }
    return;
}

