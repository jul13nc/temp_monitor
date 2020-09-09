# temp_monitor
Arduino temperature Monitor with LCD screen

Temperature is read from DS18B20 tempaerature sensor
It is displayed every s on the screen (as well as the max temperature, and the time the sensor is runnung)

Every 120 cycle, the temperature is sent via RF 433 (following oregon V2 protocol)

DS18B20 is connected to pin 7
RF 433 sender is connected to pin 8
LCD Screen is connected to PIN 2,3,4,5,11,12
