/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


// 文字列最大長
#define     SSID_NAME_SIZE      16
#define     SSID_PASS_SIZE      16
#define     SVR_ADDR_SIZE       64


// NVS namespave/key
#define     NVS_NAMESPACE_INFO      "app_param"
#define     NVS_KEY_SSID_NAME       "ssid_name"
#define     NVS_KEY_SSID_PASS       "ssid_pass"
#define     NVS_KEY_SVR_ADDR        "svr_addr"
#define     NVS_KEY_SVR_PORT        "svr_port"
#define     NVS_KEY_FWSVR_ADDR      "loop_itvl"


// 設定パラメータ構造体
struct app_param {
    char        ssid_name[SSID_NAME_SIZE];
    char        ssid_pass[SSID_PASS_SIZE];
    char        server_address[SVR_ADDR_SIZE];
    uint16_t    server_port;
    uint32_t    loop_interval;
};



// 設定パラメータ
extern struct app_param            AppParam;

extern bool LoadParam(struct app_param* pParam);
extern void SaveParam(struct app_param* pParam);
extern void ClearParam(void);
extern void DispParam(struct app_param* pParam);

