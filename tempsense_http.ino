#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>

volatile int f_wdt=1;

#define ONE_WIRE_BUS 11 // Data wire is pin 4 on the Arduino
#define TEMPERATURE_PRECISION 9 // 9 bit precision corresponds to 0.5 degrees C resolution

// The period in milliseconds to wait in between reports. 1 hour = 1000 * 60 * 60 * 1 / 8
#define REPORT_INTERVAL 450000
//#define REPORT_INTERVAL 15000

String APN="internet.cxn";

float waterTemp;
float ambientTemp;

// Create a 1-Wire bus for the temp sensors to use
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// QFI-1
String SENSOR_ID="68161"; // Our ID for reports
String COSM_API_KEY="aBUpZ_sy_SeTdiYGic-7m4fkGMOSAKxjd00xR1BFc3lkST0g";
DeviceAddress waterSensor =   {
  0x28, 0x5E, 0x7A, 0x14, 0x04, 0x00, 0x00, 0x5B};
DeviceAddress ambientSensor = {
  0x28, 0x6F, 0x35, 0x14, 0x04, 0x00, 0x00, 0x07};

/*
  // QFI-2
 String SENSOR_ID="69837"; // Our ID for reports
 String COSM_API_KEY="KoBLICFuh3xUPCe50KU-ltkkV_CSAKxML1E2NmlmSFRDdz0g";
 DeviceAddress waterSensor = {0x28, 0x1E, 0x55, 0x14, 0x04, 0x00, 0x00, 0xB9};
 DeviceAddress ambientSensor = {0x28, 0xBB, 0x55, 0x14, 0x04, 0x00, 0x00, 0x0E};
 */

// Used to save the last point in time that a report was sent
unsigned long lastReportTime;

// Used to save the last point in time we took a temperature reading
unsigned long lastTempSampleTime;

String msg;

// Create a soft serial port on pins 7(RX) and 8(TX) for modem
SoftwareSerial modem(7,8);

ISR(WDT_vect)
{
  if(f_wdt == 0)
  {
    f_wdt=1;
  }
  else
  {
    Serial.println("WDT Overrun!!!");
  }
}

void setup()
{
  // Start the serial ports
  Serial.begin(19200);  
  modem.begin(19200);
  delay(500);

  // Start up the temperatue sensors
  sensors.begin();

  // Set the resolution to use for measurement
  sensors.setResolution(waterSensor, TEMPERATURE_PRECISION);
  sensors.setResolution(ambientSensor, TEMPERATURE_PRECISION);

  // Reset the counter
  lastReportTime = lastTempSampleTime = millis();

  /*** Setup the WDT ***/

  /* Clear the reset flag. */
  MCUSR &= ~(1<<WDRF);

  /* In order to change WDE or the prescaler, we need to
   * set WDCE (This will allow updates for 4 clock cycles).
   */
  WDTCSR |= (1<<WDCE) | (1<<WDE);

  /* set new watchdog timeout prescaler value */
  WDTCSR = 1<<WDP0 | 1<<WDP3; /* 8.0 seconds */

  /* Enable the WD interrupt (note no reset). */
  WDTCSR |= _BV(WDIE);
}

void loop()
{
  if(f_wdt == 1)
  {
    delay(1000);
    unsigned long newReportTime = millis();

    // Or send a message if we have reached the report interval
    if ((newReportTime - lastReportTime) >= REPORT_INTERVAL)
    {
      wdt_disable();
      lastReportTime = newReportTime;
      powerUp();
      sendToCosm();
      powerDown();
      wdt_enable(8000);
    }
    if (modem.available())
      Serial.write(modem.read());

    f_wdt = 0;
    enterSleep();   
  }
}

void enterSleep(void)
{
  set_sleep_mode(SLEEP_MODE_PWR_SAVE);   /* EDIT: could also use SLEEP_MODE_PWR_DOWN for lowest power consumption. */
  sleep_enable();

  /* Now enter sleep mode. */
  sleep_mode();

  /* The program will continue from here after the WDT timeout*/
  sleep_disable(); /* First thing to do is disable sleep. */

  /* Re-enable the peripherals. */
  power_all_enable();
}

void ShowSerialData()
{
  while(modem.available()!=0)
    Serial.write(modem.read());
}

void powerUp()
{
  pinMode(9, OUTPUT); 
  digitalWrite(9,LOW);
  delay(1000);
  digitalWrite(9,HIGH);
  delay(2000);
  digitalWrite(9,LOW);
  delay(3000);
  delay(20000);
}

void powerDown()
{
  modem.println("AT+CPOWD=1");
}
void sendToCosm()
{ 
  sensors.requestTemperatures();
  waterTemp = sensors.getTempC(waterSensor);
  ambientTemp = sensors.getTempC(ambientSensor);

  modem.println("AT+CGATT?");
  delay(100);

  ShowSerialData();

  modem.println("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  delay(500);

  ShowSerialData();

  modem.println("AT+SAPBR=3,1,\"APN\",\""+APN+"\"");
  delay(500);

  ShowSerialData();

  modem.println("AT+SAPBR=1,1");
  delay(5000);

  ShowSerialData();

  modem.println("AT+SAPBR=2,1");
  delay(5000);

  ShowSerialData();

  modem.println("AT+HTTPINIT");
  delay(200);

  ShowSerialData();

  modem.println("AT+HTTPPARA=\"CID\",1");
  delay(200);

  ShowSerialData();

  modem.print("AT+HTTPPARA=\"URL\",");
  delay(200);

  ShowSerialData();

  modem.print("\"http://api.cosm.com/v2/feeds/"+SENSOR_ID+".csv?");
  delay(200);

  ShowSerialData();

  modem.println("_method=put&key="+COSM_API_KEY+"\"");
  delay(500);

  ShowSerialData();

  modem.println("AT+HTTPDATA=22,2000");
  delay(100);

  ShowSerialData();

  modem.print("Water,");
  modem.println(waterTemp);
  modem.print("Air,");
  modem.println(ambientTemp);
  delay(2000);

  modem.println("AT");
  delay(200);

  ShowSerialData();

  modem.println("AT+HTTPACTION=1");
  delay(5000);

  ShowSerialData();

  modem.println("AT+HTTPTERM");
  delay(5000);

  ShowSerialData();

  modem.println("AT+SAPBR=0,1");
  delay(5000);

  ShowSerialData();
}




