#include "ets_sys.h"
#include "os_type.h"
#include "c_types.h"
#include "osapi.h"
#include "mem.h"
#include "gpio.h"
#include "user_interface.h"

#include "mqtt/mqtt.h"
#include "driver/uart.h"
#include "driver/wifi.h"
#include "driver/ota.h"
#include "driver/data.h"
#include "driver/1016C.h"
#include "driver/gpio_key.h"
#include "driver/servo_lock.h"

#define MAIN_DEBUG_ON

#if defined(MAIN_DEBUG_ON)
#define INFO( format, ... ) os_printf( format, ## __VA_ARGS__ )
#else
#define INFO( format, ... )
#endif


#define USER_INFO_ADDR 0x7c


u8 mac_str[13];						//mac地址
u8 ota_topic[32]={""};				//ota升级话题
u8 lwt_topic[32]={""};				//遗嘱话题
u8 birth_topic[30]={""};			//出生话题

u8 status_topic[64]={""};				//状态话题
u8 control_topic[64]={""};				//控制话题 |开锁| |设置开锁角度| |设置关锁角度|


server_lock_info lock_info;
MQTT_Client mqttClient;




/**
 * 	smartconfig配置回调
 */
void smartconfig_cd(sm_status status){

	switch (status)
	{
		case SM_STATUS_FIND_CHANNEL:
			led_set(LED_COLOR_CYAN,LED_BLN);
			break;
		case SM_STATUS_FINISH:
			INFO("smartconfig_finish\n");
			led_set(LED_COLOR_GREEN,LED_ON);
			close_timer(3000);
			break;
	
		case SM_STATUS_GETINFO:
			INFO("wifiinfo_error\n");
			break;
		case SM_STATUS_TIMEOUT:
			INFO("smartconfig_timeout\n");
			break;
	}

}


/**
 * 	ota升级回调
 */
void ota_finished_callback(void * arg) {
	struct upgrade_server_info *update = arg;
	// os_timer_disarm(&OS_LED_Timer);
	if (update->upgrade_flag == true) {
		INFO("OTA  Success ! rebooting!\n");
		system_upgrade_reboot();
	} else {
		INFO("OTA Failed!\n");
	}
}


/**
 * 	WIFI连接回调
 */
void wifi_connect_cb(void){
	INFO("wifi connect!\r\n");
	MQTT_Connect(&mqttClient);
}


/**
 * 	WIFI断开回调
 */
void wifi_disconnect_cb(void){
	INFO("wifi disconnect!\r\n");

	MQTT_Disconnect(&mqttClient);
}


/**
 * 	MQTT连接回调
 */
void mqttConnectedCb(uint32_t *args) {

	MQTT_Client* client = (MQTT_Client*) args;
	INFO("MQTT: Connected\r\n");
	MQTT_Publish(client, birth_topic, "online", os_strlen("online"), 0,0);
	MQTT_Subscribe(client,ota_topic, 0);
	MQTT_Subscribe(client,control_topic, 0);

	if(updata_status_check()){
		MQTT_Publish(client, ota_topic, "updata_finish", os_strlen("updata_finish"), 0,0);
	}
}


/**
 * 	MQTT接收数据回调
 */
void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len,
				const char *data, uint32_t data_len) {

	char *topicBuf = (char*) os_zalloc(topic_len + 1), 
		 *dataBuf =	 (char*) os_zalloc(data_len + 1);		

	MQTT_Client* client = (MQTT_Client*) args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);

	//data = {"url"="http://yourdomain.com:9001/ota/"}
	if (os_strcmp(topicBuf, ota_topic) == 0) {
		char url_data[200];
		if(get_josn_str(dataBuf,"url",url_data)){
			INFO("ota_start\n");
			MQTT_Publish(client, ota_topic, "ota_start", os_strlen("ota_start"), 0,0);
			ota_upgrade(url_data,ota_finished_callback);
		}
	}
	else if (os_strcmp(topicBuf, control_topic) == 0) {
		//D 1 123T123
		if (dataBuf[0]=='D')
		{
			u8 id_num[3]={0};
			INFO("[INFO]DELETE\r\n");
			if (dataBuf[1]=='1' && data_len==5 && dataBuf[2]<='2')
			{
				u8 id_num[4]={0};
				u8 id;
				memcpy(id_num,dataBuf+2,data_len-2);
				id = atoi(id_num);
				fp_delete(id,id);
				INFO("[INFO]Delete user id:%d\r\n",id);
			}else if (dataBuf[1]=='2' && data_len==9 && dataBuf[5]=='T')
			{
				u8 id_s,id_end;
				memcpy(id_num,dataBuf+2,3);
				id_s = atoi(id_num);
				memcpy(id_num,dataBuf+6,3);
				id_end = atoi(id_num);
				fp_delete(id_s,id_end);
				INFO("[INFO]Delete user id:%d-%d\r\n",id_s,id_end);
			}	
		}else if (dataBuf[0]=='O' && dataBuf[1] =='P')
		{
			LOCK_ON();
		}else if (dataBuf[0]=='R')
		{
			fp_register();
		}else if(dataBuf[0]=='K'){
			lock_info.dergee_open = atoi(dataBuf+1);
			LOCK_SET(lock_info.dergee_open,0,0);
			spi_flash_erase_sector(USER_INFO_ADDR);					// 擦除0x7C扇区
			spi_flash_write((USER_INFO_ADDR) * SPI_FLASH_SEC_SIZE,	// 参数扇区标志写入0x7C
						(uint32 *)&lock_info, sizeof(server_lock_info)); 
		}else if(dataBuf[0]=='G'){
			lock_info.dergee_close = atoi(dataBuf+1);
			LOCK_SET(0,lock_info.dergee_close,0);
			spi_flash_erase_sector(USER_INFO_ADDR);					// 擦除0x7C扇区
			spi_flash_write((USER_INFO_ADDR) * SPI_FLASH_SEC_SIZE,	// 参数扇区标志写入0x7C
						(uint32 *)&lock_info, sizeof(server_lock_info)); 
		}else if(dataBuf[0]=='T'){
			lock_info.time_open = atoi(dataBuf+1);
			LOCK_SET(0,0,lock_info.time_open);
			spi_flash_erase_sector(USER_INFO_ADDR);					// 擦除0x7C扇区
			spi_flash_write((USER_INFO_ADDR) * SPI_FLASH_SEC_SIZE,	// 参数扇区标志写入0x7C
						(uint32 *)&lock_info, sizeof(server_lock_info)); 
		}
		
		

	}

	os_free(topicBuf);
	os_free(dataBuf);
}


/**
 * 指纹回调
 */
void ICACHE_FLASH_ATTR fingerprintCb(FprintStatus status,u8 ID){
	switch (status)
	{
	u8 mqttSendData[32]={0};
	case VERIFY_SUCCESS:
		os_sprintf(mqttSendData,"VS:%03d",ID);
		LOCK_ON();
		MQTT_Publish(&mqttClient, status_topic, mqttSendData, os_strlen(mqttSendData), 0,0);
		break;
	case VERIFY_FAIL:
		os_sprintf(mqttSendData,"VF");
		MQTT_Publish(&mqttClient, status_topic, mqttSendData, os_strlen(mqttSendData), 0,0);
		break;
	case REGISTER_SUCCESS:
		INFO("[INFO]REGISTER SUCCESS\r\n");
		os_sprintf(mqttSendData,"RS:%03d",ID);
		MQTT_Publish(&mqttClient, status_topic, mqttSendData, os_strlen(mqttSendData), 0,0);
		break;
	case REGISTER_FAIL:
		INFO("[INFO]REGISTER FAIL\r\n");
		break;
	case DELETE_SUCCESS:
		os_sprintf(mqttSendData,"DS");
		INFO("[INFO]DELETE SUCCESS\r\n");
		MQTT_Publish(&mqttClient, status_topic, mqttSendData, os_strlen(mqttSendData), 0,0);
		break;
	case DELETE_FAIL:
		INFO("[INFO]DELETE FAIL\r\n");
		break;
	default:
		break;
	}
}


/**
 * 	MQTT初始化
 */
void ICACHE_FLASH_ATTR mqtt_init(void) {

	MQTT_InitConnection(&mqttClient, MQTT_HOST, MQTT_PORT, DEFAULT_SECURITY);
	MQTT_InitClient(&mqttClient, mac_str, MQTT_USER,MQTT_PASS, MQTT_KEEPALIVE, 1);
	MQTT_InitLWT(&mqttClient, lwt_topic, LWT_MESSAGE, 0, 0);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);
}


/**
 * 获取MAC|生成主题
 */
void ICACHE_FLASH_ATTR get_mac(void) {

	u8 i;
	u8 mac[6];
	wifi_get_macaddr(STATION_IF, mac);
	HexToStr(mac_str, mac, 6, 1);
	INFO("mac:%s\n", mac_str);

    os_sprintf(ota_topic,OTA_TOPIC,mac_str);
	os_sprintf(lwt_topic,LWT_TOPIC,mac_str);
    os_sprintf(birth_topic,BIRTH_TOPIC,mac_str);

    os_sprintf(control_topic,"/ZWL1/%s/switch/set",mac_str);
    os_sprintf(status_topic,"/ZWL1/%s/status",mac_str);
    
}


/**
 * 短按回调
 */
void ICACHE_FLASH_ATTR key0_short(void){
	INFO("keyShortPress\n");
}

/**
 * 长按回调
 */
void ICACHE_FLASH_ATTR key0_long(void){
    INFO("keyLongPress\n");
    start_smartconfig(smartconfig_cd);
}


void user_init(void) {

	//设备初始化
	uart_init(115200, 115200);
	os_delay_us(60000);
	userbin_check();
	get_mac();

	//WIFI Init
	wifi_set_opmode(STATION_MODE); 
	wifi_station_ap_number_set(2);
	set_wifistate_cb(wifi_connect_cb, wifi_disconnect_cb);
	

	mqtt_init();

	//按键初始化
	set_key_num(1);
	key_add(D2,key0_long,key0_short);

	fprint1016_init(fingerprintCb);

	spi_flash_read(USER_INFO_ADDR * SPI_FLASH_SEC_SIZE,(uint32 *)&lock_info,sizeof(server_lock_info));
	INFO("\r\nDergee open:%d | Dergee close:%d | Time_open:%d \r\n",lock_info.dergee_open,lock_info.dergee_close,lock_info.time_open);
	if (lock_info.first_F != 0x69)
	{
		INFO("[INFO]FIRST BOOT\r\n");
		lock_info.first_F = 0x69;
		lock_info.dergee_close = 90;
		lock_info.dergee_open = 10;
		lock_info.time_open = 2000;
		spi_flash_erase_sector(USER_INFO_ADDR);					// 擦除0x7C扇区
		spi_flash_write((USER_INFO_ADDR) * SPI_FLASH_SEC_SIZE,	// 参数扇区标志写入0x7C
						(uint32 *)&lock_info, sizeof(server_lock_info)); 
	}
	LOCK_Init(lock_info);
	

}

uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void) {
	enum flash_size_map size_map = system_get_flash_size_map();
	uint32 rf_cal_sec = 0;

	switch (size_map) {
	case FLASH_SIZE_4M_MAP_256_256:
		rf_cal_sec = 128 - 5;
		break;

	case FLASH_SIZE_8M_MAP_512_512:
		rf_cal_sec = 256 - 5;
		break;

	case FLASH_SIZE_16M_MAP_512_512:
	case FLASH_SIZE_16M_MAP_1024_1024:
		rf_cal_sec = 512 - 5;
		break;

	case FLASH_SIZE_32M_MAP_512_512:
	case FLASH_SIZE_32M_MAP_1024_1024:
		rf_cal_sec = 1024 - 5;
		break;

	case FLASH_SIZE_64M_MAP_1024_1024:
		rf_cal_sec = 2048 - 5;
		break;
	case FLASH_SIZE_128M_MAP_1024_1024:
		rf_cal_sec = 4096 - 5;
		break;
	default:
		rf_cal_sec = 0;
		break;
	}

	return rf_cal_sec;
}
