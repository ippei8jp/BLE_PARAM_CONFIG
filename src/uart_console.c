#include    <stdio.h>
#include    <stdint.h>
#include    <stdbool.h>
#include    <string.h>
#include    <stdarg.h>

#include    "freertos/FreeRTOS.h"
#include    "freertos/task.h"
#include    "esp_system.h"
#include    "rom/uart.h"

#include    "uart_console.h"



// ========= UARTからの入力待ち ===============================================
// param    loop_num: 待ち時間(単位100msec)
// return   true: キー入力があった     false: キー入力はなかった
bool uart_checkkey(int loop_num)
{
    bool    ret = false;
    uint8_t ch;
     for (int loop_cnt = 0; loop_cnt < loop_num; loop_cnt++) {
        // UARTから1文字取得
        if (uart_rx_one_char(&ch) == OK) {
            // 入力あり
            ret = true;
            break;
        }
        if ((loop_cnt % 5)== 0) {
            // たくさん出ると鬱陶しいので5回毎に
            putchar('.');
            fflush(stdout);
        }
        // 100ms待つ
        vTaskDelay(100 / portTICK_RATE_MS);
    }
    putchar('\n');

    // バッファにたまっているデータを読み捨てる
    while (uart_rx_one_char(&ch) == OK);
    return ret;
}


// ========= UARTからの1文字入力(waitなし) ===============================================
// param    なし
// return   0        : キー入力がなかった
//          それ以外 : 入力された文字コード
// note     CRは無視するので注意
int uart_getchar_nowait(void)
{
    uint8_t ch;
    if (uart_rx_one_char(&ch) != OK) {
        ch =  0;
    }
    if (ch  == '\r') {
        ch = 0;     // CRは無視
    }
    return (int)ch;
}


// ========= UARTから1文字取得 ================================================
// param    なし
// return   文字コード
// note     CRは無視するので注意
int uart_getchar(void)
{
    uint8_t ch;
    while (1) {
        // UARTから1文字取得
        if (uart_rx_one_char(&ch) == OK) {
            // 入力あり
            if (ch == '\r') {
                // CRなら次の値を取得
                continue;
            }
            // 入力された値を返す
            return ch;
        }
        // 100ms待つ(CPUを握りっぱなしにしないように)
        vTaskDelay(100 / portTICK_RATE_MS);
    }
}

// ========= UARTから1行取得 ===================================================
// param    buf: 文字列格納領域へのポインタ
//          max: 最大文字列長
// return   入力された文字列長
int uart_gets(char* buf, int max)
{
    int     i = 0;
    while (i < (max - 1)) {     // null terminate の分を空けておくので max - 1
        char ch = uart_getchar();
        if (ch == '\n') {
            // LFで終了
            putchar(ch);
            fflush(stdout);
            break;
        }
        if (ch == '\b' || ch == 0x7f) {     // BackSpace or DEL
            if (i > 0) {        // 先頭でない
                i--;            // ポインタを一つ前に
                putchar('\b');  // 前の文字の表示を削除
                putchar(' ');
                putchar('\b');
                fflush(stdout);
            }
        }
        else {
            // それ以外の文字はバッファに格納
            buf[i] = ch;
            i++;
            putchar(ch);        // 表示
            fflush(stdout);
        }
    }
    buf[i] = '\0';          // null terminate
    return i;
}
