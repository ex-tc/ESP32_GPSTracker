/*
*This code is for a GPS Tracker with SMS controll. 
*It is based on the TTGO T-CALL v1.4 ESP 32 prototype board with onboard IP5306 power management and SIM800L GSM module, there are multiple utility configurations in the utilities.h file including a generic ESP32 config.
*all configurations including PIN OUT config is set in config.h
*It also uses an external power switch to be able to switch off external peripherals(Read the documentation on GitHub for more details).
*The GPS module used is a NEO6M module connected via soft serial.
*For full details please go to: https://github.com/ex-tc/ESP32_GPSTracker
*/



//Global Definitions START
//Board Config
//#define SIM800L_IP5306_VERSION_20190610
//#define SIM800L_AXP192_VERSION_20200327
//#define SIM800C_AXP192_VERSION_20200609
//#define GENERIC_ESP32
#define SIM800L_IP5306_VERSION_20200811
// Define the serial console for debug prints, if needed
#define DUMP_AT_COMMANDS
#define TINY_GSM_DEBUG          SerialMon
// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800          // Modem is SIM800
#define TINY_GSM_RX_BUFFER      1024   // Set RX buffer to 1Kb
//Sleep Management Control
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 900        /* Time ESP32 will go to sleep (in seconds) */
// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to the module)
#define sim800  Serial1

//Global Definitions END

//Includes
#include "utilities.h"
#include "macros.h"
#include "config.h"
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <time.h>
#include <ESP32Time.h>
#include <WiFi.h>
#include <TinyGsmClient.h>




//Global Functianal Declarations
String senderNumber,msg;
boolean isReply = false;
String mapsURL ="https://www.google.com/maps/search/?api=1&query=";
//Global Gonstants

//Define GPS Configuration
static const int RXPin = GPS_RX_PIN, TXPin = GPS_TX_PIN;
static const uint32_t GPSBaud = 9600;
static const String homessid = WIFI_SSID;
static const String althomessid = WIFI_SSID2;

//Safe Sender phone number with country code
const String PHONE = SAFE_SENDER_MOBILE;
const String PHONE_LIST = SAFE_SENDER_LIST;


//Other Serial stuff for GPS and GSM 
SoftwareSerial ss(RXPin, TXPin);
TinyGPSPlus gps;
TinyGsm modem(sim800);



//Globl States and Timers
int Globalreset = GLBL_RST_INTERVAL; //reboot the device every 8h incase it enters a failed state.
int masterState = 0;
int masterStateTime = 0;
int gpsState = 0;
int gpsTimer = GLBL_GPS_INTERVAL; //default GPS location test interval in ms
int smsTimer = GLBL_SMS_INTERVAL; //default SMS test interval in ms
int smsHomeTimer = GLBL_SMS_HOME_INTERVAL; //default SMS test interval delay in ms when home
int wifiTimer = GLBL_WIFI_INTERVAL; //default WiFi sniffer interval in ms
int Wsn = 0;
int Gsn = 0;
int Ssn = 0;
int HSsn = smsHomeTimer; //Prevents the sleep cycle for home timer cycle (2 minute) this ensures that SMS control is active for 2 minutes on boot up in home state.


/*RTC set to UTC*/
int rtctimestate = 0;
ESP32Time rtc(0);
static bool SetTime(int ss, int minutes, int hh, int DD, int MM, int YYYY)
            {
                  rtc.setTime(ss, minutes, hh, DD, MM, YYYY);
                  return true;
            }

//Device Reset Function
void(* resetFunc) (void) = 0; //declare reset function @ address 0

//Setup Scripts
void setup()
{
  SerialMon.begin(115200);
  logger_info("Start Setup");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  logger_info("set GPIO Pins");
  pinMode(GPS_POWER_ON_PIN, OUTPUT); //GPS Mosfet Powerswitch Control
  pinMode(LED_GPIO_PIN, OUTPUT); //Blue LED Pin
  logger_info("Initialize GPS Serial..");
  ss.begin(GPSBaud);
  logger_info("Start power management");
    if (setupPMU() == false) {
        logger_info("Setting power error");
    }
  logger_info("Scanning for networks and I found...:");
  logger_info(getWiFiSSid());
  logger_info("Initialize Modem");    
  setupModem();
  logger_info("Set GSM module baud rate and UART pins");
    sim800.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(6000);
    logger_info("Initializing modem...");
//    modem.restart();
    delay(10000);
    senderNumber="";
    msg="";

delay(5000);
  //Set SMS Mode
  logger_info("SEND: AT+CMGF=1");
  Serial.println(GetATCmdResp("AT+CMGF=1",0,5));

 logger_info("Setup Done, Starting main program.");   

logger_info("Shut down BLE");
btStop();

logger_info("Start Done"); 
}

//Main Loop
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



//display all states and timers
logger_info("mills:"+String(millis())+" HSsn:"+String(HSsn)+" Ssn:"+String(Ssn)+" Wsn:"+String(Wsn)+" masterState:"+String(masterState));

//Home State control 
if (masterState==1 && millis() >= HSsn)
    {   
        logger_info("SMS Control");
        ATListener();
        if (STE == true){
        delay(1000);
        logger_info("Shutdown Modem");
        ModemOff();
        logger_info("Enter Sleep After "+String(millis()/60000)+" Minutes of Uptime");
        logger_info("Will wakeup and Reboot device in "+String(TIME_TO_SLEEP/60)+" Minutes.");
        ledblink(3);
        esp_light_sleep_start();
        ledblink(3);
        resetFunc();
        }
        delay(1000); //Safety to resrart command
        HSsn = millis() + smsHomeTimer;
    }


if (masterState > 0 && millis() >= Ssn)
    {
        logger_info("SMS Control");
        ATListener();
        Ssn = millis() + smsTimer;
        if (masterState == 2) HSsn = millis() + smsHomeTimer;//ensuring that we dont sleep when switching to mode 1
    }
    
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
      if(PHONE_LIST.indexOf(senderNumber) > -1){
       //Prosess SMS Message from valid sender
       ledblink(1);
       Serial.println("Valid Sender recognized!");
       Serial.println("With timestamp: "+getSMSProperty(3, buff));
       Serial.println("Text Message Reads:\r "+getSMSProperty(4, buff));
       if(getSMSProperty(4, buff).indexOf("gettime") > -1)
            {
            String msg = "Message recived at: "+getSMSProperty(3, buff);
            Reply(msg,senderNumber);
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
            Reply(msg,senderNumber);
            }
        
        if(getSMSProperty(4, buff).indexOf("getssids") > -1)
        {
          String msg = "# seen,SSID name,rssi:\r" + getWiFiSSid();
          Reply(msg,senderNumber);
        }
        
        if(getSMSProperty(4, buff).indexOf("getgps") > -1)
        {
          String msg = "GPS Timestamp: "+ getlatestGpsDateTime() +" Geo Location: " + getlocation();
          Reply(msg,senderNumber);
        }

        if(getSMSProperty(4, buff).indexOf("reboot") > -1)
        {
          String msg = "Device Rebooting";
          Reply(msg,senderNumber);
          delay(10000);
         resetFunc();
        }
        
        if(getSMSProperty(4, buff).indexOf("sleep") > -1)
        {
          String msg = "Powering down Modem and enetering light sleep mode for 15 minutes";
          Reply(msg,senderNumber);
          ledblink(10);
          ModemOff();
          delay(200);
          esp_light_sleep_start();
          ledblink(3);
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




void Reply(String text,String Sender)
{
    sim800.print("AT+CMGF=1\r");
    delay(1000);
    sim800.print("AT+CMGS=\""+Sender+"\"\r");
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
            digitalWrite(LED_GPIO_PIN, LED_ON);
            delay(250);
            digitalWrite(LED_GPIO_PIN, LED_OFF);  
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


static String getlatestGpsDateTime(){
  String gpsdatetime ="";

   if (gps.date.isValid())
  { 
    gpsdatetime = gpsdatetime + gps.date.year() +"-" + gps.date.month()+"-"+ gps.date.day()+" ";
  }
  else
  {
    gpsdatetime = gpsdatetime + "INVALID DATE"+" ";
  }

  if (gps.time.isValid())
  {
    String gps_h;  
    if (gps.time.hour() < 10) gps_h="0"+gps.time.hour();
    if (gps.time.hour() > 9) gps_h=gps.time.hour();
    String gps_m;
    if (gps.time.minute() < 10) gps_m="0"+gps.time.minute();
    if (gps.time.minute() > 9) gps_m=gps.time.minute();
    //String gps_s;
    //if (gps.time.second() < 10) gps_s="0"+gps.time.second();
    //if (gps.time.second() > 9) gps_s="0"+gps.time.second();
    gpsdatetime = gpsdatetime +gps_h+":"+gps_m+" ";
    
  }
  else
    {
      gpsdatetime = gpsdatetime + "INVALID TIME";
    }
  return gpsdatetime;
}

static void MasterStateLogic()
{
  if (masterState == 0) //check for apropriate config
    {
      turnOffNetlight(); //Switch off Carrier indicator transistor
      ledblink(5); //flash blue LED for 5 seconds inicating Main startup
      if (detectHomeSSID(homessid) || detectHomeSSID(althomessid))
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
     if (detectHomeSSID(homessid) || detectHomeSSID(althomessid))
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
     if (detectHomeSSID(homessid) || detectHomeSSID(althomessid))
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
