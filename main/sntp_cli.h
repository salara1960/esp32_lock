#ifndef __SNTP_CLI_H__
#define __SNTP_CLI_H__

#include "hdr.h"

#ifdef SET_SNTP
    #include <time.h>
    #include <sys/time.h>
    #include "esp_sntp.h"

    const char *TAGS;
    uint8_t sntp_start;
    void sntp_task(void *arg);
#endif

#endif
