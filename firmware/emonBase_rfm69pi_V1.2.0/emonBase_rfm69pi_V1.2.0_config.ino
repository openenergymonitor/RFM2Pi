/*
Configuration functions for emonBase_rfm69n.ino


EEPROM layout

Byte
0       RF Band
1       Group
2       rfOn
*/


#include <Arduino.h>
#include <avr/pgmspace.h>
#include <EEPROM.h>

 // Available Serial Commands
const PROGMEM char helpText1[] =                                
"|\n"
"|Available commands:\n"
"| l\t\t- list config (terse)\n"
"| L\t\t- list config (verbose)\n"
"| r\t\t- restore defaults & restart\n"
"| s\t\t- save to EEPROM\n"
"| v\t\t- show version\n"
"| V<n>\t\t- verbose mode, 1 = ON, 0 = OFF\n"
"| b<nn>\t\t- set r.f. band n = an integer 0<n<255: 4 = 433MHz, 49 = 433.92MHz, 8 = 868MHz, 9 = 915MHz (may require hardware change)\n"
"| p<nn>\t\t- set r.f. transmit power. nn (0 - 31) = -18 dBm to +13 dBm. Default: 25 (+7 dBm)\n"
"| g<nnn>\t- set Group (OEM default = 210)\n"
"| n<nn>\t\t- set node ID (1..60)\n"
"| T<ccc>\\n\t- transmit a string.\n"  
"| w<n>\t\t- n = 0 for radio transmit OFF, n = 1 for Transmit ON\n" 
"| ?\t\t- show this again\n|"
;



#define SERIAL_LOCK 2000                                               // Lockout period (ms) after 'old-style' config command

static void load_config(void)
{
  eepromRead(eepromSig, (byte *)&EEProm);
}

static void list_calibration(void)
{
  
  Serial.println(F("|Settings"));
  Serial.print(F("|Radio: ")); Serial.println(EEProm.rfOn);
  Serial.print(F("|RF Band: "));
  if (EEProm.RF_freq == RFM_433MHZ) Serial.println(F("433MHz"));
  if (EEProm.RF_freq == RFM_433_92MHZ) Serial.println(F("433.92MHz"));  
  if (EEProm.RF_freq == RFM_868MHZ) Serial.println(F("868MHz"));
  if (EEProm.RF_freq == RFM_915MHZ) Serial.println(F("915MHz")); 
  Serial.print(F("|Power: "));Serial.print(EEProm.rfPower - 18);Serial.println(F(" dBm"));
  Serial.print(F("|Group: ")); Serial.println(EEProm.networkGroup);
  Serial.print(F("|Node ID: ")); Serial.println(EEProm.nodeID);
}

static void report_calibration(void)
{
  Serial.print(F("Settings:"));
  Serial.print(F(" r")); Serial.print(EEProm.rfOn);
  Serial.print(F(" b"));
  if (EEProm.RF_freq == RFM_433MHZ) Serial.print(F("4"));
  if (EEProm.RF_freq == RFM_433_92MHZ) Serial.print(F("49"));
  if (EEProm.RF_freq == RFM_868MHZ) Serial.print(F("8"));
  if (EEProm.RF_freq == RFM_915MHZ) Serial.print(F("9")); 
  Serial.print(F(" p")); Serial.print(EEProm.rfPower);
  Serial.print(F(" g")); Serial.print(EEProm.networkGroup);
  Serial.print(F(" i")); Serial.print(EEProm.nodeID);
}

static void save_config()
{
  eepromWrite(eepromSig, (byte *)&EEProm, sizeof(EEProm));
  if (verbose)
  {
    eepromPrint(true);
    Serial.println(F("\r\n|Config saved\r\n|"));
  }
}

static void wipe_eeprom(void)
{
  if (verbose)
  {
    Serial.print(F("|Resetting..."));
  }
  eepromHide(eepromSig);   
  if (verbose)
  {
    Serial.println(F("|Sketch restarting with default config."));
  }
}

void softReset(void)
{
  asm volatile ("  jmp 0");
}

void getCalibration(void)
{
/*
 * Reads calibration information (if available) from the serial port at runtime. 
 * Data is expected generally in the format
 * 
 *  [l] [x] [y] [z]
 * 
 * where:
 *  [l] = a single letter denoting the variable to adjust
 *  [x] [y] [z] are values to be set.
 *  see the user instruction above, the comments below or the separate documentation for details
 * 
 */

	if (Serial.available())
  {   
    char c = Serial.peek();
    char* msg;
    
    if (!lockout(c))
      switch (c) {
        
        case 'b':  // set band: 4 = 433, 49 = 433.92, 8 = 868, 9 = 915
          EEProm.RF_freq = bandToFreq(Serial.parseInt());
          if (verbose)
          {
            Serial.print(F("|RF Band = "));
            if (EEProm.RF_freq == RFM_433MHZ) Serial.println(F("433MHz"));
            if (EEProm.RF_freq == RFM_433_92MHZ) Serial.println(F("433.92MHz"));
            if (EEProm.RF_freq == RFM_868MHZ) Serial.println(F("868MHz"));
            if (EEProm.RF_freq == RFM_915MHZ) Serial.println(F("915MHz"));
          }
          rfChanged = true;
          break;

        case 'g':  // set network group
          EEProm.networkGroup = Serial.parseInt();
          if (verbose)
          {
            Serial.print(F("|Group ")); Serial.println(EEProm.networkGroup);
          }
          rfChanged = true;
          break;

            
        case 'L':
          list_calibration(); // print the calibration values (verbose)
          break;
            
        case 'l':
          report_calibration(); // report calibration values to emonHub (terse)
          break;
            
        case 'n':
        case 'i':  //  Set NodeID - range expected: 1 - 60
          EEProm.nodeID = Serial.parseInt();
          EEProm.nodeID = constrain(EEProm.nodeID, 1, 63);
          if (verbose)
          {
            Serial.print(F("|Node ")); Serial.println(EEProm.nodeID);
          }
          rfChanged = true;
          break;

        case 'p': // set RF power level
          EEProm.rfPower = (Serial.parseInt() & 0x1F);
          if (verbose)
          {
            Serial.print(F("|p: "));Serial.print((EEProm.rfPower & 0x1F) - 18);Serial.println(F(" dBm"));
          }
          rfChanged = true;
          break; 

        case 'r':
          wipe_eeprom(); // restore sketch defaults
          softReset();
          break;

        case 's' :
          save_config(); // Save to EEPROM. ATMega328p has 1kB  EEPROM
          break;
            
        case 'T': // write alpha-numeric string to be transmitted.
          msg = outmsg;
          {
            char c = 0;
            byte len = 0;
            Serial.read();  // discard 'w'
            while (c != '\n' && len < MAXMSG)
            {
              c = Serial.read();
              if (c > 31 && c < 127)
              {
                *msg++ = c;
                len++;
              }
            }
            outmsgLength = len;
          }          
          break;
        
        case 'v': // print firmware version
          Serial.print(F("|emonPi CM V")); Serial.write(firmware_version);
          break;

        case 'V': // Verbose mode
          /*
          * Format expected: V0 | V1
          */
          verbose = (bool)Serial.parseInt();    
          Serial.print(F("|Verbose mode "));Serial.println(verbose?F("on"):F("off"));
         break;
          
        case 'w':
          /*
          * Wireless off = 0, tx = 1
          * Format expected: w0 or w1 -- rx is always enabled
          */
          EEProm.rfOn = Serial.parseInt();
          if (verbose)
          {
            Serial.print(F("|Radio "));;Serial.println(EEProm.rfOn);
          }
          break;

        case '?':  // show Help text        
          showString(helpText1);
          Serial.println();
          break;

        default:
          ;
      }
      // flush the input buffer
      while (Serial.available())
        Serial.read();
  }
}

bool lockout(char c)
{
  static bool locked = false;
  static unsigned long locktime;
  
  if (c > 47 && c < 58)                                                // lock out old 'Reverse Polish' format: numbers first.
  {
    locked = true;
    locktime = millis();
    while (Serial.available())
      Serial.read();
  }
  else if ((millis() - locktime) > SERIAL_LOCK)
  {
    locked = false;
  }
  return locked;
}  
  
static byte bandToFreq (byte band) {
  return band == 43 ? RFM_433MHZ : band == 49 ? RFM_433_92MHZ : band == 86 ? RFM_868MHZ : band == 91 ? RFM_915MHZ : 0;
}

static void showString (PGM_P s) 
{
  for (;;) 
  {
    char c = pgm_read_byte(s++);
    if (c == 0)
      break;
    if (c == '\n')
      Serial.print('\r');
    Serial.print(c);
  }
}

byte c2h(byte b)
{
  if (b > 47 && b < 58) 
    return b - 48;
  else if (b > 64 && b < 71) 
    return b - 55;
  else if (b > 96 && b < 103) 
    return b - 87;
  return 0;
}