/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

// ==== マクロ定義 ===========================================================================================
#define PARAM_CONFIG_PROFILE_APP_IDX        0                               // 心拍計のプロファイルID(プロファイルリスト中のインデックス)
#define ESP_PARAM_CONFIG_APP_ID             PARAM_CONFIG_PROFILE_APP_IDX    // 心拍計のアプリケーションID  (プロファイルIDと同じにしておく)
#define PARAM_CONFIG_DEVICE_NAME            "ESP_PARAM_CONFIG"              // デバイス名
#define PARAM_CONFIG_SVC_INST_ID            0                               // サービスインスタンスID


// ==== enum ===========================================================================================
///Attributes State Machine
enum
{
    PCONF_IDX_SVC,

    PCONF_IDX_SSID_NAME_CHAR,       // SSID名
    PCONF_IDX_SSID_NAME_VAL,

    PCONF_IDX_SSID_PASS_CHAR,       // SSIDパスワード
    PCONF_IDX_SSID_PASS_VAL,

    PCONF_IDX_LOOP_IVAL_CHAR,       // ループインターバル
    PCONF_IDX_LOOP_IVAL_VAL,

    PCONF_IDX_NUM,
};

// ==== 構造体 ===========================================================================================
struct char_var_tab {           // キャラクタリスティック-プログラム内変数対応テーブル
    void*   value;
    uint16_t length;
};


// ==== extern 宣言 ===========================================================================================
extern  uint16_t    param_config_handle_table[];
extern  void        param_config_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
extern void         param_config_disconnect(void);

// extern uint8_t  ssid_name[SSID_NAME_SIZE];      // SSID名格納領域
// extern uint8_t  ssid_pass[];                    // SSIDパスワード格納領域
// extern uint32_t loop_interval;                  // ループインターバル値格納領域


extern const uint8_t   service_uuid[16];        // Service UUID
extern const uint8_t   ssid_name_uuid[16];      // UUID
extern const uint8_t   ssid_pass_uuid[16];      // UUID
extern const uint8_t    loop_interval_uuid[16]; // UUID

