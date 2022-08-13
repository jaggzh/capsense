#if !defined(ARDUINO) && !defined(ESP_PLATFORM)
	#include <sys/time.h>
	#include <unistd.h>
	
	void init_millis(void);
	unsigned long int millis(void);
#endif

