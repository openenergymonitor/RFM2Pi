/*


  emonBase rfm69pi LowPowerLabs radio format
  
   ------------------------------------------
  Part of the openenergymonitor.org project

  Authors: Glyn Hudson, Trystan Lea & Robert Wall
  Builds upon JCW JeeLabs RF69 Driver and Arduino

  Licence: GNU GPL V3


Change Log:

V1.1.1					First published release.
V1.2.0	13 July 2026	Radio Library was LowPowerLabs RFM69 (full version) -> OEM RFM69_LPL, 
						frequency was hardcoded at 433.00 MHz -> choice of 433.00, 433.92, 868 or 915 MHz. (RW)
*/
const char *firmware_version = {"1.2.0\n\r"};
/*

*********************************************************************************************
*                                                                                           *
*                                  IMPORTANT NOTE                                           *
*                                                                                           *
* When compiling for the RFM2Pi:                                                            * 
*  In IDE, set Board to "Arduino Pro or Pro Mini" & Processor to "ATmega328P (3.3V, 8MHz)"  *
* When compiling for the RFM69Pi:                                                           *
*  In IDE, set Board to "Arduino Uno"                                                       *
*                                                                                           *
* The output file used must NOT be the "with_bootloader" version, else the processor will   *
*  be locked.                                                                               *
*********************************************************************************************
*/

// #define SAMPPIN 19 

// Used to select radio format
#define RFM69_JEELIB_NATIVE 2
#define RFM69_LOW_POWER_LABS 3
#define RadioFormat RFM69_LOW_POWER_LABS

// OEM EPROM library
#include <emonEProm.h>

// RFM interface
#if RadioFormat == RFM69_LOW_POWER_LABS
  #include <RFM69_LPL.h>
  RFM69 radio;
#else
  #include "spi.h"                                                       // Requires "RFM69 Native" JeeLib Driver
  #include "rf69.h"
  RF69<SpiDev10> rf;
#endif

byte nativeMsg[66];                                                    // 'Native' format message buffer

#define MAXMSG 66                                                      // Max length of o/g message
char outmsg[MAXMSG];                                                   // outgoing message (to emonGLCD etc)
byte outmsgLength;                                                     // length of message: non-zero length triggers transmission

bool verbose = true;

struct {                                                               // Ancilliary information
  byte srcNode = 0;
  byte msgLength = 0;
  signed char rssi = -127;
  bool crc = false;
} rfInfo;

enum rfband {RFM_433MHZ = 43, RFM_433_92MHZ = 49, RFM_868MHZ = 86, RFM_915MHZ = 91};  // frequency band.
bool rfChanged = false;                                                // marker to re-initialise radio
#define RFTX 0x01                                                      // Control of RFM - transmit enabled


//----------------------------emonBase Settings------------------------------------------------------------------------------------------------
const unsigned long BAUD_RATE   = 38400;

void single_LED_flash(void);
void double_LED_flash(void);
void getCalibration(void);
static void showString (PGM_P s);


//---------------------------- Settings - Stored in EEPROM and shared with config.ino ------------------------------------------------
struct {
  byte RF_freq = RFM_433_92MHZ;                           // Frequency of radio module can be RFM_433MHZ, RFM_433_92MHZ, RFM_868MHZ or RFM_915MHZ. 
  byte rfPower = 25;                                    // Power when transmitting
  byte networkGroup = 210;                              // wireless network group, must be the same as emonBase / emonPi and emonGLCD. OEM default is 210
  byte  nodeID = 5;                                     // node ID for this emonBase.
  byte  rfOn = 0x1;                                     // Turn transmitter on -receiver is always on
} EEProm;

uint16_t eepromSig = 0x0017;                            // oemEProm signature - see oemEEProm Library documentation for details.


//--------------------------- hard-wired connections ----------------------------------------------------------------------------------------

const byte LEDpin               = 9;                                   // emonPi LED - on when HIGH

// Use D5 for ISR timimg checks - only if Pi is not connected!

//-------------------------------------------------------------------------------------------------------------------------------------------


/**************************************************************************************************************************************************
*
* SETUP        Set up & start the radio
*
***************************************************************************************************************************************************/
void setup() 
{  
  // Set I/O pins, print initial message, read configuration from EEPROM
  pinMode(LEDpin, OUTPUT);
  digitalWrite(LEDpin,HIGH);

  Serial.begin(BAUD_RATE);
  Serial.print(F("|emonBase_rfm69pi_LPL V")); Serial.write(firmware_version);
  Serial.println(F("|OpenEnergyMonitor.org"));
  load_config();                                                      // Load RF config from EEPROM (if any exists)

#ifdef SAMPPIN
  pinMode(SAMPPIN, OUTPUT);
  digitalWrite(SAMPPIN, LOW);
#endif

  delay(2000);

  #if RadioFormat == RFM69_LOW_POWER_LABS
    radio.initialize(RF69_433_92MHZ,EEProm.nodeID,EEProm.networkGroup);  
    radio.encrypt("89txbe4p8aik5kt3");
  #else
    rf.init(EEProm.nodeID, EEProm.networkGroup, 
               EEProm.RF_freq == RFM_915MHZ ? 915                      // Fall through to 433 MHz Band @ 434 MHz
            : (EEProm.RF_freq == RFM_868MHZ ? 868 : 434)); 
  #endif
  digitalWrite(LEDpin, LOW);
}

/**************************************************************************************************************************************************
*
* LOOP         Poll the radio for incoming data, and the serial input for calibration & outgoing r.f. data 
*
***************************************************************************************************************************************************/

void loop()             
{
  
//-------------------------------------------------------------------------------------------------------------------------------------------
// RF Data handler - inbound ****************************************************************************************************************
//-------------------------------------------------------------------------------------------------------------------------------------------

  #if RadioFormat == RFM69_LOW_POWER_LABS
  if (radio.receiveDone())
  {    
    // Copy
    rfInfo.srcNode = radio.SENDERID;
    rfInfo.msgLength = radio.DATALEN;
    for (byte i = 0; i < radio.DATALEN; i++) {
      nativeMsg[i] = radio.DATA[i];
    }
    rfInfo.rssi = radio.readRSSI();

    // Send ACK
    if (radio.ACKRequested()) {
      radio.sendACK();
    }
  
    // Print frame
    Serial.print(F("OK")); 
    Serial.print(F(" "));
    Serial.print(rfInfo.srcNode, DEC);
    Serial.print(F(" "));
    for (byte i = 0; i < rfInfo.msgLength; i++) {
      Serial.print((word)nativeMsg[i]);
      Serial.print(F(" "));
    }
    Serial.print(F("("));
    Serial.print(rfInfo.rssi);
    Serial.print(F(")"));
    Serial.println();
    
    // Flash LED
    double_LED_flash();
  }
  #else
  int len = rf.receive(&nativeMsg, sizeof(nativeMsg));                 // Poll the RFM buffer and extract the data
  if (len > 1)
  {
    rfInfo.crc = true;
    rfInfo.msgLength = len;
    rfInfo.srcNode = nativeMsg[1];
    rfInfo.rssi = -rf.rssi/2;

    // send serial data
    Serial.print(F("OK"));                                              // Bad packets (crc failure) are discarded by RFM69CW
    Serial.print(F(" "));
    Serial.print(rfInfo.srcNode);        // Extract and print node ID
    Serial.print(F(" "));
    for (byte i = 2; i < rfInfo.msgLength; ++i) 
    {
      Serial.print((word)nativeMsg[i]);
      Serial.print(F(" "));
    }
    Serial.print(F("("));
    Serial.print(rfInfo.rssi);
    Serial.print(F(")"));
    Serial.println();
    double_LED_flash();
  }
  #endif
//-------------------------------------------------------------------------------------------------------------------------------------------
// RF Data handler - outbound ***************************************************************************************************************
//-------------------------------------------------------------------------------------------------------------------------------------------


	if ((EEProm.rfOn & RFTX) && outmsgLength) {                          // if command 'outmsg' is waiting to be sent then let's send it
    showString(PSTR(" -> "));
    Serial.print((word) outmsgLength);
    showString(PSTR(" b\n"));

    #if RadioFormat == RFM69_LOW_POWER_LABS
      radio.send(0, (void *)outmsg, outmsgLength);
    #else
      rf.send(0, (void *)outmsg, outmsgLength);                    //  void RF69<SPI>::send (uint8_t header, const void* ptr, int len) {
    #endif
    
    outmsgLength = 0;
    single_LED_flash();
	}

  
//-------------------------------------------------------------------------------------------------------------------------------------------
// Calibration Data handler *****************************************************************************************************************
//-------------------------------------------------------------------------------------------------------------------------------------------

  if (Serial.available())                                              // Serial input from RPi for configuration/calibration
  {
    getCalibration();                                                  // If serial input is received from RPi
    double_LED_flash();
    if (rfChanged)
    {   
      #if RadioFormat == RFM69_LOW_POWER_LABS
        radio.initialize(EEProm.RF_freq,EEProm.nodeID,EEProm.networkGroup);
        radio.encrypt("89txbe4p8aik5kt3");
      #else
        rf.init(EEProm.nodeID, EEProm.networkGroup, 
                   EEProm.RF_freq == RFM_915MHZ ? 915                      // Fall through to 433 MHz Band @ 434 MHz
                : (EEProm.RF_freq == RFM_868MHZ ? 868 : 434)); 
      #endif
            
      rfChanged = false;
    }
  }

}


// LED flash
void single_LED_flash(void)
{
  digitalWrite(LEDpin, HIGH);  delay(30); digitalWrite(LEDpin, LOW);
}

void double_LED_flash(void)
{
  digitalWrite(LEDpin, HIGH);  delay(10); digitalWrite(LEDpin, LOW); delay(30); 
  digitalWrite(LEDpin, HIGH);  delay(10); digitalWrite(LEDpin, LOW);
}