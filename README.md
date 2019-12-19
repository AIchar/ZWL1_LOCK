# 智能门锁
## 功能
* 指纹开锁
* 智能配网
* OTA升级
## MQTT
### 配置文件 `app/include/mqtt_config.h`

* 命令主题：`/ZWL1/%s/switch/set`
    * 开锁`OP`
    * 设置开锁角度`Kxxx`(xxx：0-180)
    * 设置关锁角度`Gxxx`(xxx：0-180)
    * 设置开锁时间`Txxx`(ms)
    * 删除指定指纹`D1xxx`
    * 删除指定区间指纹`D2xxxTxxx`  

* 状态主题： `/ZWL1/%s/status`  
* OTA主题： `/esp8266/ota/%s`
    * 升级`"url"="http://yourdomain.com:9001/ota/"`
