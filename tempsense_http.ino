#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
//#include "LowPower.h"
#include <PString.h>

#define ONE_WIRE_BUS 8 // Data wire is pin 4 on the Arduino
#define TEMPERATURE_PRECISION 9 // 9 bit precision corresponds to 0.5 degrees C resolution
#define SENSOR_ID "68161" // Our ID for reports

// The period in milliseconds to wait in between reports. 1 hour = 1000 * 60 * 60 * 1
//#define REPORT_INTERVAL 3600000
#define REPORT_INTERVAL 60000

#define COSM_API_KEY "aBUpZ_sy_SeTdiYGic-7m4fkGMOSAKxjd00xR1BFc3lkST0g"
#define APN "internet.cxn"

float waterTemp;
float ambientTemp;

// Create a 1-Wire bus for the temp sensors to use
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// These device addresses were determined in advance by running some of the DallasTemperature library example code
DeviceAddress waterSensor =   {0x28, 0x5E, 0x7A, 0x14, 0x04, 0x00, 0x00, 0x5B};
DeviceAddress ambientSensor = {0x28, 0x6F, 0x35, 0x14, 0x04, 0x00, 0x00, 0x07};

// Used to save the last point in time that a report was sent
unsigned long lastReportTime;

// Used to save the last point in time we took a temperature reading
unsigned long lastTempSampleTime;

// Create a soft serial port on pins 7(RX) and 8(TX) for modem
SoftwareSerial modem(7,8);

char incoming_char = 0;
char strBuffer[288];

unsigned char buffer[64]; // buffer array for data recieve over serial port
int count=0;     // counter for buffer array 

void modemSend(String command, int interTime=1000){
  modem.print(command);
  modem.write(byte(13));
  delay(interTime);
}


PString outputJSON(void)
{
  PString jsonOut(strBuffer,sizeof(strBuffer));
  jsonOut.print("{\"version\":\"1.0.0\",\"datastreams\":");
  jsonOut.print("[{\"id\":\"Water\", \"current_value\":\"");
  jsonOut.print(waterTemp);
  jsonOut.print("\"},");
  jsonOut.print("{\"id\":\"Air\", \"current_value\":\"");
  jsonOut.print(ambientTemp);
  jsonOut.print("\"}]}");
  return(jsonOut);
}

PString outputCSV(void)
{
  PString csvOut(strBuffer,sizeof(strBuffer));
  csvOut.print("Water,");
  csvOut.println(waterTemp);
  csvOut.print("Air,");
  csvOut.println(ambientTemp);
  return(csvOut);
}

void powerUpOrDown(void)
{
  pinMode(9, OUTPUT); 
  digitalWrite(9,LOW);
  delay(1000);
  digitalWrite(9,HIGH);
  delay(2000);
  digitalWrite(9,LOW);
  delay(10000);
}

void establishNetwork(void)
{
  modemSend("AT");
  modemSend("AT+SAPBR=3,1,\"Contype\",\"GPRS\"",2000);
  modemSend("AT+SAPBR=3,1,\"APN\",\""+String(APN)+"\"",5000);
  modemSend("AT+SAPBR=1,1",5000);
  modemSend("AT+SAPBR=2,1",2000);
}

void shutdownNetwork(void)
{
  Serial.println("Shutting down GPRS context");
  modemSend("AT+SAPBR=0,1",2000);
}

void sleepModem(boolean yesNo)
{
  if( yesNo == true ) {
    modemSend("AT+CSCLK=2");
  } else {
    modemSend("AT+CSCLD=0");
  }
}

void sendToCosm(void)
{
  PString json=(strBuffer,sizeof(strBuffer),outputCSV());
  int jsonSize=json.length();
  String url;
  url = "http://api.cosm.com/v2/feeds/"+String(SENSOR_ID)+".csv?_method=put&key="+String(COSM_API_KEY);
  Serial.println("Sending data via HTTP");
  //Serial.println(json);
  modemSend("AT+HTTPINIT");
  modemSend("AT+HTTPPARA=\"CID\",1");
  modemSend("AT+HTTPPARA=\"URL\",\""+url+"\"");
  modemSend("AT+HTTPDATA="+String(jsonSize)+",3000");
  modemSend(String(json));
  modemSend("AT+HTTPACTION=1");
  modemSend("AT+HTTPTERM");
}
  
void setup(void)
{
  // Start the serial ports
  Serial.begin(9600);  
  modem.begin(9600);
 
  Serial.println("I'm Alive");
  modemSend("ATE1");
  // Start up the temperatue sensors
  sensors.begin();

  // Set the resolution to use for measurement
  sensors.setResolution(waterSensor, TEMPERATURE_PRECISION);
  sensors.setResolution(ambientSensor, TEMPERATURE_PRECISION);

  lastReportTime = lastTempSampleTime = millis();
}

void sampleTemps(void)
{
  // Get the temperatures from both sensors
  sensors.requestTemperatures();
  waterTemp = sensors.getTempC(waterSensor);
  ambientTemp = sensors.getTempC(ambientSensor);
}

void loop(void)
{
  //LowPower.idle(SLEEP_1S,ADC_OFF,TIMER2_OFF,TIMER1_OFF,TIMER0_ON,SPI_OFF,USART0_OFF,TWI_OFF);  
  //LowPower.powerDown(SLEEP_1S,ADC_OFF,BOD_OFF);  

  unsigned long newReportTime = millis();
  
  // Or send a message if we have reached the report interval
  if ((newReportTime - lastReportTime) > REPORT_INTERVAL)
  {
    Serial.println("Sending Report");
    sampleTemps();
    lastReportTime = newReportTime;
    //powerUpOrDown();
    establishNetwork();
    sendToCosm();
    shutdownNetwork();
    //powerUpOrDown();
  }
  
  while (modem.available() > 0)
  {
    // Take any incoming data from the SM5100B modem and send it to the terminal
    {
      buffer[count++]=modem.read();     // writing data into array
      if(count == 64)break;
    }
    Serial.write(buffer,count);      // if no data transmission ends, write buffer to hardware serial port
    clearBufferArray();              // call clearBufferArray function to clear the storaged data from the array
    count = 0;                       // set counter of while loop to zero
  }
  
  // Relay input from the terminal to the SM5100B modem
  while (Serial.available() > 0)
  {
    // If the character is CTRL-Z then set a flag we will check later to print the current temps to the terminal
     modem.write(Serial.read());
  }
  
}
