# ESP32_GPSTracker

This code is based on the LiliGO TTGO T-Call v1.4
<img src="lilygo Tcall 1.4.jpg" alt="LiliGO TTGO T-Call v1.4" width="600"/>

It uses a generic NEO-6M GPS module connected via soft serial and a custom built power module to switch on and off the VCC power for the external GPS Module depending on the state of the module.
<img src="Power Switching circuit.png" alt="600mA Power Switching circuit" width="300"/>

The code accepts commands via SMS that allows you to: 
- Get the operational state of the module
- Get the GPS coordinates via a Google Maps URL
- Restart the module
- Get a list of nearby WiFi SSIDs and their RSSI
