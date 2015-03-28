#ifndef CREA_ESP8266_h
#define CREA_ESP8266_h

#if ARDUINO < 100
#include <WProgram.h>
#include <pins_arduino.h>  // fix for broken pre 1.0 version - TODO TEST
#else
#include <Arduino.h>
#endif

#define DEST_HOST   "api.arduinogt.com"
#define DEST_IP     "97.74.215.186"
#define TIMEOUT     5000 // mS
#define CONTINUE    false
#define HALT        true
#define INPUT_SIZE  30

class CREA_ESP8266
{
	typedef void (*GeneralMessageFunction) (String value);
public:
	void CREA_setup(String _SSID, String _PASS, String _MODULEID, String _AUTH);
	boolean execute(String order);
	void CREA_loop(GeneralMessageFunction callback);
	void set_response(String message);
	String value;
	String command;
	int ref;

private:
	void errorHalt(String msg);
	boolean echoFind(String keyword);
	void echoFlush();
	void echoSkip();
	boolean echoCommand(String cmd, String ack, boolean halt_on_fail);
	boolean connectWiFi();

	String response;
	String SSID;
	String PASS;
	String MODULEID;
	String AUTH;
	String CALL_RESP;
};

#endif
