# ESP8266-home-battery-control-
This is a project to control a LiFePo4 battery for home storage with a charger and an inverter and connect it to an existing OpenWB Wallbox.
I use an Sun GTIL 2000 inverter with Truckis gateway (https://github.com/trucki-eu/Trucki2Shelly-Gateway) to control it with a Wemos D1 Mini pro
As charger, an old 48V server power supply is used that was amended to be able to control the voltage and the current, much like a normal desktop power supply. The potentiometer to adjust the current is swapped to a digital potentiometer (X9C103S: https://www.ebay.de/itm/284116615364?var=585607053245) controlled by the Wemos D1 board.
An Allegro ACS758LCB-050B-PFF-T is used to measure the current running in or out of the battery.
A ADS 1115 A/D converter is used with the ESP 8266 Board to measure the current from the Allegro and the battery voltage (easy made with a voltage divider)
For switching on and off the charger and the inverter, two solid state relais are used. I choosed two SSR-40DA for this: https://www.ebay.de/itm/373939652460
The connection to and from the OpenWB wallbox, which has a builtin Mosquitto MQTT broker is done via MQTT.

The current circuit looks like this:
![Hausakku](https://user-images.githubusercontent.com/12899111/235508035-44413899-c5f5-417d-9cb4-c3c2148462b9.png)
