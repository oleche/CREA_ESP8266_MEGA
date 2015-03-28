// Simple Base64 code
// (c) Copyright 2010 MCQN Ltd.
// Released under Apache License, version 2.0

#include "CREA_ESP8266.h"

void CREA_ESP8266::errorHalt(String msg)
{
  Serial.println(msg);
  Serial.println("HALT");
  while(true){};
}

// Read characters from WiFi module and echo to serial until keyword occurs or timeout.
boolean CREA_ESP8266::echoFind(String keyword)
{
  byte current_char   = 0;
  byte keyword_length = keyword.length();
  
  // Fail if the target string has not been sent by deadline.
  long deadline = millis() + TIMEOUT;
  while(millis() < deadline)
  {
    if (Serial1.available())
    {
      char ch = Serial1.read();
      Serial.write(ch);
      if (ch == keyword[current_char])
        if (++current_char == keyword_length)
        {
          Serial.println();
          return true;
        }
    }
  }
  return false;  // Timed out
}

// Read and echo all available module output.
// (Used when we're indifferent to "OK" vs. "no change" responses or to get around firmware bugs.)
void CREA_ESP8266::echoFlush()
  {while(Serial1.available()) Serial.write(Serial1.read());}
  
// Echo module output until 3 newlines encountered.
// (Used when we're indifferent to "OK" vs. "no change" responses.)
void CREA_ESP8266::echoSkip()
{
  echoFind("\n");        // Search for nl at end of command echo
  echoFind("\n");        // Search for 2nd nl at end of response.
  echoFind("\n");        // Search for 3rd nl at end of blank line.
}

// Send a command to the module and wait for acknowledgement string
// (or flush module output if no ack specified).
// Echoes all data received to the serial monitor.
boolean CREA_ESP8266::echoCommand(String cmd, String ack, boolean halt_on_fail)
{
  Serial1.println(cmd);
  #ifdef ECHO_COMMANDS
    Serial.print("--"); Serial.println(cmd);
  #endif
  
  // If no ack response specified, skip all available module output.
  if (ack == "")
    echoSkip();
  else
    // Otherwise wait for ack.
    if (!echoFind(ack))          // timed out waiting for ack string 
      if (halt_on_fail)
        errorHalt(cmd+" failed");// Critical failure halt.
      else
        return false;            // Let the caller handle it.
  return true;                   // ack blank or ack found
}

// Connect to the specified wireless network.
boolean CREA_ESP8266::connectWiFi()
{
  String cmd = "AT+CWJAP=\""; cmd += SSID; cmd += "\",\""; cmd += PASS; cmd += "\"";
  if (echoCommand(cmd, "OK", CONTINUE)) // Join Access Point
  {
    Serial.println("Connected to WiFi.");
    return true;
  }
  else
  {
    Serial.println("Connection to WiFi failed.");
    return false;
  }
}


void CREA_ESP8266::CREA_setup(String _SSID, String _PASS, String _MODULEID, String _AUTH){
  SSID = _SSID;
  PASS = _PASS;
  MODULEID = _MODULEID;
  AUTH = _AUTH;

  Serial.begin(115200);         // Communication with PC monitor via USB
  Serial1.begin(9600);        // Communication with ESP8266 via 5V/3.3V level shifter
  
  Serial1.setTimeout(TIMEOUT);
  Serial.println("CREA ESP8266");
  
  delay(2000);

  echoCommand("AT+RST", "ready", HALT);    // Reset & test if the module is ready  
  Serial.println("Module is ready.");
  echoCommand("AT+GMR", "OK", CONTINUE);   // Retrieves the firmware ID (version number) of the module. 
  echoCommand("AT+CWMODE?","OK", CONTINUE);// Get module access mode. 
  
  // echoCommand("AT+CWLAP", "OK", CONTINUE); // List available access points - DOESN't WORK FOR ME
  
  echoCommand("AT+CWMODE=1", "", HALT);    // Station mode
  echoCommand("AT+CIPMUX=1", "", HALT);    // Allow multiple connections (we'll only use the first).

  //connect to the wifi
  boolean connection_established = false;
  for(int i=0;i<5;i++)
  {
    if(connectWiFi())
    {
      connection_established = true;
      break;
    }
  }
  if (!connection_established) errorHalt("Connection failed");
  
  delay(5000);

  //echoCommand("AT+CWSAP=?", "OK", CONTINUE); // Test connection
  echoCommand("AT+CIFSR", "", HALT);         // Echo IP address. (Firmware bug - should return "OK".)
  //echoCommand("AT+CIPMUX=0", "", HALT);      // Set single connection mode
}

boolean CREA_ESP8266::execute(String order){
  if (order != "NA"){
    command = order.substring(0,2);
    ref = order.substring(3,order.indexOf("|")).toInt();
    value = order.substring(order.indexOf("|")+1,order.length());
    if (command == "DO"){
        int v = value.toInt();
        if (value <= 0){
            digitalWrite(ref, LOW);
        }
        digitalWrite(ref, v);
        return true;
    }else if( command == "AO" ){
        analogWrite(ref, value.toInt());
        return true;
    }else if( command == "SR"){
        return true;
    }
    return false;
  }else{
    return true;
  }
}

void CREA_ESP8266::set_response(String message){
    CALL_RESP = message;
}

void CREA_ESP8266::CREA_loop(GeneralMessageFunction callback){
    // Establish TCP connection
  String cmd = "AT+CIPSTART=0,\"TCP\",\""; cmd += DEST_IP; cmd += "\",80";
  if (!echoCommand(cmd, "OK", CONTINUE)) { echoCommand("AT+CIPCLOSE=0", "", CONTINUE); return;}
  delay(2000);
  
  // Get connection status 
  if (!echoCommand("AT+CIPSTATUS", "OK", CONTINUE)) return;

  // Build HTTP request.
  cmd = "GET /v1/aw/"; cmd += MODULEID; cmd += (CALL_RESP != "")?"?q="+CALL_RESP:""; cmd += " HTTP/1.1\r\nHost: "; cmd += DEST_HOST; cmd += ":80\r\nAuthorization: Basic "; cmd += AUTH; cmd+= "\r\n\r\n";
  
  // Ready the module to receive raw data
  if (!echoCommand("AT+CIPSEND=0,"+String(cmd.length()), ">", CONTINUE))
  {
    echoCommand("AT+CIPCLOSE", "", CONTINUE);
    Serial.println("Connection timeout.");
    return;
  }
  
  // Send the raw HTTP request
  echoCommand(cmd, "OK", CONTINUE);  // GET
  
  // Loop forever echoing data received from destination server.
  boolean completed = false;
  boolean executed = false; 
  CALL_RESP = "";
  while(!completed)
    while (Serial1.available()){
      char c = Serial1.read();
      //Serial.write(c);
      response += c;
      int st = response.indexOf('<');
      int ed = response.indexOf('>');
      if (st > 0 and ed > 0){
        response = response.substring(st+1,ed);
        executed = true;
        callback(response);
      }
      if (response.indexOf("OK") > 0 && executed){
        echoCommand("AT+CIPCLOSE=0", "", CONTINUE);
        completed = true;
      }
      if (c == '\r') { response = ""; Serial.print('\n'); };
    }
      
  
  delay(5000);
}