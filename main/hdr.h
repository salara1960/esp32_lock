#define SET_ST7789
#ifdef SET_ST7789
	//#define SET_ST7789_TEST
#endif

#define SET_WIFI

#ifndef SET_WIFI
	#undef SET_SNTP
#else
	#define SET_SNTP
#endif

#define SET_PARK_CLI
