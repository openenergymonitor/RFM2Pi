# emonBase_rfm69pi

**Version 1.1.0**

RFM69Pi firmware supporting via `#define` before compilation either LowPowerLabs radio format or JeeLib native radio format.

## EmonHub Interfacer

```
    [[RFM69PI]]
        Type = EmonHubOEMInterfacer
        [[[init_settings]]]
            com_port = /dev/ttyAMA0
            com_baud = 38400
        [[[runtimesettings]]]
            pubchannels = ToEmonCMS,
            subchannels = ToRFM12,
```

## Description

This sketch runs on the Atmel ATMega328P, handling the radio traffic coming in from sensor nodes.

The radio traffic is almost completely processed within the RFM69CW radio module, all the sketch needs to do is retrieve the complete message when it becomes available. 

**LowPowerLabs radio format**<br>
Uses the LowPowerLabs RFM69 Library.

**JeeLib native format**<br>
It uses the SPI driver spi.h to interface to a patched version of the JeeLabs driver rf69.h. When a complete message is available, a call to rf.receive( ) will return a non-zero length, and the
message is formatted and sent via the serial connection to the Raspberry Pi.

It’s also possible to transmit a message – that function is handled by the outbound r.f. data handler.

Configuration values are stored in the EEPROM memory of the ATMega 328P. The task of saving the data to, and retrieving the data from, the EEPROM is handled in the configuration source file by calling functions from the oemEProm library.

## Configuration

On-line configuration is locked for the first 3 minutes after the front-end sketch has started in order to allow the Raspberry Pi to start, without any data emitted to the serial port affecting the configuration.

The following setup/configuration commands are available:

```
l - list config (terse)
L - list config (verbose)
r - restore defaults & restart
s - save to EEPROM
v - show version
V<n> - verbose mode, 1 = ON, 0 = OFF
b<n> - set r.f. band n = 4 > 433MHz, 8 > 868MHz, 9 > 915MHz
(may require hardware change)
p<nn> - set r.f. transmit power. nn (0 - 31) = -18 dBm to +13 dBm.
Default: 25 (+7 dBm)
g<nnn> - set Group (OEM default = 210)
n<nn> - set node ID (1..60)
T<ccc>\n - transmit a string.
w<n> - n = 0 for radio transmit OFF, n = 1 for Transmit ON
? - show this again
```

The command will be acknowledged, and confirmation given when saving the settings, only after verbose mode has been set. When you change one or more of the settings, the change will take effect immediately.

Option (‘s’) will save all the changes. If you do not do this, the settings will revert to the previous values at the next restart. After you save (‘s’) the changes, the new settings will be used forever, or until changed again.

If you restore the sketch default values (‘r’), all the EEPROM data is ignored and the sketch restarts immediately, using the values set in the sketch. There is then no means of recovering the EEPROM data.

If the EEPROM has been used previously and has had non-compliant values written, the EEPROM content will be ignored and the sketch will start using its own default values.
Saving the configuration will format the EEPROM and the sketch’s set values will be stored.

### Compile with PlatformIO

    pio run

### Licence

GNU GENERAL PUBLIC LICENSE v3
