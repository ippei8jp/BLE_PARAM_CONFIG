/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "esp_netif_ip_addr.h"


extern esp_err_t wait_wifi_connect(void);
extern esp_err_t wifi_init_sta(char* ssid_name, char* ssid_pass);

// 自身に割り当てられたIPアドレス
extern esp_ip4_addr_t my_ipaddr;
