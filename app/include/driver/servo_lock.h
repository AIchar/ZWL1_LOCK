#ifndef _SERVO_LOCK_H_
#define _SERVO_LOCK_H_

#include "driver/pwm.h"

typedef struct
{
    u8 first_F;
    u8 dergee_open;
    u8 dergee_close;
    u8 nothing;
    u32 time_open;
}server_lock_info;


void ICACHE_FLASH_ATTR LOCK_Init(server_lock_info info);
void ICACHE_FLASH_ATTR LOCK_ON();
void ICACHE_FLASH_ATTR LOCK_OFF();
void ICACHE_FLASH_ATTR LOCK_SET(u8 lock_on_dergee,int lock_off_dergee,uint32 op_time);


#endif