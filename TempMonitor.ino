/********************************************************************/
// First we include the libraries
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <LiquidCrystal.h>  //bibliothèque de l'écran lcd
/********************************************************************/
#define NODE_ID 0xAB 
#define TX_PIN 8 
// Data wire is plugged into pin 2 on the Arduino 
#define ONE_WIRE_BUS 7 
#define ms_per_hour  3600000
#define ms_per_min    60000
#define ms_per_sec    1000
#define TEMP_SCREEN_REFRESH 1000 // Refresh temp on screen every s
#define TEMP_SEND_RATE 120 // Send temp via RF every 2 min (120*TEMP_SCREEN_REFRESH ms)
/********************************************************************/
// Setup a oneWire instance to communicate with any OneWire devices  
// (not just Maxim/Dallas temperature ICs) 
OneWire oneWire(ONE_WIRE_BUS); 
/********************************************************************/
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
/********************************************************************/ 
LiquidCrystal lcd(11,12,2,3,4,5);//Affectation des broches de l'écran
volatile float tempMax = 0;
volatile int loopCount = TEMP_SEND_RATE; // So Temp is sent at startup
const unsigned long TIME = 512;
const unsigned long TWOTIME = TIME*2;
#define SENDHIGH() digitalWrite(TX_PIN, HIGH)
#define SENDLOW() digitalWrite(TX_PIN, LOW)
byte OregonMessageBuffer[8];

/**
 * \brief    Send logical "0" over RF
 * \details  azero bit be represented by an off-to-on transition
 * \         of the RF signal at the middle of a clock period.
 * \         Remenber, the Oregon v2.1 protocol add an inverted bit first 
 */
inline void sendZero(void) 
{
  SENDHIGH();
  delayMicroseconds(TIME);
  SENDLOW();
  delayMicroseconds(TWOTIME);
  SENDHIGH();
  delayMicroseconds(TIME);
}
 
/**
 * \brief    Send logical "1" over RF
 * \details  a one bit be represented by an on-to-off transition
 * \         of the RF signal at the middle of a clock period.
 * \         Remenber, the Oregon v2.1 protocol add an inverted bit first 
 */
inline void sendOne(void) 
{
   SENDLOW();
   delayMicroseconds(TIME);
   SENDHIGH();
   delayMicroseconds(TWOTIME);
   SENDLOW();
   delayMicroseconds(TIME);
}
 
/**
* Send a bits quarter (4 bits = MSB from 8 bits value) over RF
*
* @param data Source data to process and sent
*/
 
/**
 * \brief    Send a bits quarter (4 bits = MSB from 8 bits value) over RF
 * \param    data   Data to send
 */
inline void sendQuarterMSB(const byte data) 
{
  (bitRead(data, 4)) ? sendOne() : sendZero();
  (bitRead(data, 5)) ? sendOne() : sendZero();
  (bitRead(data, 6)) ? sendOne() : sendZero();
  (bitRead(data, 7)) ? sendOne() : sendZero();
}
 
/**
 * \brief    Send a bits quarter (4 bits = LSB from 8 bits value) over RF
 * \param    data   Data to send
 */
inline void sendQuarterLSB(const byte data) 
{
  (bitRead(data, 0)) ? sendOne() : sendZero();
  (bitRead(data, 1)) ? sendOne() : sendZero();
  (bitRead(data, 2)) ? sendOne() : sendZero();
  (bitRead(data, 3)) ? sendOne() : sendZero();
}
 
/******************************************************************/
/******************************************************************/
/******************************************************************/
 
/**
 * \brief    Send a buffer over RF
 * \param    data   Data to send
 * \param    size   size of data to send
 */
void sendData(byte *data, byte size)
{
  for(byte i = 0; i < size; ++i)
  {
    sendQuarterLSB(data[i]);
    sendQuarterMSB(data[i]);
  }
}
 
/**
 * \brief    Send an Oregon message
 * \param    data   The Oregon message
 */
void sendOregon(byte *data, byte size)
{
    sendPreamble();
    //sendSync();
    sendData(data, size);
    sendPostamble();
}
 
/**
 * \brief    Send preamble
 * \details  The preamble consists of 16 "1" bits
 */
inline void sendPreamble(void)
{
  byte PREAMBLE[]={0xFF,0xFF};
  sendData(PREAMBLE, 2);
}
 
/**
 * \brief    Send postamble
 * \details  The postamble consists of 8 "0" bits
 */
inline void sendPostamble(void)
{
#ifdef DS18B20
  sendQuarterLSB(0x00);
#else
  byte POSTAMBLE[]={0x00};
  sendData(POSTAMBLE, 1);  
#endif
}
 
/**
 * \brief    Send sync nibble
 * \details  The sync is 0xA. It is not use in this version since the sync nibble
 * \         is include in the Oregon message to send.
 */
inline void sendSync(void)
{
  sendQuarterLSB(0xA);
}
 
/******************************************************************/
/******************************************************************/
/******************************************************************/
 
/**
 * \brief    Set the sensor type
 * \param    data       Oregon message
 * \param    type       Sensor type
 */
inline void setType(byte *data, byte* type) 
{
  data[0] = type[0];
  data[1] = type[1];
}
 
/**
 * \brief    Set the sensor channel
 * \param    data       Oregon message
 * \param    channel    Sensor channel (0x10, 0x20, 0x30)
 */
inline void setChannel(byte *data, byte channel) 
{
    data[2] = channel;
}
 
/**
 * \brief    Set the sensor ID
 * \param    data       Oregon message
 * \param    ID         Sensor unique ID
 */
inline void setId(byte *data, byte ID) 
{
  data[3] = ID;
}
 
/**
 * \brief    Set the sensor battery level
 * \param    data       Oregon message
 * \param    level      Battery level (0 = low, 1 = high)
 */
void setBatteryLevel(byte *data, byte level)
{
  if(!level) data[4] = 0x0C;
  else data[4] = 0x00;
}
 
/**
 * \brief    Set the sensor temperature
 * \param    data       Oregon message
 * \param    temp       the temperature
 */
void setTemperature(byte *data, float temp) 
{
  // Set temperature sign
  if(temp < 0)
  {
    data[6] = 0x08;
    temp *= -1;  
  }
  else
  {
    data[6] = 0x00;
  }
 
  // Determine decimal and float part
  int tempInt = (int)temp;
  int td = (int)(tempInt / 10);
  int tf = (int)round((float)((float)tempInt/10 - (float)td) * 10);
 
  int tempFloat =  (int)round((float)(temp - (float)tempInt) * 10);
 
  // Set temperature decimal part
  data[5] = (td << 4);
  data[5] |= tf;
 
  // Set temperature float part
  data[4] |= (tempFloat << 4);
}
 
/**
 * \brief    Set the sensor humidity
 * \param    data       Oregon message
 * \param    hum        the humidity
 */
void setHumidity(byte* data, byte hum)
{
    data[7] = (hum/10);
    data[6] |= (hum - data[7]*10) << 4;
}
 
/**
 * \brief    Sum data for checksum
 * \param    count      number of bit to sum
 * \param    data       Oregon message
 */
int Sum(byte count, const byte* data)
{
  int s = 0;
 
  for(byte i = 0; i<count;i++)
  {
    s += (data[i]&0xF0) >> 4;
    s += (data[i]&0xF);
  }
 
  if(int(count) != count)
    s += (data[count]&0xF0) >> 4;
 
  return s;
}
 
/**
 * \brief    Calculate checksum
 * \param    data       Oregon message
 */
void calculateAndSetChecksum(byte* data)
{
  int s = ((Sum(6, data) + (data[6]&0xF) - 0xa) & 0xff);
  data[6] |=  (s&0x0F) << 4;     data[7] =  (s&0xF0) >> 4;
}
 
/******************************************************************/
/******************************************************************/

  
void setup(void) 
{ 

 // start serial port 
  Serial.begin(9600); 
  lcd.begin (16, 2);//initialisation de l'écran
  lcd.setCursor(0, 0);// au caractère 0 de la ligne 0
  lcd.print(" CAPTEUR TEMP.  ");
  lcd.setCursor(0, 1);//au caractère 0 de la ligne 1
  lcd.print("  IMPRIMANTE 3D ");
  delay(2000);//affichage pendant 3secondes
  lcd.clear();//rafraichissement de l'écran
 // Start up the library 
 sensors.begin(); 

   // Create the Oregon message for a temperature only sensor (TNHN132N)
  byte ID[] = {0xEA,0x4C}; 
 
  setType(OregonMessageBuffer, ID);
  setChannel(OregonMessageBuffer, 0x20);
  setId(OregonMessageBuffer, NODE_ID);
} 

void sendTempViaRF(float temp) {
  // Handle Oregon sending
 // Set Battery Level
  setBatteryLevel(OregonMessageBuffer, 1);  // 0=low, 1=high
  Serial.print("BatteryOK ");
  // Set Temperature
  setTemperature(OregonMessageBuffer, temp);
  Serial.print("TempOK ");
  // Calculate the checksum
  calculateAndSetChecksum(OregonMessageBuffer);
  Serial.print("ChecksumOK ");
  // Send the Message over RF
  sendOregon(OregonMessageBuffer, sizeof(OregonMessageBuffer));
  Serial.print("SendOK ");
  // Send a "pause"
  SENDLOW();
  delayMicroseconds(TWOTIME*8);
  // Send a copie of the first message. The v2.1 protocol send the message two time 
  sendOregon(OregonMessageBuffer, sizeof(OregonMessageBuffer));
  SENDLOW();
Serial.print("SendOK ");
}

void loop(void) 
{ 
  // call sensors.requestTemperatures() to issue a global temperature 
  // request to all devices on the bus 
  /********************************************************************/
  Serial.print(" Requesting temperatures..."); 
  sensors.requestTemperatures(); // Send the command to get temperature readings 
  Serial.println("DONE"); 
  /********************************************************************/
  float temp = sensors.getTempCByIndex(0);
  Serial.print("Temperature is: "); 
  Serial.println(temp);
  //Serial.print(" ");
  //Serial.print(sensors.getTempCByIndex(1)); // Why "byIndex"?  
  //Serial.print(" ");
  //Serial.print(sensors.getTempCByIndex(2)); // Why "byIndex"?  

  displayTemp(temp);
  Serial.print("Display done - ");
  if (loopCount >= TEMP_SEND_RATE) {
    Serial.print(" sending RF - ");
    Serial.println(loopCount);
    loopCount = 0;
    sendTempViaRF(temp);
  } else {
    Serial.print(" not sending yet");
    Serial.println(loopCount);
  }
  loopCount++;

  Serial.println("Sending part done - waiting");
  delay(TEMP_SCREEN_REFRESH); 
} 

void displayTemp(float temp)
{
  if (temp > tempMax) {
    tempMax = temp;
  }
  // 1ere ligne
  lcd.setCursor (0, 0);
  lcd.print ("Temp:     ");
  //lcd.setCursor(7,0);
  //lcd.write((char)223);
  lcd.setCursor(5,0);
  lcd.print(temp,1);

  long now = millis();
  byte hours = (now / ms_per_hour);
  now -= (hours * ms_per_hour);
  byte minutes = (now / ms_per_min);
  char duration[5];
  sprintf(duration,"%02d:%02d",hours,minutes);
  lcd.setCursor(11,1);
  lcd.print(duration);

  
  // 2e ligne
  lcd.setCursor (0, 1); //au caractére 2 de la ligne 1
  lcd.print("Max : ");
  lcd.setCursor(5,1);
  lcd.print(tempMax,1); //Information récupérée sur l'adresse 0 de la sonde
}
