
#define sim800 Serial1
#include <WiFi.h>
static const char* ssid = WIFI_SSID;
static const char* password = WIFI_PASSWORD;
static const char* ssid2 = WIFI_SSID2;
static const char* password2 = WIFI_PASSWORD2;
static const bool loggmode = true; //enables logging

void logger_info(String message){
  if (loggmode == true )
      { 
        Serial.println("[info]: " + message);
      }
}


static void lightsleep(int minutes){
  int seconds = minutes * 60;
  //#define TIME_TO_SLEEP seconds
  #define uS_TO_S_FACTOR 1000000ULL 
  logger_info("Entering Light Sleep Mode for "+ String(minutes) + " minute(s).");
                delay(2000);
                esp_sleep_enable_timer_wakeup(seconds * uS_TO_S_FACTOR);
                delay(100);
                esp_light_sleep_start();
                
  logger_info("Woke Up from Light Sleep Mode");
}




static String GetATCmdResp(String ATCmd, int SendMode, int timeout){
  int stat = -1;
  String strModem;
  sim800.flush();
  delay(500);
sim800.println(ATCmd);
delay(1000);
  unsigned int n = millis();
  while (sim800.available() && stat == -1)
  { 
    String strBuff = sim800.readString();
    strModem = strModem + strBuff;
   if (strModem.indexOf("\nOK\r") > -1)
   {
     stat = 0;
   } else if (strModem.indexOf("\nERROR\r") > -1)
   {
     stat = 1;
   } else if (millis() - n < (timeout * 1000)){
    stat = 2;
   }
  }
  if (stat == 0) 
  { strModem.trim();
    return "0," + strModem;
  }
  else if (stat == 1)
        {
        return "1,Error";
        }
  else if (stat == 2)
        {
        return "1,Timeout";
        }
  
  
}

String getSMSProperty(int PID ,String buff){
   unsigned int index;
   String smsStatus="";
   String senderNumber="";
   String receivedDate="";
   String msg="";
   
    index = buff.indexOf(",");
    smsStatus = buff.substring(1, index-1); 
    buff.remove(0, index+2);
    
    senderNumber = buff.substring(0, 13);
    buff.remove(0,19);
   
    receivedDate = buff.substring(0, 20);
    buff.remove(0,buff.indexOf("\r"));
    buff.trim();
    
    index =buff.indexOf("\n\r");
    buff = buff.substring(0, index);
    buff.trim();
    msg = buff;
    buff = "";
    msg.toLowerCase();
    if (PID == 1) {return senderNumber;}
    else if (PID == 2) {return smsStatus;}
    else if (PID == 3) {return receivedDate;}
    else if (PID == 4) {return msg;}
}




static bool connectToWiFi()
{
  logger_info("Connecting to WIFI SSID " + String(ssid));

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int x = 0;
  while (WiFi.status() != WL_CONNECTED && x < 100)
  {
    delay(100);
    Serial.print(".");
    x++;
    if (x >= 100)
    {
      WiFi.disconnect();
      Serial.println("");
      logger_info("Connecting to WIFI SSID " + String(ssid2));
      WiFi.begin(ssid2, password2);
      while (WiFi.status() != WL_CONNECTED && x <= 200)
      {
        delay(100);
        Serial.print(".");
       x++;
      }
    }
  }

  Serial.println("");

  logger_info("WiFi connected, IP address: " + WiFi.localIP().toString());
  if (WiFi.status() != WL_CONNECTED){
    return false;
  }
  else {
    return true; 
  }

  
}

static String getWiFiSSid()
{
  String SSIDs ="";  
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  if (n == 0) {
    SSIDs ="0";
  } else {
    SSIDs = String(n);
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      SSIDs = SSIDs + "," + String(WiFi.SSID(i));
           SSIDs = SSIDs + ":" + String(WiFi.RSSI(i));
      delay(10);
    }
  }
  return SSIDs;
}

static bool detectHomeSSID(String buff)
{
  if (getWiFiSSid().indexOf(buff) > -1){
    return true;
  } else {
    return false;
    }
}
