/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


// UUIDを配列に変換するマクロ
/*
    UUID16値を指定する
*/
#define UUID16_to_ARRAY(id)   { \
                                        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, (uint8_t)(id), (uint8_t)(id >>  8), 0x00, 0x00, \
                                    }

/*
    一般的な表記 12345678-9012-3456-7890-123456789012  の場合、
    UUID128_to_ARRAY(0x12345678, 0x9012, 0x3456, 0x7890,0x123456789012)
    と指定する
*/
#define UUID128_to_ARRAY(id1, id2, id3, id4, id5)   { \
                (uint8_t)(id5), (uint8_t)(id5 >> 8), (uint8_t)(id5 >> 16), (uint8_t)(id5 >> 24), (uint8_t)(id5 >> 32), (uint8_t)(id5 >> 40), \
                (uint8_t)(id4), (uint8_t)(id4 >> 8), \
                (uint8_t)(id3), (uint8_t)(id3 >> 8), \
                (uint8_t)(id2), (uint8_t)(id2 >> 8), \
                (uint8_t)(id1), (uint8_t)(id1 >> 8), (uint8_t)(id1 >> 16), (uint8_t)(id1 >> 24), \
                                    }

// 静的パスキー
#define STATIC_PASSKEY      123456;

// ==== 構造体 ======================================================================================
// GATTサーバのプロファイル管理用構造体
struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
#if 0   // 今回 以下は使わないので削除
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
#endif
};

// ==== extern宣言 ======================================================================================
extern uint8_t      manufacturer_data[3];                  // 参照先でsizeof()を使いたいのでサイズも指定
extern struct       gatts_profile_inst profile_tab[];


extern void ble_main(void);


// プロファイル関連設定
#define PROFILE_NUM                               1                         // プロファイル数
#include "param_config.h"

// コールバッグ関連設定
#include "callbacks.h"

