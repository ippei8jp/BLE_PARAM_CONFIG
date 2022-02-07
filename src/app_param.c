/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#include    <stdio.h>
#include    <stdbool.h>
#include    <stdint.h>
#include    <string.h>

#include    "freertos/FreeRTOS.h"
#include    "freertos/task.h"
#include    "esp_log.h"
#include    "esp_err.h"

#include    "nvs_flash.h"

#include    "uart_console.h"
#include    "app_param.h"

// LOG表示用TAG(関数名にしておく)
#define     TAG             __func__

// 設定パラメータ
struct app_param            AppParam;

// 設定パラメータのロード
// return : ture  ロードできた   false   ロードできなかった
bool LoadParam(struct app_param* pParam)
{
    bool        ret = true;
    esp_err_t   err;
    nvs_handle  handle_1;
    size_t      buf_len;

    // NVS オープン
    err = nvs_open(NVS_NAMESPACE_INFO, NVS_READWRITE, &handle_1);
    if (err != ESP_OK) {
        // NVS オープン失敗
        ESP_LOGE(TAG, "nvs_open() failed.(%d)\n", err);
        return false;
    }

    // SSID名称
    buf_len = sizeof(pParam->ssid_name);
    err = nvs_get_str(handle_1, NVS_KEY_SSID_NAME, pParam->ssid_name, &buf_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_get_str(ssid_name) failed.(%d)\n", err);
        ret = false;
        // 失敗しても残りのパラメータを読む
    }
    else {
        if (strlen((const char*)pParam->ssid_name) == 0) {
            // 文字列長が0(設定されていない)だったらエラー扱い
            ret = false;
        }
    }

    // SSIDパスワード
    buf_len = sizeof(pParam->ssid_pass);
    err = nvs_get_str(handle_1, NVS_KEY_SSID_PASS, pParam->ssid_pass, &buf_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_get_str(ssid_pass) failed.(%d)\n", err);
        ret = false;
        // 失敗しても残りのパラメータを読む
    }
    else {
        if (strlen((const char*)pParam->ssid_pass) == 0) {
            // 文字列長が0(設定されていない)だったらエラー扱い
            ret = false;
        }
    }
#if 0
    // サーバ アドレス
    buf_len = sizeof(pParam->server_address);
    err = nvs_get_str(handle_1, NVS_KEY_SVR_ADDR, pParam->server_address, &buf_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_get_str(svr_addr) failed.(%d)\n", err);
        ret = false;
        // 失敗しても残りのパラメータを読む
    }

    // サーバ ポート番号
    err = nvs_get_u16(handle_1, NVS_KEY_SVR_PORT, &pParam->server_port);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_get_i32(svr_port) failed.(%d)\n", err);
        ret = false;
        // 失敗しても残りのパラメータを読む
    }
#endif
    // LOOP INTERVAL
    err = nvs_get_u32(handle_1, NVS_KEY_SVR_PORT, &pParam->loop_interval);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_get_i32(loop_interval) failed.(%d)\n", err);
        ret = false;
        // 失敗しても残りのパラメータを読む
    }
    else {
        if (pParam->loop_interval == 0) {
            // 設定値が0(設定されていない)だったらエラー扱い
            ret = false;
        }
    }

    // NVS クローズ
    nvs_close(handle_1);
    return ret;
}

// 設定パラメータのセーブ
void SaveParam(struct app_param* pParam)
{
    esp_err_t   err;
    nvs_handle handle_1;

    // NVS オープン
    err = nvs_open(NVS_NAMESPACE_INFO, NVS_READWRITE, &handle_1);
    if (err != ESP_OK) {
        // NVS オープン失敗
        ESP_LOGE(TAG, "nvs_open() failed.(%d)\n", err);
        return;
    }

    // SSID NAME
    err = nvs_set_str(handle_1, NVS_KEY_SSID_NAME, pParam->ssid_name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_str(ssid_name) failed.(%d)\n", err);
        // とりあえず続ける
    }

    // SSIDパスワード
    err = nvs_set_str(handle_1, NVS_KEY_SSID_PASS, pParam->ssid_pass);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_str(ssid_pass) failed.(%d)\n", err);
        // とりあえず続ける
    }
#if 0
    // サーバ アドレス
    err = nvs_set_str(handle_1, NVS_KEY_SVR_ADDR, pParam->server_address);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_str(svr_addr) failed.(%d)\n", err);
        // とりあえず続ける
    }

    // サーバ ポート番号
    err = nvs_set_u16(handle_1, NVS_KEY_SVR_PORT, pParam->server_port);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_i32(svr_port) failed.(%d)\n", err);
        // とりあえず続ける
    }
#endif
    // LOOP INTERVAL
    err = nvs_set_u32(handle_1, NVS_KEY_SVR_PORT, pParam->loop_interval);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_i32(loop_interval) failed.(%d)\n", err);
        // とりあえず続ける
    }

    // NVS クローズ
    nvs_close(handle_1);

    return;
}

// 設定パラメータの消去
void ClearParam(void)
{
    esp_err_t   err;

    nvs_handle handle_1;
    err = nvs_open(NVS_NAMESPACE_INFO, NVS_READWRITE, &handle_1);
    if (err != ESP_OK) {
        // NVS オープン失敗
        ESP_LOGE(TAG, "nvs_open() failed.(%d)\n", err);
        return;
    }

    // serverinfoの下すべてを消去する
    err = nvs_erase_all(handle_1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_erase_all() failed.(%d)\n", err);
        // とりあえず続ける
    }

    nvs_close(handle_1);

    return;
}

// 設定パラメータの表示
void DispParam(struct app_param* pParam)
{
    printf("---------------------------------------\n");
    printf("    ssid_name     : %s\n", pParam->ssid_name);
    printf("    ssid_pass     : %s\n", pParam->ssid_pass);
    printf("    loop_interval : 0x%08x  (%d)\n", pParam->loop_interval, pParam->loop_interval);
    printf("---------------------------------------\n");

    return;
}