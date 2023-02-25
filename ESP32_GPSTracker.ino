


// Please select the corresponding model

// #define SIM800L_IP5306_VERSION_20190610
//#define SIM800L_AXP192_VERSION_20200327
// #define SIM800C_AXP192_VERSION_20200609
#define SIM800L_IP5306_VERSION_20200811

// Define the serial console for debug prints, if needed
#define DUMP_AT_COMMANDS
#define TINY_GSM_DEBUG          SerialMon

#include "utilities.h"
#include "macros.h"
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <time.h>
#include <ESP32Time.h>

//Powermanagement control
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 600        /* Time ESP32 will go to sleep (in seconds) */

//esp_light_sleep_start()




//Define GPS Configuration
static const int RXPin = 12, TXPin = 14;
static const uint32_t GPSBaud = 9600;
SoftwareSerial ss(RXPin, TXPin);

String mapsURL ="https://www.google.com/maps/search/?api=1&query=";
TinyGPSPlus gps;
static const String homessid = WIFI_SSID;


#include <WiFi.h>

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to the module)
#define sim800  Serial1

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800          // Modem is SIM800
#define TINY_GSM_RX_BUFFER      1024   // Set RX buffer to 1Kb

#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(sim800, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(sim800);
#endif


//sender phone number with country code
const String PHONE = "+44XXXXXXXXXX";

String smsStatus,senderNumber,receivedDate,msg;
boolean isReply = false;


/*RTC set to UTC*/
int rtctimestate = 0;

ESP32Time rtc(0);

static bool SetTime(int ss, int minutes, int hh, int DD, int MM, int YYYY)
            {
                  rtc.setTime(ss, minutes, hh, DD, MM, YYYY);
                  return true;
            }

void(* resetFunc) (void) = 0; //declare reset function @ address 0

void setup()
{
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  //set LED Pin
  pinMode(25, OUTPUT); //mosfet control
  pinMode(13, OUTPUT);
    // Set console baud rate
    SerialMon.begin(115200);
    Serial.println("Start of Setup..");
    delay(10);
    Serial.println("Initialize GPS Serial..");
    ss.begin(GPSBaud);
    
    Serial.println("Start power management");
    if (setupPMU() == false) {
        Serial.println("Setting power error");
    }
    Serial.println("Scanning for networks and I found...:");
 Serial.println(getWiFiSSid());
    // Some start operations
    
    setupModem();

    // Set GSM module baud rate and UART pins
    sim800.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

    delay(6000);
        // Restart takes quite some time
    // To skip it, call init() instead of restart()
    SerialMon.println("Initializing modem...");
    modem.restart();


    delay(10000);

    String imei = modem.getIMEI();
    DBG("IMEI:", imei);

    smsStatus = "";
    senderNumber="";
    receivedDate="";
    msg="";

delay(5000);
  //Set SMS Mode
  Serial.println("SEND: AT+CMGF=1");
  Serial.println(GetATCmdResp("AT+CMGF=1",0,5));
  //delete all sms
  //Serial.println("SEND: AT+CMGD=1,4");
  //Serial.println(GetATCmdResp("AT+CMGD=1,4",0,5));
  //Delete all read SMS
  Serial.println("SEND: AT+CMGDA=\"DEL READ\"");
  Serial.println(GetATCmdResp("AT+CMGDA=\"DEL READ\"",0,5));
  

 Serial.println("Setup Done, Starting main program.");   
//Shut down unwanted peripherals
btStop();
//esp_bluedroid_deinit(void);
 
}
//globlstates and timers
int masterState = 0;
int masterStateTime = 0;
int gpsState = 0;
int gpsTimer = 30000; //default GPS location test interval in ms
int smsTimer = 15000; //default SMS test interval in ms
int smsHomeTimer = 300000; //default SMS test interval in ms when home
int wifiTimer = 300000; //default WiFi sniffer interval in ms
int Wsn = 0;
int Gsn = 0;
int Ssn = 0;
int HSsn = 0;

void loop() {
//Power on the GPS via pin 25 controlling the p-channel mosfet circuit in mode 2 (away)
if (masterState==2)
    {
      digitalWrite(25,HIGH);
    }
if (masterState < 2)
    {
      digitalWrite(25,LOW);
    }
//Wifi sniffer control
if (masterState==0 || millis() >= Wsn)
    { 
      logger_info("WiFi Control:");
      logger_info(getWiFiSSid());
      MasterStateLogic();
      Wsn = millis() + wifiTimer;
    } 

//ModemOff();


//SMS State control
if (masterState==1 && millis() >= HSsn)
    {   
        logger_info("Startup Modem");
       // setupModem();
       // delay(5000);
        logger_info("SMS Control");
        ATListener();
        logger_info("Shutdown Modem");
      //  ModemOff();
        delay(50);
        HSsn = millis() + smsHomeTimer;
    }


if (masterState==2 && millis() >= Ssn)
    {
        logger_info("SMS Control");
        ATListener();
        Ssn = millis() + smsTimer;
    }
  
//smscheckstate(); depricated?

  while (ss.available() > 0)
    if (gps.encode(ss.read()))
     logger_info("ReadGPS");
}

void ATListener(){
int i =0;
while (i <= 20)
  {
    ATListen();
    delay(100);
    i=i+1;  
  }
}

void ATListen(){
 while(sim800.available()){
  parseData(sim800.readString());
}
while(Serial.available())  {
  sim800.println(Serial.readString());
} 
}



//***************************************************
void parseData(String buff){
 // Serial.println(buff);

  unsigned int len, index;
  //////////////////////////////////////////////////
  //Remove sent "AT Command" from the response string.
  index = buff.indexOf("\r");
  buff.remove(0, index+2);
  buff.trim();
  //////////////////////////////////////////////////
  
  //////////////////////////////////////////////////
  if(buff != "OK"){
    index = buff.indexOf(":");
    String cmd = buff.substring(0, index);
    cmd.trim();
    
    buff.remove(0, index+2);
    
    if(cmd == "+CMTI"){
      //get newly arrived memory location and store it in temp
      index = buff.indexOf(",");
      String temp = buff.substring(index+1, buff.length()); 
      temp = "AT+CMGR=" + temp + "\r"; 
      //get the message stored at memory location "temp"
      sim800.println(temp); 
    }
    else if(cmd == "+CMGR"){
     
      senderNumber=getSMSProperty(1, buff);
      if(PHONE.indexOf(senderNumber) > -1){
       //Prosess SMS Message from valid sender
       ledblink(1);
       Serial.println("Valid Sender recognized!");
       Serial.println("With timestamp: "+getSMSProperty(3, buff));
       Serial.println("Text Message Reads:\r "+getSMSProperty(4, buff));
       if(getSMSProperty(4, buff).indexOf("gettime") > -1)
            {
            String msg = "Message recived at: "+getSMSProperty(3, buff);
            Reply(msg);
            }
        
        if(getSMSProperty(4, buff).indexOf("getmode") > -1)
            {
            String msg;
            int instate = millis() - masterStateTime;
            String instateStr = " " + String(instate/60000) + " Minutes in state.";
            if (masterState == 2) 
                  { 
                    msg = "Mode: 2 Out and About." + instateStr;
                  } 
             else 
                  {
                    msg = "Mode: 1 Known Location." + instateStr;
                  } 
            Reply(msg);
            }
        
        if(getSMSProperty(4, buff).indexOf("getssids") > -1)
        {
          String msg = "# seen,SSID name,rssi:\r" + getWiFiSSid();
          Reply(msg);
        }
        
        if(getSMSProperty(4, buff).indexOf("getgps") > -1)
        {
          String msg = getlocation();
          Reply(msg);
        }

        if(getSMSProperty(4, buff).indexOf("reboot") > -1)
        {
          String msg = "Device Rebooting";
          Reply(msg);
          delay(10000);
         resetFunc();
        }
        
        if(getSMSProperty(4, buff).indexOf("sleep") > -1)
        {
          String msg = "Powering down Modem and enetering light sleep mode for 2 minutes";
          Reply(msg);
          ledblink(10);
         // ModemOff();
          esp_light_sleep_start();
          ledblink(3);
          //setupModem();
          resetFunc();
        }

        //ModemOff()
       
        //delete all sms
        GetATCmdResp("AT+CMGD=1,4",0,5);
        GetATCmdResp("AT+CMGDA=\"DEL ALL\"",0,5);
        }
    }
  //////////////////////////////////////////////////
  }
  else{
  //The result of AT Command is "OK"
  }
}

//************************************************************


void doAction(){
  if(msg == "relay1 off"){  
  //  digitalWrite(RELAY_1, HIGH);
    Reply("Relay 1 has been OFF");
  }
  else if(msg == "relay1 on"){
  //  digitalWrite(RELAY_1, LOW);
    Reply("Relay 1 has been ON");
  }
  else if(msg == "relay2 off"){
  //  digitalWrite(RELAY_2, HIGH);
    Reply("Relay 2 has been OFF");
  }
  else if(msg == "relay2 on"){
  //  digitalWrite(RELAY_2, LOW);
    Reply("Relay 2 has been ON");
  }

  
  smsStatus = "";
  senderNumber="";
  receivedDate="";
  msg="";  
}

void Reply(String text)
{
    sim800.print("AT+CMGF=1\r");
    delay(1000);
    sim800.print("AT+CMGS=\""+PHONE+"\"\r");
    delay(1000);
    sim800.print(text);
    delay(100);
    sim800.write(0x1A); //ascii code for ctrl-26 //sim800.println((char)26); //ascii code for ctrl-26
    delay(1000);
   // Serial.println("SMS Sent Successfully.");
}


static void ledblink(int sec)
{
  int n =0;
  sec=sec + sec;
  while (n < sec)
  {
            digitalWrite(LED_GPIO, LED_ON);
            delay(250);
            digitalWrite(LED_GPIO, LED_OFF);  
            delay(250);
            n=n+1;
  }
}

void turnOffNetlight()
{
    logger_info("Turning off SIM800 Red LED...");
    modem.sendAT("+CNETLIGHT=0");
}

void turnOnNetlight()
{
    logger_info("Turning on SIM800 Red LED...");
    modem.sendAT("+CNETLIGHT=1");
}


static void CheckTime(){
  if (rtctimestate == 0 && gps.time.isValid() && gps.date.isValid())    
          {
                     
          SetTime(gps.time.second(), gps.time.minute(), gps.time.hour(), gps.date.day(), gps.date.month(), gps.date.year());
          logger_info("RTC Set");
          rtctimestate = 1;
          }
}

static String getlocation(){
    String linktosend;  
    if (gps.location.isValid())
    {
    Serial.print(F("Location: ")); 
    String GPSLat = String(gps.location.lat(), 6);
    String GPSLon = String(gps.location.lng(), 6);
    Serial.print(GPSLat);
    Serial.print(F(","));
    Serial.print(GPSLon);
    
    linktosend = mapsURL + GPSLat + "," + GPSLon;
    }
    else
    {
      linktosend ="No GPS Lock available try again later";
    }
  return linktosend;
}




void displayInfo()
{
  Serial.print(F("Location: ")); 
  if (gps.location.isValid())
  {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Date/Time: "));
  if (gps.date.isValid())
  {
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F(" "));
  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
    Serial.print(F("."));
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gps.time.centisecond());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.println();
}

static void MasterStateLogic()
{
  if (masterState == 0) //check for apropriate config
    {
      turnOffNetlight(); //Switch off Carrier indicator transistor
      ledblink(5); //flash blue LED for 5 seconds inicating Main startup
      if (detectHomeSSID(homessid))
          {
          logger_info("Home Wifi was Detected, entering state 1");
          masterState = 1;
          masterStateTime = millis();
          } else 
          {
            logger_info("Home Wifi was not Detected, Entering satate 2 (not home)");
            masterState = 2;
            masterStateTime = millis();
          }
    }
    
if (masterState == 1)
    {
     if (detectHomeSSID(homessid))
     {
      logger_info("Home Wifi was Detected");
      } else 
          {
            logger_info("Wifi was lost, Entering satate 2 (not home)");
            masterState = 2;
            masterStateTime = millis();
          }
     delay(50);
     }
if (masterState == 2)
    {
     if (detectHomeSSID(homessid))
      {
        logger_info("Home Wifi was Detected, entering state 1");
        masterState = 1;
        masterStateTime = millis();
        } else 
          {
           logger_info("Home Wifi was not Detected");
          }
    
     delay(50);
    }    
    

}
