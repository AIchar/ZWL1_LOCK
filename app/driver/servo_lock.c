#include "driver/servo_lock.h"

static os_timer_t timer_lock_close;

static uint32 duty_on = 7500;
static uint32 duty_off = 3055;
static uint32 open_time = 2000;

/**
 * pwm初始化
 */
void ICACHE_FLASH_ATTR LOCK_Init(server_lock_info info)
{
    duty_on = (((float)info.dergee_open/180)*10000) + 2500;
    duty_off = (((float)info.dergee_close/180)*10000) + 2500;
    open_time = info.time_open;
	
    
    uint32_t period = 100000; // * 200ns ^= 50 Hz = 20ms

	// PWM setup
	uint32 io_info[1][3] = {{PERIPHS_IO_MUX_GPIO0_U,  FUNC_GPIO0, 0}}; //D5
	uint32 pwm_duty_init[1] = {duty_off};
	pwm_init(period, pwm_duty_init, 1, io_info);
	pwm_start();

    os_timer_disarm(&timer_lock_close);
    os_timer_setfn(&timer_lock_close,LOCK_OFF,NULL);
    
    LOCK_OFF();
}


/**
 * 开锁
 */
void ICACHE_FLASH_ATTR LOCK_ON(){
    os_timer_disarm(&timer_lock_close);
    pwm_set_duty(duty_on,0);
    pwm_start();
    os_timer_arm(&timer_lock_close,open_time,0);
}


/**
 * 关锁
 */
void ICACHE_FLASH_ATTR LOCK_OFF(){

    pwm_set_duty(duty_off,0);
    pwm_start();
}


/**
 * 设置开关锁角度
 */
void ICACHE_FLASH_ATTR LOCK_SET(u8 lock_on_dergee,int lock_off_dergee,uint32 op_time){
    if (lock_on_dergee != 0)
    {
        duty_on=(((float)lock_on_dergee/180)*10000) + 2500;
        LOCK_ON();
    }
    if (lock_off_dergee != 0)
    {
        duty_off=(((float)lock_off_dergee/180)*10000) + 2500;
        LOCK_OFF();
    }    
    if (op_time != 0)
    {
        open_time = op_time;
    }
}

