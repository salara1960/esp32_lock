#include "hdr.h"

#ifdef SET_SNTP

#include "main.h"
//----------------------------------------------------------------------------------------
const char *TAGS = "NTP";
uint8_t sntp_start = 0;
//----------------------------------------------------------------------------------------
// Нитка (thread) sntp-клиента - получает от sntp-сервера текущие дату и время
//
void sntp_task(void *arg)
{
sntp_start = 1;
total_task++;


    //ets_printf("[%s] Start sntp_client task | FreeMem %u\n", TAGS, xPortGetFreeHeapSize());
    print_msg(1, TAGS, "Start sntp_client task | FreeMem %u\n", xPortGetFreeHeapSize());

    char *sntp_server = (char *)arg;

    if (sntp_server) {

        time_t now = 0;
        struct tm timeinfo = {0};
        uint8_t retry = 0, retry_max = 10;

        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

        print_msg(1, TAGS, "Getting time from SNTP server '%s'.\n", sntp_server);

        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, sntp_server);
        sntp_init();

        do {
            retry++;
            time(&now);
            localtime_r(&now, &timeinfo);
            if (restart_flag) break; else vTaskDelay(500 / portTICK_PERIOD_MS);
        } while ( (timeinfo.tm_year < (2021 - 1900)) && (retry < retry_max) );

        if (retry < retry_max) {
            time(&now);// update 'now' variable with current time
            setenv("TZ", time_zone, 1);
            tzset();
            localtime_r(&now, &timeinfo);
            char strftime_buf[64] = {0};
            strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
            //ets_printf("[%s] The current date/time is: %s %s\n", TAGS, strftime_buf, time_zone);
            print_msg(1, TAGS, "The current date/time is: %s %s\n", strftime_buf, time_zone);
            setDateTimeOK = 1;
            sntp_tmr = 0;
        } else {
            //ets_printf("[%s] Error getting date/time from srv '%s %s'\n", TAGS, sntp_server, time_zone);
            print_msg(1, TAGS, "Error getting date/time from srv '%s %s'\n", sntp_server, time_zone);
            sntp_tmr = get_tmr(_10s);
        }

        sntp_stop();

    }

    if (total_task) total_task--;
    sntp_start = 0;

    print_msg(1, TAGS, "Stop sntp_client task | FreeMem %u\n", xPortGetFreeHeapSize());

    vTaskDelete(NULL);
}

#endif
