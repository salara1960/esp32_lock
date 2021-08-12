/*
    Программый эмулятор сетевого контроллера Promix-CN.LN.01
    с замком типа Promix-SM307.10.3
    Реализована частичная эмуляция - все команды замка и некоторые команды контроллеру.
    Эмулятор позволяет ставить транспортное средство на хранение,
    с оповещением сервера управления о таком событии, а также "выдавать"
    с хранения транспортное средство.
*/

#include "hdr.h"
#include "main.h"

#include "../version.c"


//*******************************************************************

volatile uint8_t restart_flag = 0;

uint8_t total_task = 0;
uint8_t last_cmd = 255;

uint8_t dbg = ON;

uint32_t tiks = 0;

char stk[128] = {0};

static const char *TAG = "MAIN";
static const char *TAGN = "NVS";
static const char *TAGT = "VFS";

#ifdef SET_WIFI
    static const char *TAGW = "WIFI";
    static int s_retry_num = 0;
#endif    

static const char *wmode_name[] = {"NULL", "STA", "AP", "APSTA", "MAX"};
uint8_t wmode = WIFI_MODE_STA;
static unsigned char wifi_param_ready = 0;
char work_ssid[wifi_param_len] = {0};
static char work_pass[wifi_param_len] = {0};
bool show_ip = false;

uint8_t setDateTimeOK = 0;
uint32_t sntp_tmr = 0;
#ifdef SET_SNTP
    volatile uint8_t sntp_go = 0;

    char work_sntp[sntp_srv_len] = {0};
    char time_zone[sntp_srv_len] = {0};
#endif

EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

xSemaphoreHandle prn_mutex;

uint8_t *macs = NULL;

static char sta_mac[18] = {0};
uint32_t cli_id = 0;
char localip[32] = {0};
esp_ip4_addr_t ip4;
ip_addr_t bca;

static uint32_t seconda = 0;

static uint32_t varta = 0;

static uint16_t secFlag = SEC_FLAG_DEF;//8000;//1000;//100;//10
static uint8_t led = LED_OFF;
uint32_t tperiod = TIME_PERIOD;//125us = 8KHz;//1000us = 1ms//10000us = 10 ms //100000 us = 100 ms

#ifdef SET_PARK_CLI
    static const char *TAGCLI = "CLI";   
    xQueueHandle scr_queue = NULL;
    uint8_t cmdLeds = LED_OFF;
    uint8_t cmdLedsEnable = 0;
    uint32_t half_sec = HALF_SEC;//0,5 sec

    static uint8_t online = 0;
    int connsocket = -1;
    uint8_t lock_status = zBIT0_OPEN;
    uint8_t evt_auto = 1;
    uint32_t tmr_evt = 0;
    const char *lstatus[] = {"BUSY", "EMPTY"};

    uint8_t ctl_num_part = 5; //  номер Promix-CN.LN.01 (секции)
    uint32_t rfid = 1234;

    const s_one_cmd all_cmds[MAX_CMD] = {
        { 0, 3, {0x8a, 0, 1, 0, 0, 0, 0}, "OPEN_ZAMOK"}, // OPEN_ZAMOK = 1 //Команда «Разблокировать замок»
        { 0, 4, {0x8a, 0, 2, 0, 0, 0, 0}, "TOUT_ZAMOK"}, // TOUT_ZAMOK = 2//Команда «Задать время изъятия транспорта»
                                                      // Диапазон значения 4-го байта: 01Н-40Н, интервал времени: 0.5-32сек, шаг – 0.5сек.
        { 0, 4, {0x8a, 0, 3, 0, 0, 0, 0}, "OUT_ZAMOK"},  // OUT_ZAMOK = 3//Команда «Выдать транспорт»
                                                      // 4-й байт : 00Н – прекратить выдачу транспорта, 01Н – выдать транспорт.
        { 0, 4, {0x8a, 0, 4, 0, 0, 0, 0}, "TIZ_ZAMOK"},  // TIZ_ZAMOK = 4// Команда «Задать время выдачи транспорта»
                                                      // Диапазон значения 4-го байта: 01Н-F0Н, интервал времени: 5-1200сек(20мин), шаг–5сек.        
        {10, 3, {0x8a, 0, 5, 0, 0, 0, 0}, "STAT_ZAMOK"}, // STAT_ZAMOK = 5 //Команда «Передать флаги состояния устройств замка»
        { 0, 4, {0x8a, 0, 6, 0, 0, 0, 0}, "LED_ZAMOK"},  // LED_ZAMOK = 6 //Команда «Включение индикации «Мигание: Красный-зелёный»»
                                                      // 4-й байт : 1 - вкл., 0 - выкл.
        { 0, 6, {0xff, 0xff, 0xff, 0xff, 0x9a, 0xbf, 0}, "MODE_CTL"},// MODE_CTL = 7 //Команда «Включить режим прямого управления»
        {18, 6, {0xff, 0xff, 0xff, 0xff, 0x9a, 0xb2, 0}, "STAT_CTL"}, // STAT_CTL = 8 // Команда «Передать состояние Promix-CN.LN.01»
        { 0, 7, {0xff, 0xff, 0xff, 0xff, 0x9a, 0xb8, 1}, "EVT_CTL"} // EVT_CTL = 9 // Команда «Установить вариант передачи событий»
    };

    xQueueHandle evt_queue = NULL;

    //

    static void IRAM_ATTR pin_trap(void *arg)//эмулятор защелки на замке, срабатывание - транспорт установлен на хранение
    {
        if (tmr_evt) {
            if (check_tmr(tmr_evt)) tmr_evt = 0; else return;
        }
        uint32_t gpio_num = (uint32_t)arg;
        if (gpio_num == GPIO_LOCK_PIN) {
            if (!tmr_evt) {
                tmr_evt = get_tmr(_1s);
                lock_status = zBIT0_CLOSE;//закрыть замок !
                gpio_set_level(GPIO_LOCK_LED, lock_status);
                if (evt_queue) xQueueSendFromISR(evt_queue, &lock_status, NULL);
            }
        }
    }

#endif

//------------------------------------------------------------------------------------

char *sec_to_str_time(char *stx);

//--------------------------------------------------------------------------------------
// Функция выводит на устройство stdout текстовую строку
// (с гарантией преодолнния колизии при одновременном вызове из разных параллельных процессов)
//
void print_msg(uint8_t with, const char *tag, const char *fmt, ...)
{
size_t len = BUF_SIZE << 1;//1024

    if (dbg == OFF) return;

    char *st = (char *)calloc(1, len);
    if (st) {
        if (xSemaphoreTake(prn_mutex, portMAX_DELAY) == pdTRUE) {
            int dl = 0, sz;
            va_list args;
            if (with) {
                struct tm *ctimka;
                time_t it_ct = time(NULL);
                ctimka = localtime(&it_ct);
                dl = sprintf(st, "%02d.%02d %02d:%02d:%02d ",
                                 ctimka->tm_mday,ctimka->tm_mon + 1,ctimka->tm_hour,ctimka->tm_min,ctimka->tm_sec);
            }
            if (tag) dl += sprintf(st+strlen(st), "[%s] ", tag);
            sz = dl;
            va_start(args, fmt);
            sz += vsnprintf(st + dl, len - dl, fmt, args);
            printf(st);
            va_end(args);
            xSemaphoreGive(prn_mutex);
        }
        free(st);
    }
}
//----------------------------------------------------------------------------------------------
#ifdef SET_ST7789
    //
    static const char *TAGFFS = "FFS";

    // Функция читает информацию о файлах у файлового раздела флэш-диска
    //
    static void SPIFFS_Directory(char * path)
    {
        DIR *dir = opendir(path);
        if (!dir) return;

        while (true) {
            struct dirent *pe = readdir(dir);
            if (!pe) break;
            print_msg(0, TAGFFS, "d_name=%s d_ino=%d d_type=%x\n", pe->d_name,pe->d_ino, pe->d_type);
        }
        closedir(dir);
    }
    //
#endif

//***************************************************************************************************************

esp_err_t read_param(const char *param_name, void *param_data, size_t len);
esp_err_t save_param(const char *param_name, void *param_data, size_t len);

//***************************************************************************************************************

//--------------------------------------------------------------------------------------
static uint32_t get_varta()
{
    return varta;
}
//--------------------------------------------------------------------------------------
// CallBack функция периодического таймера управляет светодиодами
// Интервал таймера - 10ms
// 
static void periodic_timer_callback(void *arg)
{
    varta++; //10ms period = 100 Hz
    //
#ifdef SET_PARK_CLI
    half_sec--;
    if (!half_sec) {
        half_sec = HALF_SEC;
        if (cmdLedsEnable) {   
            gpio_set_level(GPIO_GREEN_LED, cmdLeds);
            cmdLeds = ~cmdLeds;
            gpio_set_level(GPIO_RED_LED, cmdLeds);
        } else {
            if (cmdLeds != LED_OFF) {
            cmdLeds = LED_OFF;
                gpio_set_level(GPIO_GREEN_LED, cmdLeds);
                gpio_set_level(GPIO_RED_LED, cmdLeds);
            }
        }
    }
#endif    
    //
    secFlag--;
    if (!secFlag) {
        seconda++;
        secFlag = SEC_FLAG_DEF;
        led = ~led;
        gpio_set_level(GPIO_LOG_PIN, led);
    }
}
//--------------------------------------------------------------------------------------
// Функция установки временной задержки
//
uint32_t get_tmr(uint32_t tm)
{
    return (get_varta() + tm);
}
//--------------------------------------------------------------------------------------
// Функция проверки временной задержки 
//
int check_tmr(uint32_t tm)
{
    if (!tm) return 0;

    return (get_varta() >= tm ? 1 : 0);
}
//------------------------------------------------------------------------------------------------------------
#ifdef SET_WIFI
// Функции обслуживания WI-FI транспорта
//
bool check_pin(uint8_t pin_num)
{
    gpio_pad_select_gpio(pin_num);
    gpio_pad_pullup(pin_num);
    if (gpio_set_direction(pin_num, GPIO_MODE_INPUT) != ESP_OK) return true;

    return (bool)(gpio_get_level(pin_num));
}
//------------------------------------------------------------------------------------------------------------
const char *wifi_auth_type(wifi_auth_mode_t m)
{

    switch (m) {
        case WIFI_AUTH_OPEN:// = 0
            return "AUTH_OPEN";
        case WIFI_AUTH_WEP:
            return "AUTH_WEP";
        case WIFI_AUTH_WPA_PSK:
            return "AUTH_WPA_PSK";
        case WIFI_AUTH_WPA2_PSK:
            return "AUTH_WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "AUTH_WPA_WPA2_PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE:
            return "AUTH_WPA2_ENTERPRISE";
        default:
            return "AUTH_UNKNOWN";
    }

}
//------------------------------------------------------------------------------------------------------------
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START :
                esp_wifi_connect();
            break;
            case WIFI_EVENT_STA_DISCONNECTED :
                if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
                    esp_wifi_connect();

                    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);

                    print_msg(1, TAGW, "Retry to connect to the AP\n");
                } else print_msg(1, TAGW, "Connect to the AP fail\n");
                memset(localip, 0, sizeof(localip));
            break;
            case WIFI_EVENT_STA_CONNECTED :
            {
                wifi_ap_record_t wd;
                if (!esp_wifi_sta_get_ap_info(&wd)) {
                    print_msg(1, TAGW, "Connected to AP '%s' auth(%u):'%s' chan:%u rssi:%d\n",
                               (char *)wd.ssid,
                               (uint8_t)wd.authmode, wifi_auth_type(wd.authmode),
                               wd.primary, wd.rssi);

                }
            }
            break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP :
            {
                ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                sprintf(localip, IPSTR, IP2STR(&event->ip_info.ip));
                ip4 = event->ip_info.ip;
                bca.u_addr.ip4.addr = ip4.addr | 0xff000000;
                print_msg(1, TAGW, "Local ip_addr : %s\n", localip);
                s_retry_num = 0;

                xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            }
            break;
        };
    }

}
//------------------------------------------------------------------------------------------------------------
void initialize_wifi(wifi_mode_t w_mode)
{

    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
         if (w_mode == WIFI_MODE_STA) esp_netif_create_default_wifi_sta();
    else if (w_mode == WIFI_MODE_AP) esp_netif_create_default_wifi_ap();
    else {
        print_msg(0, TAGW, "Unknown wi-fi mode - %d | FreeMem %u\n", w_mode, xPortGetFreeHeapSize());
        return;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(w_mode));


    switch ((uint8_t)w_mode) {
        case WIFI_MODE_STA :
        {
           ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,    &event_handler, NULL));
           ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, &event_handler, NULL));

            wifi_config_t wifi_config;
            memset((uint8_t *)&wifi_config, 0, sizeof(wifi_config_t));
            if (wifi_param_ready) {
                memcpy(wifi_config.sta.ssid, work_ssid, strlen(work_ssid));
                memcpy(wifi_config.sta.password, work_pass, strlen(work_pass));
                ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
                print_msg(0, TAGW, "WIFI_MODE - STA: '%s':'%s'\n", wifi_config.sta.ssid, wifi_config.sta.password);
            }
            ESP_ERROR_CHECK(esp_wifi_start());
        }
        break;
        case WIFI_MODE_AP:
        {
            ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

            wifi_config_t wifi_configap;
            memset((uint8_t *)&wifi_configap, 0, sizeof(wifi_config_t));
            wifi_configap.ap.ssid_len = strlen(sta_mac);
            memcpy(wifi_configap.ap.ssid, sta_mac, wifi_configap.ap.ssid_len);
            memcpy(wifi_configap.ap.password, work_pass, strlen(work_pass));
            wifi_configap.ap.channel = 6;
            wifi_configap.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;//WIFI_AUTH_WPA_WPA2_PSK
            wifi_configap.ap.ssid_hidden = 0;
            wifi_configap.ap.max_connection = 4;
            wifi_configap.ap.beacon_interval = 100;
            ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_configap));
            ESP_ERROR_CHECK(esp_wifi_start());
            print_msg(0, TAGW, "WIFI_MODE - AP: '%s':'%s'\n", wifi_configap.ap.ssid, work_pass);
        }
        break;
    }
}

#endif
//********************************************************************************************************************
// Функция чтения данных из NVS-области data-flash
//
esp_err_t read_param(const char *param_name, void *param_data, size_t len)//, int8_t type)
{
nvs_handle mhd;

    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &mhd);
    if (err != ESP_OK) {
        ESP_LOGE(TAGN, "%s(%s): Error open '%s'", __func__, param_name, STORAGE_NAMESPACE);
    } else {//OK
        err = nvs_get_blob(mhd, param_name, param_data, &len);
        if (err != ESP_OK) {
            ESP_LOGE(TAGN, "%s: Error read '%s' from '%s'", __func__, param_name, STORAGE_NAMESPACE);
        }
        nvs_close(mhd);
    }

    return err;
}
//--------------------------------------------------------------------------------------------------
// Функция записи данных в NVS-область data-flash
//
esp_err_t save_param(const char *param_name, void *param_data, size_t len)
{
nvs_handle mhd;

    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &mhd);
    if (err != ESP_OK) {
        ESP_LOGE(TAGN, "%s(%s): Error open '%s'", __func__, param_name, STORAGE_NAMESPACE);
    } else {
        err = nvs_set_blob(mhd, param_name, (uint8_t *)param_data, len);
        if (err != ESP_OK) {
            ESP_LOGE(TAGN, "%s: Error save '%s' with len %u to '%s'", __func__, param_name, len, STORAGE_NAMESPACE);
        } else err = nvs_commit(mhd);
        nvs_close(mhd);
    }

    return err;
}
//------------------------------------------------------------------------------------
// Функция формирует текстовую строку с текущими датой и временем
//
char *sec_to_str_time(char *stx)
{
    if (setDateTimeOK) {
        time_t ct = time(NULL);
        struct tm *timka = localtime(&ct);
        sprintf(stx, "%02d.%02d %02d:%02d:%02d",
                    timka->tm_mday, timka->tm_mon + 1,
                    timka->tm_hour, timka->tm_min, timka->tm_sec);
    } else {
        uint32_t s = seconda;

        uint32_t day = s / (60 * 60 * 24);
        s %= (60 * 60 * 24);
        uint32_t hour = s / (60 * 60);
        s %= (60 * 60);
        uint32_t min = s / (60);
        s %= 60;
        sprintf(stx, "%03u.%02u:%02u:%02u", day, hour, min, s);
    }

    return stx;
}
//------------------------------------------------------------------------------------

#ifdef SET_PARK_CLI
// Функция помещает в очередь указатель на текстовую строку в динамической области ОЗУ 
//
void toScr(const char *st)
{
    if (!st || !scr_queue) return;
    char *tp = (char *)calloc(1, strlen(st) + 1);
    if (tp) {
        strcpy(tp, st);
        if (xQueueSend(scr_queue, (void *)&tp, (TickType_t)0) != pdPASS) {
            ESP_LOGE(TAGCLI, "Error while sending to scr_queue");
            free(tp);
        }
    }    
}
//-------------------------------------------
// Функция подсчитывает контрольную сумму по
// алгоритму простого арифметического сумирования
// в 16-ти разрядной сетке
//
uint16_t calcCRC(uint8_t *buf, int len)
{
uint16_t ret = 0;

    for (int i = 0; i < len; i++) ret += buf[i];

    return ret;
}
//-------------------------------------------
// Функция формирует ответ на команду в выходном буфере
//
int mkAck(const uint8_t code, uint8_t *buf)
{
int ret = 0;

    switch (code) {
        case cSTAT: //18 byte
            memset(buf, 0, 20);
            *buf = 0x9B; // заголовок ответа Promix-CN.LN.01 заголовок команды,
            *(buf + 1) = 0xB2; // заголовок команды
            *(buf + 2) = ctl_num_part;
            *(buf + 3) = 0x10; // байт флагов режимов работы Promix-CN.LN.01:
                               // X0h – режим определения СКУД,
                               // X1h – режим сканирования,
                               // 0Xh – вариант отправки событий по запросу,
                               // 1Xh – вариант автоматической отправки событий,
            *(buf + 4) = 0;    // байты 4-5 – информация о включенных контроллерах (см.п.4.5) -  00h – старший байт
            *(buf + 5) = 0x10; // b B 7 B6 B5 B4 B3 B2 B1 B0 – младший байт, (контроллер Promix-CN.RD.01)
                               // бит 0 – B0 – контроллер Promix-CN.PR.04,
                               // бит 1 – B1 – зарезервировано,
                               // бит 2 – B2 – контроллер Promix-CN.RD.01,
                               // бит 3 – B3 – контроллер Promix-CN.PR.08,
                               // бит 4 – B4 – замок Promix-SM307,
                               // 0 – опрос типа устройства выключен, 1 – опрос включён
            *(buf + 6) = 0;    // байт 6 – XXh – время активации механизмов в режиме нештатной ситуации
            //     байты 7-17 – информация о количестве найденных контроллерах в секции:
            *(buf + 7) = 0x82; // байт 7 – 82h – указатель количества контроллеров Promix-CN.PR.04,
            // байт 8 – количество контроллеров Promix-CN.PR.04,
            *(buf + 9) = 0x84; // байт 9 – 84h – зарезервировано,
            // байты 10-11 – зарезервировано,
            *(buf + 12) = 0x86;// байт 12 – 86h – указатель количества контроллеров Promix-CN.RD.01,
            *(buf + 13) = 1;   // байт 13 – количество контроллеров Promix-CN.RD.01,
            *(buf + 14) = 0x88;// байт 14 – 88h – указатель количества контроллеров Promix-CN.PR.08,
            // байт 15 – количество контроллеров Promix-CN.PR.08,
            *(buf + 16) = 0x8A;// байт 16 – 8Аh – указатель количества замков Promix-SM307,
            *(buf + 17) = 1;   // байт 17 – количество замков Promix-SM307.
            ret = 18;
            break;
        case zSTAT: //10 byte // 8b 00 05 04 00 00 04 d2 01 6a
            *buf = 0x8b;
            ret += MIN_CMD_LEN;
            *(buf + ret) = (uint8_t)(/*zBIT0_OPEN*/lock_status | zBIT2_ID);//флаги состояния
            //байт 4 – байт 0 (старший) номера идентификатора транспорта,
            //байт 5 – байт 1 номера идентификатора транспорта,
            //байт 6 – байт 2 номера идентификатора транспорта,
            //байт 7 – байт 3 (младший) идентификатора транспорта.
            rfid = htonl(rfid);
            ret++;
            memcpy(buf + ret, &rfid, sizeof(uint32_t));
            ret += sizeof(uint32_t);
            uint16_t crc = calcCRC(buf, ret);
            crc = htons(crc);
            memcpy(buf + ret, &crc, sizeof(uint16_t));
            ret += sizeof(uint16_t);
            break;
    }

    return ret;
}
//-------------------------------------------
// Функция отправляет серверу сообщение (event) об изменении 
// статуса замка из состояния EMPTY в состояние BUSY
//
uint8_t sndEvt()
{    
char tp[128];    
uint8_t evtBuf[64];
uint8_t er = 0;

    // create event = ack for zSTAT command
    memcpy(evtBuf, all_cmds[4].data, all_cmds[4].dlen);
    int dl = mkAck(zSTAT, evtBuf);
    if (dl) {
        if (send(connsocket, evtBuf, dl, MSG_DONTWAIT) != dl) er = 0x80;
        sprintf(tp, "event (%d):", dl);
        for (int i = 0; i < dl; i++) sprintf(tp+strlen(tp), " %02X", evtBuf[i]);
        print_msg(1, TAGCLI, "%s\n", tp);
    }

    return er;
}
//-------------------------------------------
// Нитка (thread) обслуживает tcp-соединение с сервером управления :
// - прием команд и формирование ответов на команды,
//   если таковые предусмотрены;
// - изменение внутреннего состояния замка по
//   внешнему событию или по соответствующей команде
//
void net_ctl_task(void *arg)
{
total_task++;

scr_data_t *sdata = (scr_data_t *)arg;

uint32_t net_id = cli_id;//100;

char line[64] = {0};
uint16_t tcp_port;
struct sockaddr_in srv_conn;
socklen_t srvlen;
int i = 0, dl, lenr = 0, lenr_tmp = 0, ind = 0;
size_t resa;
uint8_t err = 0, first = 1, rdy = 0, Vixod = 0;
uint8_t start_byte = 0, cmd = NONE, wait_len = 3, ack_len = 0, good = 0;
cmd_min_t *cmd_min = NULL;
cmd_max_t *cmd_max = NULL;
uint32_t tmr_cmd = 0;

uint8_t faza = 0;

struct timeval cli_tv;
fd_set read_Fds;

char *tmp = NULL;
uint8_t *to_server = NULL, *from_server = NULL;
uint32_t try = 0;


    // Ждем полного "подъема" wh-fi канала
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);


    print_msg(1, TAGCLI, "Start net_ctl task | FreeMem %u\n", xPortGetFreeHeapSize());

    if (!sdata) goto done;

    // Выделяем память из динамической области для рабочих буферов
    to_server   = (uint8_t *)calloc(1, BUF_SIZE);   if (!to_server)   goto done;
    from_server = (uint8_t *)calloc(1, BUF_SIZE);   if (!from_server) goto done;
    tmp         = (char *)calloc(1, BUF_SIZE << 1); if (!tmp)         goto done;

    strcpy(line, SRV_IP_DEF);
    tcp_port = SRV_PORT_DEF;

    print_msg(1, TAGCLI, "NET_CTL_CLI:\n\tsrv=%s:%u\n\tnet_id=%u\n", line, tcp_port, net_id);


    while (!restart_flag) {


        if (connsocket < 0) {
            connsocket = socket(AF_INET, SOCK_STREAM, 6);// Создаем сокет
            if (connsocket < 0) {
                print_msg(1, TAGCLI, "FATAL ERROR: open socket (%d)\n", connsocket);
                vTaskDelay(1000 / portTICK_RATE_MS);
                continue;
            }
            srvlen = sizeof(struct sockaddr);
            memset(&srv_conn, 0, srvlen);
            srv_conn.sin_family = AF_INET;
            srv_conn.sin_port = htons(tcp_port);
            srv_conn.sin_addr.s_addr = inet_addr(line);
        }

        try++;
        if (connect(connsocket, (struct sockaddr *)&srv_conn, srvlen) < 0) {// Пытаемся подключиться к серверу по tcp-протоколу
            print_msg(1, TAGCLI, "ERROR: connect to %s:%u (%u). Wait 10 sec. before new connect...\n ", line, tcp_port, try);
            close(connsocket);
            connsocket = -1;
#ifdef SET_ST7789
            sprintf(tmp, "Connect Error %u", try);
            toScr(tmp);
            //lcdDrawString(&dev, sdata->fnt, sdata->pos_x, sdata->pos_y, (uint8_t *)mkLineCenter(tmp, sdata->fnt_w, NULL), GREEN);
#endif
            vTaskDelay(10000 / portTICK_RATE_MS);
            continue;
        } else {
            //gpio_set_level(GPIO_GPS_PIN, LED_ON);
            print_msg(1, TAGCLI, "Connect to %s:%d OK (%u)\n", line, tcp_port, try);
#ifdef SET_ST7789
            sprintf(tmp, "Connect OK %u", try);
            toScr(tmp);
#endif            
        }
        fcntl(connsocket, F_SETFL, (fcntl(connsocket, F_GETFL)) | O_NONBLOCK);// делаем сокет неблокирующим
        //
        online = 1;// Флаг "соединение с сервером установлено"
        gpio_set_level(GPIO_CON_PIN, LED_ON);
        // 
        memset(from_server, 0, BUF_SIZE);
        Vixod = 0; resa  = 0; lenr = 0; ind = 0;
        err  = 0; rdy = 0;
        start_byte = good = 0;
        wait_len = MIN_CMD_LEN;
        ack_len = 0;
        tmr_cmd = 0;
#ifdef SEND_CTL_ID
        faza  = 0; first = 1; 
#else 
        faza  = 1; first = 0; 
#endif       
        //

        while (!Vixod && !restart_flag) {

            switch (faza) {
                case 0 :// Блок выдачи на сервер своего ID сразу после установки соединения
                    if (first) {
                        first = 0;
                        dl = sprintf(tmp, "NET_CTL_ID=%u\n", net_id);
                        print_msg(1, TAGCLI, "%s", tmp);
                        resa = send(connsocket, tmp, dl, MSG_DONTWAIT);
                        if (resa != dl) {
                            err |= 1;
                            Vixod = 1;
                            break;
                        } else {
                            faza = 1;
#ifdef SET_ST7789
                            toScr(tmp);
                            //lcdDrawString(&dev, sdata->fnt, sdata->pos_x, sdata->pos_y, (uint8_t *)mkLineCenter(tmp, sdata->fnt_w, NULL), GREEN);
#endif
                        }    
                    }
                break;
                case 1:// Блок обслуживания tcp-соединения с сервером : send/recv. data to/from server
                    cli_tv.tv_sec = 0; cli_tv.tv_usec = 50000;
                    FD_ZERO(&read_Fds); FD_SET(connsocket, &read_Fds);
                    // Ожидаем в течении 50ms изменения в дескрипторе сокета по чтению
                    if (select(connsocket + 1, &read_Fds, NULL, NULL, &cli_tv) > 0) {
                        if (FD_ISSET(connsocket, &read_Fds)) {// есть изменения для чтения по нашему сокету

                            lenr_tmp = recv(connsocket, &from_server[ind], 1, MSG_DONTWAIT);
                            if (lenr_tmp == 0) {
                                err |= 2;
                                if (!lenr) {
                                    Vixod = 1;
                                    break;
                                } else {
                                    rdy = 1;
                                }
                            } else if (lenr_tmp > 0) {
                                lenr += lenr_tmp;
                                ind += lenr_tmp;
                                if (lenr) tmr_cmd = get_tmr(_3s);// Установить таймаут на ожидание очередного байта 
                                if (lenr == 1) {
                                    if ((from_server[0] == CMD_MARKER0) || (from_server[0] == CMD_MARKER1)) {
                                        start_byte = 1;
                                    }
                                }
                                if (start_byte) {
                                    if (lenr == MIN_CMD_LEN) {  
                                        switch (from_server[MIN_CMD_LEN - 1]) {
                                            case zLED:
                                            case zOUT:
                                            case zTOUT:
                                            case zTIZ:
                                                wait_len = 4;
                                                break;
                                            case CMD_MARKER1:
                                                wait_len = MID_CMD_LEN;
                                                break;
                                        }
                                    } 
                                    if ((lenr == MID_CMD_LEN) && (from_server[MID_CMD_LEN - 1] == cEVT)) wait_len = MAX_CMD_LEN;
                                    if (lenr == wait_len) { rdy = 1; good = 1; }
                                }
                                //if (!start_byte && lenr) rdy = 1;
                            }
                            if (lenr >= MAX_CMD_LEN) rdy = 1; 
                        }//if (FD_ISSET
                    }//if (select
                break;
            }//switch (faza)    
            //
            if (tmr_cmd) {// Если истёк интервал одидания очередного байта от сервера -> на Блок обработки команды
                if (check_tmr(tmr_cmd)) rdy = 1;
            }
            //
            if (rdy) {// Блок обработки полученной команды
                rdy = 0;
                tmr_cmd = 0;
                sprintf(tmp, "data from server (%d) :", lenr);
                for (i = 0; i < lenr; i++) sprintf(tmp+strlen(tmp), "%02X", from_server[i]);
                print_msg(1, TAGCLI, "%s\n", tmp);
                //
                ack_len = 0;
                if (good) {// команда от сервера "валидная"
                    if (lenr > 4) {
                        cmd_max = (cmd_max_t *)&from_server; 
                        cmd = cmd_max->cmd;
                        switch (cmd) {
                            case cSTAT:
                                ack_len = 18;
                                strcpy(tmp, "cmd:STAT_CTL");
                            break;    
                            case cMODE:
                                strcpy(tmp, "cmd:MODE_CTL");
                            break;
                            case cEVT:
                                strcpy(tmp, "cmd:EVT_CTL");
                                evt_auto = cmd_max->param & 1;
                            break;    
                        }
                    } else if ((lenr == 3) || (lenr == 4)) {
                        cmd_min = (cmd_min_t *)&from_server;
                        cmd = cmd_min->cmd;
                        switch (cmd) {
                            case zLED:
                                strcpy(tmp, "LED_ZAMOK ");
                                if (cmd_min->param) {
                                    strcat(tmp, "ON");
                                    cmdLedsEnable = 1;
                                } else {
                                    strcat(tmp, "OFF");
                                    cmdLedsEnable = 0;
                                }
                            break;
                            case zOPEN:
                                lock_status = zBIT0_OPEN;//1;
                                gpio_set_level(GPIO_LOCK_LED, lock_status);
                                sprintf(tmp, "OPEN_ZAMOK %u", lock_status);
                                if (evt_queue) {
                                    if (xQueueSend(evt_queue, (void *)&lock_status, (TickType_t)0) != pdPASS) {
                                        ESP_LOGE(TAGCLI, "Error while sending to evt_queue");
                                    }
                                }
                            break;
                            case zSTAT:
                                ack_len = 10;
                                strcpy(tmp, "STAT_ZAMOK");
                            break;
                            case zTOUT:
                                sprintf(tmp, "TOUT_ZAMOK %.1f", (float)(cmd_min->param * 0.5));
                            break;
                            case zOUT:
                                sprintf(tmp, "OUT_ZAMOK %u", cmd_min->param);
                            break;
                            case zTIZ:
                                sprintf(tmp, "TIZ_ZAMOK %u", cmd_min->param * 5);
                            break;

                        }
                        sprintf(tmp+strlen(tmp), " #%u", cmd_min->id & 0x1f);
                    }
                    if (lenr >= 3) {
                        toScr(tmp);
                        print_msg(1, TAGCLI, "%s\n", tmp);
                    } else {
                        toScr("Bad command");
                        print_msg(1, TAGCLI, "Bad command from server !\n");
                    }
                    //    
                    if (ack_len) {// Блок формирования и отправки на сервер ответа на команду
                        ack_len = 0;
                        dl = mkAck(cmd, to_server);
                        if (dl) {
                            strcpy(tmp, "ack: ");
                            for (i = 0; i < dl; i++) sprintf(tmp+strlen(tmp), "%02X", to_server[i]);
                            print_msg(1, TAGCLI, "%s\n", tmp);
                            resa = send(connsocket, to_server, dl, MSG_DONTWAIT);
                            if (resa != dl) {
                                err |= 0x20;
                                Vixod = 1;
                            }
                        }
                    }    
                    //
                } else {// неизвестная команда - закрываем соединение и переходим в режим ожидания нового
                    err |= 4;
                    Vixod = 1;
                }
                //
                memset(from_server, 0, BUF_SIZE); lenr = ind = 0;  
                //      
            }
            //
        }//while (!Vixod && !restart_flag)

        online = 0;
        gpio_set_level(GPIO_CON_PIN, LED_OFF);

        if (connsocket > 0) {
            shutdown(connsocket, SHUT_RDWR);
            close(connsocket);
            connsocket = -1;
        }
        try = 0;
        tmp[0] = '\0';
        if (err) {
            sprintf(tmp,"Error code 0x%02X : ", err);
            if (err&1)    strcat(tmp, "Send NET_CTL_ID to server error.");
            if (err&2)    strcat(tmp, "Server closed connection.");
            if (err&4)    strcat(tmp, "Unknown command from Server.");
            if (err&8)    strcat(tmp, "Timeout recv. data from server.");
            if (err&0x10) strcat(tmp, "Internal error.");
            if (err&0x20) strcat(tmp, "Send ack to server error.");
            if (err&0x40) strcat(tmp, "CPU reset now !");
            if (err&0x80) strcat(tmp, "Send event to server error.");
        }
        strcat(tmp, " Wait 10 sec. before new connect...\n");
        print_msg(1, TAGCLI, tmp);
#ifdef SET_ST7789
        toScr("Disconnected.");                   
#endif
        vTaskDelay(10000 / portTICK_RATE_MS);       


    }//while (!restart_flag)



    print_msg(1, TAGCLI, "net_ctl_client task stop, srv=%s:%d net_id=%s\n========================================\n",
                 line, tcp_port, net_id);


done:

    if (tmp) free(tmp);
    if (to_server) free(to_server);
    if (from_server) free(from_server);

    if (total_task) total_task--;

    vTaskDelete(NULL);

}

#endif

//************************************************************************************************************
//************************************************************************************************************
//************************************************************************************************************

void app_main()
{

    total_task = 0;

    vTaskDelay(1000 / portTICK_RATE_MS);

    // Инициализация NVS data_flash памяти
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAGN, "1: nvs_flash_init() ERROR (0x%x) !!!", err);
        nvs_flash_erase();
        err = nvs_flash_init();
        if (err != ESP_OK) {
            ESP_LOGE(TAGN, "2: nvs_flash_init() ERROR (0x%x) !!!", err);
            while (1);
        }
    }

    // Формируем ID устройства из 4-х последних байт mac-адреса wf-fi модуля
    macs = (uint8_t *)calloc(1, 6);
    if (macs) {
        esp_efuse_mac_get_default(macs);
        sprintf(sta_mac, MACSTR, MAC2STR(macs));
        memcpy(&cli_id, &macs[2], 4);
        cli_id = ntohl(cli_id);
    }

    prn_mutex = xSemaphoreCreateMutex();

    vTaskDelay(250 / portTICK_RATE_MS);

    print_msg(0, TAG, "\nApp version %s | MAC %s | SDK Version %s | FreeMem %u\n", Version, sta_mac, esp_get_idf_version(), xPortGetFreeHeapSize());

    //--------------------------------------------------------

    esp_log_level_set("wifi", ESP_LOG_WARN);

    //--------------------------------------------------------
    // Начальная инициализация пина секундного светодиода 
    gpio_pad_select_gpio(GPIO_LOG_PIN);
    gpio_pad_pullup(GPIO_LOG_PIN);
    gpio_set_direction(GPIO_LOG_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LOG_PIN, LED_OFF);
    // Начальная инициализация пина светодиода соединения с сервером
    gpio_pad_select_gpio(GPIO_CON_PIN);
    gpio_pad_pullup(GPIO_CON_PIN);
    gpio_set_direction(GPIO_CON_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_CON_PIN, LED_OFF);


    //--------------------------------------------------------
    //uint8_t rt = true;


#ifdef SET_PARK_CLI
    // Начальная инициализация пинов технологических светодиодов 
    gpio_pad_select_gpio(GPIO_GREEN_LED);
    gpio_pad_pullup(GPIO_GREEN_LED);
    gpio_set_direction(GPIO_GREEN_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_GREEN_LED, LED_OFF);
    
    gpio_pad_select_gpio(GPIO_RED_LED);
    gpio_pad_pullup(GPIO_RED_LED);
    gpio_set_direction(GPIO_RED_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_RED_LED, LED_OFF);

    gpio_pad_select_gpio(GPIO_LOCK_LED);
    gpio_pad_pulldown(GPIO_LOCK_LED);
    gpio_set_direction(GPIO_LOCK_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LOCK_LED, LED_ON);
    

    lock_status = zBIT0_OPEN;// 1 - замок открыт !
    
    // Инициализация прерывания по положительному фронту сигнала на пине замка (эмулятор состояния замка)
    gpio_config_t lock_conf = {
        .intr_type = GPIO_PIN_INTR_POSEDGE,//interrupt of rising edge
        .pin_bit_mask = GPIO_INPUT_PIN_SEL,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
    };
    gpio_config(&lock_conf);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_LOCK_PIN, pin_trap, (void *)GPIO_LOCK_PIN);

    evt_queue = xQueueCreate(8, sizeof(uint8_t));// Создаем очередь для событий при изменении состояния замка

#endif 


    //--------------------------------------------------------
    //  Блок чтения (или записи при необходимости) служебных данных :
    //  - ID устройства;
    //  - параметры для связи с sntp-сервером;
    //  - имя точки доступа, ключ подключения

    bool rt = true;

#ifdef SET_WIFI

    rt = check_pin(GPIO_WIFI_PIN);
    if (!rt) print_msg(0, TAGT, "=== CHECK_WIFI_REWRITE_PIN %d LEVEL IS %d ===\n", GPIO_WIFI_PIN, rt);

#endif

    //CLI_ID
    err = read_param(PARAM_CLIID_NAME, (void *)&cli_id, sizeof(uint32_t));
    if (err != ESP_OK) save_param(PARAM_CLIID_NAME, (void *)&cli_id, sizeof(uint32_t));
    print_msg(0, TAGT, "DEVICE_ID='%08X'\n", cli_id);

#ifdef SET_SNTP
    //SNTP + TIME_ZONE
    memset(work_sntp, 0, sntp_srv_len);
    err = read_param(PARAM_SNTP_NAME, (void *)work_sntp, sntp_srv_len);
    if (err != ESP_OK) {
        memset(work_sntp, 0, sntp_srv_len);
        memcpy(work_sntp, sntp_server_def, strlen((char *)sntp_server_def));
        save_param(PARAM_SNTP_NAME, (void *)work_sntp, sntp_srv_len);
    }
    memset(time_zone, 0, sntp_srv_len);
    err = read_param(PARAM_TZONE_NAME, (void *)time_zone, sntp_srv_len);
    if (err != ESP_OK) {
        memset(time_zone, 0, sntp_srv_len);
        memcpy(time_zone, sntp_tzone_def, strlen((char *)sntp_tzone_def));
        save_param(PARAM_TZONE_NAME, (void *)time_zone, sntp_srv_len);
    }
    print_msg(0, TAGT, "SNTP_SERVER '%s' TIME_ZONE '%s'\n", work_sntp, time_zone);
#endif

    
/* Set STA mode !!!
    wmode = WIFI_MODE_STA;
    save_param(PARAM_WMODE_NAME, (void *)&wmode, sizeof(uint8_t));
*/
    print_msg(0, TAGT, "WIFI_MODE (%d): %s\n", wmode, wmode_name[wmode]);


    //   SSID
    memset(work_ssid, 0, wifi_param_len);
    err = read_param(PARAM_SSID_NAME, (void *)work_ssid, wifi_param_len);
    if ((err != ESP_OK) || (!rt)) {
        memset(work_ssid,0,wifi_param_len);
        memcpy(work_ssid, EXAMPLE_WIFI_SSID, strlen(EXAMPLE_WIFI_SSID));
        save_param(PARAM_SSID_NAME, (void *)work_ssid, wifi_param_len);
    }
    //   KEY
    memset(work_pass, 0, wifi_param_len);
    err = read_param(PARAM_KEY_NAME, (void *)work_pass, wifi_param_len);
    if ((err != ESP_OK) || (!rt)) {
        memset(work_pass,0,wifi_param_len);
        memcpy(work_pass, EXAMPLE_WIFI_PASS, strlen(EXAMPLE_WIFI_PASS));
        save_param(PARAM_KEY_NAME, (void *)work_pass, wifi_param_len);
    }
    print_msg(0, TAGT, "WIFI_STA_PARAM: '%s:%s'\n", work_ssid, work_pass);

    wifi_param_ready = 1;


    //--------------------------------------------------------
    // Инициализация и запуск таймера с периодом в 10ms
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &periodic_timer_callback,
        .name = "periodic"
    };
    esp_timer_handle_t periodic_timer;
    esp_timer_create(&periodic_timer_args, &periodic_timer);
    esp_timer_start_periodic(periodic_timer, tperiod);//10ms = 100Hz

    vTaskDelay(10 / portTICK_RATE_MS);
    print_msg(0, TAGT, "Started timer with period in %u us\n", tperiod);


//******************************************************************************************************


#ifdef SET_WIFI

    initialize_wifi(wmode);

#endif

//******************************************************************************************************


#ifdef SET_ST7789
    // Блок инициализации шрифтов для дисплея ST7789

    print_msg(0, TAGFFS, "Initializing SPIFFS\n");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true
    };

    esp_err_t rets = esp_vfs_spiffs_register(&conf);

    if (rets != ESP_OK) {
        if (rets == ESP_FAIL) {
            ESP_LOGE(TAGFFS, "Failed to mount or format filesystem");
        } else if (rets == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAGFFS, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAGFFS, "Failed to initialize SPIFFS (%s)", esp_err_to_name(rets));
        }
    } else {
        ets_printf("[%s] Mount SPIFFS partition '%s' OK\n", TAGFFS, conf.base_path);
        size_t total = 0, used = 0;
        rets = esp_spiffs_info(NULL, &total,&used);
        if (rets != ESP_OK) {
            ESP_LOGE(TAGFFS,"Failed to get SPIFFS partition information (%s)",esp_err_to_name(rets));
        } else {
            print_msg(0, TAGFFS, "Partition size: total=%d, used=%d\n", total, used);
        }
    }

    SPIFFS_Directory("/spiffs/");

    ipsInit();

    lcdFillScreen(&dev, BLACK);
    lcdSetFontDirection(&dev, 0);
    lcdDisplayOn(&dev);

    FontxFile *lfnt = &fx16G[0];
    OpenFontx(lfnt);
    uint8_t lfnt_w = fontWidth(lfnt);//8
    uint8_t lfnt_h = fontHeight(lfnt);//16
    CloseFontx(lfnt);

    FontxFile *sfnt = &fx24G[0];
    OpenFontx(sfnt);
    uint8_t sfnt_w = fontWidth(sfnt);//12
    uint8_t sfnt_h = fontHeight(sfnt);//24
    CloseFontx(sfnt);

    FontxFile *fnt = &fx32G[0];
    OpenFontx(fnt);
    uint8_t fnt_w = fontWidth(fnt);//16
    uint8_t fnt_h = fontHeight(fnt);//32
    CloseFontx(fnt);

    uint8_t margin = 1;    
    uint32_t tsec = tsec = get_tmr(0);  
    uint16_t pos_x = margin;
    uint16_t pos_y = margin + fnt_h;
    int tu, tn;

    print_msg(0, TAG, "Use Fonts:\n\t'%s': w=%u h=%u\n\t'%s': w=%u h=%u\n\t'%s': w=%u h=%u\n",
                      lfnt->path, lfnt_w, lfnt_h, sfnt->path, sfnt_w, sfnt_h, fnt->path, fnt_w, fnt_h);

    uint16_t rx1 = 0, rx2 = CONFIG_WIDTH - 1;
    uint16_t ry1 = 0;//CONFIG_HEIGHT - 1 - lfnt_h;
    uint16_t ry2 = fnt_h;//CONFIG_HEIGHT - 1;
    lcdDrawFillRect(&dev, rx1, ry1, rx2, ry2, WHITE);

    #ifdef SET_PARK_CLI
        scr_data_t scr_data = {
            .pos_x = pos_x,
            .pos_y = pos_y + fnt_h + sfnt_h,
            .fnt = sfnt,
            .fnt_w = sfnt_w,
            .fnt_h = sfnt_h
        };
        char *msg_adr = NULL;
        scr_queue = xQueueCreate(16, sizeof(msg_adr));// Создание очереди для указателей на текстовые строки
    #endif

#endif

//******************************************************************************************************


#ifdef SET_SNTP
    // Старт задачи sntp-клиента - получение даты и времени от sntp-сервера
    if (wmode & 1) {// WIFI_MODE_STA) || WIFI_MODE_APSTA
        if (xTaskCreatePinnedToCore(&sntp_task, "sntp_task", STACK_SIZE_4K, work_sntp, 7, NULL, 1) != pdPASS) {//5,NULL,1
            ESP_LOGE(TAGS, "Create sntp_task failed | FreeMem %u", xPortGetFreeHeapSize());
        }
        vTaskDelay(250 / portTICK_RATE_MS);
    }

#endif


#ifdef SET_PARK_CLI

    void *sadr = NULL;
    #ifdef SET_ST7789
        sadr = (void *)&scr_data;
    #endif

    uint8_t byte;
    // Нитка (thread) обслуживает tcp-соединение с сервером управления
    if (xTaskCreatePinnedToCore(&net_ctl_task, "net_ctl_task", STACK_SIZE_1K * 6, sadr, 5, NULL, 0) != pdPASS) {
        ESP_LOGE(TAGS, "Create net_park_client_task failed | FreeMem %u", xPortGetFreeHeapSize());
    }
    vTaskDelay(250 / portTICK_RATE_MS);
    //
    if (evt_queue) {// Помещаем в очередь текущее состояние замка (EMPTY или BUSY)
        if (xQueueSend(evt_queue, (void *)&lock_status, (TickType_t)0) != pdPASS) {
            ESP_LOGE(TAGCLI, "Error while sending to evt_queue");
        }
    }

#endif


    tsec = get_tmr(_1s);
    restart_flag = 0;

    while (!restart_flag) {//main loop

        //

#if defined(SET_ST7789) && defined(SET_PARK_CLI)
        if (check_tmr(tsec)) {// Каждую секунду выполняем этот блок
            tsec = get_tmr(_1s);
            
            sec_to_str_time(stk);
            invert = true;
            lcdDrawString(&dev,
                        fnt,
                        pos_x,
                        pos_y,
                        (uint8_t *)mkLineCenter(stk, fnt_w, NULL),
                        BLUE);
            invert = false;

            tu = strlen(localip);
            if ((tu > 0) && (tu <= 16)) {
                if (!show_ip) {
                    tn = (16 - tu ) / 2;
                    if ((tn > 0) && (tn < 8)) sprintf(stk,"%*.s",tn," ");
                    sprintf(stk+strlen(stk),"%s", localip);
                    show_ip = true;
                } else stk[0] = '\0';
            }
            if (strlen(stk))
                lcdDrawString(&dev,
                            fnt,
                            pos_x,
                            pos_y + fnt_h,
                            (uint8_t *)mkLineCenter(stk, fnt_w, NULL),
                            CYAN);
        } 
        //   
        if (xQueueReceive(scr_queue, &msg_adr, 50)) {// Читаем очередь сообщений для вывода на дисплей
            if (msg_adr) {
                strcpy(stk, msg_adr);
                lcdDrawString(&dev,
                            scr_data.fnt,
                            scr_data.pos_x,
                            scr_data.pos_y,
                            (uint8_t *)mkLineCenter(stk, scr_data.fnt_w, NULL),
                            GREEN);
                free(msg_adr);
                msg_adr = NULL;
            }
        }
        //
        if (xQueueReceive(evt_queue, &byte, 50)) {// Читаем очередь событий для отправки сообщений серверу
            sprintf(stk, "Lock status : %s", lstatus[byte & 1]);
            print_msg(1, TAGCLI, "%s\n", stk);
            lcdDrawString(&dev,
                        scr_data.fnt,
                        scr_data.pos_x,
                        scr_data.pos_y + scr_data.fnt_h,
                        (uint8_t *)mkLineCenter(stk, scr_data.fnt_w, NULL),
                        YELLOW);
            // send event to server 
            if (online && evt_auto) {
                if (sndEvt()) print_msg(1, TAGCLI, "Send event to server error.");
            }    
        }   
#endif

        if (wmode == WIFI_MODE_STA) {// WIFI_MODE_STA || WIFI_MODE_APSTA

#ifdef SET_SNTP
            // Блок перезапуска нитки sntp-клиента если ранее не удалось получить дату и время от sntp-сервера
            if (sntp_tmr) {    
                if (check_tmr(sntp_tmr)) {
                    sntp_tmr = 0;
                    sntp_go = 1;    
                }
            }     
            if (sntp_go) {
                if (!sntp_start) {
                    sntp_go = 0;
                    if (xTaskCreatePinnedToCore(&sntp_task, "sntp_task", STACK_SIZE_2K, work_sntp, 5, NULL, 1)  != pdPASS) {//5//7 core=1
                        ESP_LOGE(TAGS, "Create sntp_task failed | FreeMem %u", xPortGetFreeHeapSize());
                    }
                    vTaskDelay(500 / portTICK_RATE_MS);
                } else vTaskDelay(50 / portTICK_RATE_MS);
            }
#endif

        }

    }//while (!restart_flag)

    vTaskDelay(500 / portTICK_RATE_MS);


    strcpy(stk, "Restart...");
    //print string on display

    uint8_t cnt = 30;
    print_msg(1, TAG, "Waiting for all task closed...%d sec.\n", cnt/10);

    while (total_task) {
        cnt--; if (!cnt) break;
        vTaskDelay(100 / portTICK_RATE_MS);
    }
    print_msg(1, TAG, "DONE (%d). Total unclosed task %d\n", cnt, total_task);

    if (macs) free(macs);

    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());

    vTaskDelay(500 / portTICK_RATE_MS);

    esp_restart();
}

