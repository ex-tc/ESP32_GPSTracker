/*
 * This is the consolidated configuration file, all functianal definitions should be stored here
 */




 //Digital IO Pins
#define MODEM_RST_PIN         5
#define MODEM_PWRKEY_PIN      4
#define MODEM_POWER_ON_PIN   23
#define MODEM_TX_PIN         27
#define MODEM_RX_PIN         26
#define GPS_POWER_ON_PIN   25
#define GPS_TX_PIN         14
#define GPS_RX_PIN         12
#define LED_GPIO_PIN         13


 //Wifi credentials
#define WIFI_SSID              "xxxxxxx"
#define WIFI_PASSWORD          "xxxxx"
#define WIFI_SSID2             "xxxxxxxbile"
#define WIFI_PASSWORD2         "xxxxxxrxx"


 //GSM Credentials


 //Safe Sender phone number with country code
#define SAFE_SENDER_MOBILE    "+44XXXXXXXXXX"
#define SAFE_SENDER_LIST      "+44XXXXXXXXXX,+44XXXXXXXXXX"


//operational

#define DEBUGMODE "true"            //Eneable the console output
#define ENABLE_SLEEP_TIMERS "true"  //enables the sleep timers

//Timers
#define GLBL_RST_INTERVAL       28800000  //reboot the device every 8h incase it enters a failed state.
#define GLBL_GPS_INTERVAL       30000     //default GPS location test interval in ms
#define GLBL_SMS_INTERVAL       15000     //default SMS test interval in ms
#define GLBL_SMS_HOME_INTERVAL  120000    //default SMS test interval delay in ms when home
#define GLBL_WIFI_INTERVAL      300000    //default WiFi sniffer interval in ms
