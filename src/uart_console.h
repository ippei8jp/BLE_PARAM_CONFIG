#include    <stdio.h>
#include    <stdint.h>
#include    <stdbool.h>

extern bool uart_checkkey(int loop_num);
extern int  uart_getchar_nowait(void);
extern int  uart_getchar(void);
extern int  uart_gets(char* buf, int max);
