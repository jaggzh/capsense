#define ARDUENV (defined(ARDUINO) || defined(ESP_PLATFORM))

#if ARDUENV
	#define spa(v) Serial.print(v)
	#define spal(v) Serial.println(v)
	#define spa2(v) Serial.print(v)
	#define spal2(v) Serial.println(v)
	#define spa2begin(baud, bitstuff, rx, tx) Serial2.begin(baud, bitstuff, rx, tx)
	#define spabegin(v) Serial.begin(v)
#else
	#define spa(v)
	#define spal(v)
	#define spa2(v)
	#define spal2(v)
	#define spa2begin(baud, bitstuff, rx, tx)
	#define spabegin(v)
#endif
