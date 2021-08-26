#ifndef __MQTT_H__
#define __MQTT_H__


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "freertos/FreeRTOS.h"
#include <freertos/semphr.h>
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"


#include "driver/adc.h"
#include "driver/dac.h"
#include "driver/gpio.h"
#include "driver/ledc.h"


#include "esp_system.h"
#include "esp_adc_cal.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_flash_spi_init.h"
#include "nvs.h"
#include "esp_partition.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_wps.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_vfs_dev.h"
#include "esp_spiffs.h"


//SET_SDCARD
#include <sys/unistd.h>
#include <sys/stat.h>
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_slave.h"
#include "sdmmc_cmd.h"


//SET_FMB630
#include <endian.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/select.h>
//

//SET_SERIAL
#include <sys/fcntl.h>
#include <sys/errno.h>
#include "driver/uart.h"
#include "esp_intr_alloc.h"
#include "hal/uart_types.h"
#include "soc/uart_caps.h"
//

#include <esp_intr_alloc.h>
#include "esp_types.h"
#include "stdatomic.h"
#include "esp_netif.h"

//#include "crt.h"

#include "esp32/rom/ets_sys.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include <soc/rmt_struct.h>
#include <soc/dport_reg.h>
#include <soc/gpio_sig_map.h>

#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"

#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/api.h"
#include "lwip/netif.h"

#include "esp_timer.h"

//---------------------------------------

#define DEV_MASTER ((uint8_t)0)


#define sntp_srv_len   32
#define wifi_param_len 32


#pragma once


#define MAX_FILE_BUF_SIZE 1024
#define RDBYTE "rb"
#define WRBYTE "wb"
#define WRPLUS "w+"
#define RDONLY "r"
#define WRTAIL "a+"

#define BUF_SIZE 256

//***********************************************************************

#define LED_ON  1
#define LED_OFF 0

#define TIME_PERIOD 10000 //10000us = 10 ms// 1000 us = 1ms
#define SEC_FLAG_DEF (1000000 / TIME_PERIOD) //((1000 / TIME_PERIOD) * 1000)

#define GPIO_CTL_ID_PIN   15//snd_id_flag pin
#define GPIO_CON_PIN      25//led connect pin
#define GPIO_LOG_PIN      13//led tik pin
#define GPIO_WIFI_PIN      4
#define GPIO_GREEN_LED    12
#define GPIO_RED_LED      14
#define GPIO_LOCK_LED     27
#define GPIO_LOCK_PIN     26
#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_LOCK_PIN)
#define ESP_INTR_FLAG_DEFAULT 0

#define STACK_SIZE_0K5   512
#define STACK_SIZE_1K    1024
#define STACK_SIZE_1K5   STACK_SIZE_1K + STACK_SIZE_0K5//1536
#define STACK_SIZE_2K    STACK_SIZE_1K << 1
#define STACK_SIZE_2K5   STACK_SIZE_2K + STACK_SIZE_0K5//2560
#define STACK_SIZE_3K    STACK_SIZE_1K * 3
#define STACK_SIZE_4K    STACK_SIZE_1K << 2

#define STORAGE_NAMESPACE "nvs"
#define PARAM_CLIID_NAME  "cliid"
#define PARAM_SNTP_NAME   "sntp"
#define PARAM_TZONE_NAME  "tzone"
#define PARAM_SSID_NAME   "ssid"
#define PARAM_KEY_NAME    "key"
#define PARAM_WMODE_NAME  "wmode"


#define EXAMPLE_WIFI_SSID "armLinux" //CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS "armLinux32" //CONFIG_WIFI_PASSWORD
#define max_inbuf         512

#define EXAMPLE_ESP_MAXIMUM_RETRY  5

#define sntp_server_def "pool.ntp.org"//"2.ru.pool.ntp.org"
#define sntp_tzone_def  "EET-2"


#define _10ms  1
#define _20ms  2 *_10ms
#define _30ms  3 *_10ms
#define _40ms  4 *_10ms
#define _50ms  5 *_10ms
#define _100ms 10 * _10ms
#define _150ms 15 * _10ms
#define _200ms 20 * _10ms
#define _250ms 25 * _10ms
#define _300ms 30 * _10ms
#define _350ms 35 * _10ms
#define _400ms 40 * _10ms
#define _450ms 45 * _10ms
#define _500ms 50 * _10ms
#define _550ms 55 * _10ms
#define _600ms 60 * _10ms
#define _650ms 65 * _10ms
#define _700ms 70 * _10ms
#define _750ms 75 * _10ms
#define _800ms 80 * _10ms
#define _850ms 85 * _10ms
#define _900ms 90 * _10ms
#define _950ms 95 * _10ms

#define _1s   100 * _10ms
#define _1s5  _1s + _500ms
#define _2s     2 * _1s
#define _3s     3 * _1s
#define _4s     4 * _1s
#define _5s     5 * _1s
#define _10s   10 * _1s
#define _15s   15 * _1s
#define _20s   20 * _1s
#define _25s   25 * _1s
#define _30s   30 * _1s
#define _60s   60 * _1s

#define HALF_SEC _500ms

//**************************************************************************

enum {
    OFF = 0,
    ON,
    DEBUG,
    DUMP
};

//**************************************************************************

const char *Version;

uint32_t cli_id;

uint8_t wmode;
char work_ssid[wifi_param_len];

EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT;

uint8_t total_task;
volatile uint8_t restart_flag;

uint8_t setDateTimeOK;
uint32_t sntp_tmr;


//**************************************************************************

void print_msg(uint8_t with, const char *tag, const char *fmt, ...);

uint32_t get_tmr(uint32_t tm);
int check_tmr(uint32_t tm);


esp_err_t read_param(const char *param_name, void *param_data, size_t len);
esp_err_t save_param(const char *param_name, void *param_data, size_t len);

//**************************************************************************

#ifdef SET_SNTP
    #include "sntp_cli.h"

    volatile uint8_t sntp_go;
    char work_sntp[sntp_srv_len];
    char time_zone[sntp_srv_len];
#endif


#ifdef SET_ST7789
    #include "st7789.h"
    #include "fontx.h"

    #define INTERVAL 1500//400
    #define WAIT vTaskDelay(INTERVAL)
    

    #define CONFIG_WIDTH  240
    #define CONFIG_HEIGHT 240
    #define CONFIG_OFFSETX 0
    #define CONFIG_OFFSETY 0
    #define CONFIG_MOSI_GPIO 23
    #define CONFIG_SCLK_GPIO 18
    #define CONFIG_CS_GPIO -1
    #define CONFIG_DC_GPIO 21//16//19
    #define CONFIG_RESET_GPIO 22//17//15
    #define CONFIG_BL_GPIO -1

    #ifdef SET_PARK_CLI
        typedef struct {
            uint16_t pos_x;
            uint16_t pos_y; 
            FontxFile *fnt;
            uint8_t fnt_w;
            uint8_t fnt_h;
        } scr_data_t;
    #endif

#endif

#ifdef SET_PARK_CLI
    #define SRV_IP_DEF "192.168.0.101"//"92.53.65.4"


    #define SRV_PORT_DEF 9889

    #define CMD_MARKER0 0x8a
    #define CMD_MARKER1 0xff
    #define NONE 255   

    #define EMPTY 0
    #define BUSY 1 

    #define zOPEN 1 // clen = 3, alen =  0, Команда «Разблокировать замок»
    #define zTOUT 2 // clen = 4, alen =  0, Команда «Задать время изъятия транспорта»  
    #define zOUT  3 // clen = 4, alen =  0, Команда «Выдать транспорт»
    #define zTIZ  4 // clen = 4, alen =  0, Команда «Задать время выдачи транспорта»    
    #define zSTAT 5 // clen = 3, alen = 10, Команда «Передать флаги состояния устройств замка»    
    #define zLED  6 // clen = 4, alen =  0, Команда «Включение индикации «Мигание: Красный-зелёный»»

    //#define cMODE 0xbf // clen = 6, alen = 0, Команда «Включить режим прямого управления»
    #define cSTAT 0xb2 // clen = 6, alen = 18, Команда «Передать состояние Promix-CN.LN.01»
    #define aSTAT 0xb1 // clen = 6, alen = 4+...,  Команда «Передать текущее состояние СКУД»  
    #define cEVT 0xb8  // clen = 7, alen = 0, Команда «Установить вариант передачи событий»    

    #define zBIT0_OPEN     1//состояние замка (1- открыт, 0- закрыт),    
    #define zBIT0_CLOSE    0//состояние замка (1- открыт, 0- закрыт),    
    #define zBIT1_SET      2//Бит 1 – режим работы замка (1 – режим установки, 0 – режим хранения)
    #define zBIT1_STY      0//Бит 1 – режим работы замка (1 – режим установки, 0 – режим хранения)
    #define zBIT2_ID       4//Бит 2 – наличие идентификатора транспорта (1- вставлен, 0- отсутствует),
    #define zBIT2_NOID     0//Бит 2 – наличие идентификатора транспорта (1- вставлен, 0- отсутствует),    
    #define zBIT7_EXEC  0x80//Бит 7 – состояние включения замка (1- запуск, 0- работа),
    #define zBIT7_WORK     0//Бит 7 – состояние включения замка (1- запуск, 0- работа),  

    #define LOCK_ONLINE    1
    #define LOCK_OFFLINE   0

    #define MAX_CMD 8
    #define MIN_CMD_LEN 3
    #define MID_CMD_LEN 4    
    #define MAX_CMD_LEN 6    

    enum {
        ZERO = 0,
        OPEN_ZAMOK,
        TOUT_ZAMOK,
        OUT_ZAMOK,
        TIZ_ZAMOK,
        STAT_ZAMOK,
        LED_ZAMOK,
        STAT_CTL,
        STAT_ALL,
        EVT_CTL
    };

    #pragma pack(push,1)
    typedef struct
    {
        unsigned char alen;
        unsigned char dlen;
        unsigned char data[6];
        char name[12];
    } s_one_cmd;
    #pragma pack(pop)

    #pragma pack(push,1)
    typedef struct {
        uint8_t marker;
        uint8_t id;
        uint8_t cmd;
        uint8_t param;
        //uint8_t cmd;
        //uint8_t id;
        //uint8_t marker;  
    } cmd_min_t;  
    #pragma pack(pop)

    #pragma pack(push,1)
    typedef struct {
        uint32_t marker;
        uint8_t prefix;
        uint8_t cmd;
        uint8_t param;
        //uint8_t cmd;
        //uint8_t prefix;
        //uint32_t marker;  
    } cmd_max_t;
    #pragma pack(pop)



    #define MARKER_ID 0x40
    #define CTL_ID 333

    #pragma pack(push,1)
    typedef struct {
        uint8_t marker;
        uint32_t id;
    } s_ctl_id_t;
    #pragma pack(pop)



#endif        

#endif /* */
