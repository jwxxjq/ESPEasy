#ifdef USES_P200
//#######################################################################################################
//############################## Plugin 200: Plantower custom sensor ####################################
//#######################################################################################################
//
// self build sensor for environment, include PM2.5, PM10, CO2, HCHO, Temperature and Humidity
// using UART interface, like PMSx00x
// UART format:
/*
* 1-byte:  0xa5 (header)
* 2-byte:  0x5a (header)
* 3-byte:  high byte of PM2.5 (ug/m^3)
* 4-byte:   low byte of PM2.5 (ug/m^3)
* 5-byte:  high byte of PM10 (ug/m^3)
* 6-byte:   low byte of PM10 (ug/m^3)
* 7-byte:  high byte of CO2 (ppm)
* 8-byte:   low byte of CO2 (ppm)
* 9-byte:  high byte of HCHO (ppm)
* 10-byte:  low byte of HCHO (ppm)
* 11-byte: integer byte of Temperature
* 12-byte: decimal byte of Temperature
* 13-byte: integer byte of Humidity
* 14-byte: decimal byte of Humidity
* 15-byte: high byte of chechsum
* 16-byte:  low byte of chechsum
*
* test data: a55a000102030405060708090a0b0141
* PM2.5 = 1
* PM10 = 515
* CO2 = 1029
* HCHO = 1543
* TEMP = 8.9
* RH = 10.11
*/


#include <ESPeasySerial.h>
#include "_Plugin_Helper.h"

#define PLUGIN_200
#define PLUGIN_ID_200 200
#define PLUGIN_NAME_200 "Environment - custom_sensor"
#define PLUGIN_VALUENAME1_200 "PM2.5"
#define PLUGIN_VALUENAME2_200 "PM10"
#define PLUGIN_VALUENAME3_200 "CO2"
#define PLUGIN_VALUENAME4_200 "HCHO"
#define PLUGIN_VALUENAME5_200 "Temperature"
#define PLUGIN_VALUENAME6_200 "Humidity"

#define CUSTOM_SENSOR_SIG1 0xa5
#define CUSTOM_SENSOR_SIG2 0x5a
#define CUSTOM_SENSOR_SIZE 16
#define CUSTOM_SENSOR_VALUE_COUNT 6

ESPeasySerial *P200_easySerial = nullptr;
boolean Plugin_200_init = false;
boolean Plugin_200_values_received = false;

// Read 2 bytes from serial and make an uint16 of it. Additionally calculate
// checksum for sensor. Assumption is that there is data available, otherwise
// this function is blocking.
void Plugin_200_SerialRead16(uint16_t* value, uint16_t* checksum)
{
  uint8_t data_high, data_low;

  // If P200_easySerial is initialized, we are using soft serial
  if (P200_easySerial == nullptr) return;
  data_high = P200_easySerial->read();
  data_low = P200_easySerial->read();

  *value = data_low;
  *value |= (data_high << 8);

  if (checksum != nullptr)
  {
    *checksum += data_high;
    *checksum += data_low;
  }

#if 0
  // Low-level logging to see data from sensor
  String log = F("CUSTOM_SENSOR : byte high=0x");
  log += String(data_high,HEX);
  log += F(" byte low=0x");
  log += String(data_low,HEX);
  log += F(" result=0x");
  log += String(*value,HEX);
  addLog(LOG_LEVEL_INFO, log);
#endif
}

void Plugin_200_SerialFlush() {
  if (P200_easySerial != nullptr) {
    P200_easySerial->flush();
  }
}

boolean Plugin_200_PacketAvailable(void)
{
  if (P200_easySerial != nullptr) // Software serial
  {
    // When there is enough data in the buffer, search through the buffer to
    // find header (buffer may be out of sync)
    if (!P200_easySerial->available()) return false;
    while ((P200_easySerial->peek() != CUSTOM_SENSOR_SIG1) && P200_easySerial->available()) {
      P200_easySerial->read(); // Read until the buffer starts with the first byte of a message, or buffer empty.
    }
    if (P200_easySerial->available() < CUSTOM_SENSOR_SIZE) return false; // Not enough yet for a complete packet
  }
  return true;
}

boolean Plugin_200_process_data(struct EventStruct *event) {
  uint16_t checksum = 0, checksum2 = 0;
//  uint16_t framelength = 0;
  uint16_t packet_header = 0;
  Plugin_200_SerialRead16(&packet_header, &checksum); // read CUSTOM_SENSOR_SIG1 + CUSTOM_SENSOR_SIG2
  if (packet_header != ((CUSTOM_SENSOR_SIG1 << 8) | CUSTOM_SENSOR_SIG2)) {
    // Not the start of the packet, stop reading.
    return false;
  }
/*
  SerialRead16(&framelength, &checksum);
  if (framelength != (CUSTOM_SENSOR_SIZE - 2))
  {
    if (loglevelActiveFor(LOG_LEVEL_ERROR)) {
      String log = F("CUSTOM_UART : invalid framelength - ");
      log += framelength;
      addLog(LOG_LEVEL_ERROR, log);
    }
    return false;
  }
*/
  uint16_t data[CUSTOM_SENSOR_VALUE_COUNT]; // byte data_low, data_high;
  for (int i = 0; i < CUSTOM_SENSOR_VALUE_COUNT; i++)
    Plugin_200_SerialRead16(&data[i], &checksum);
/*
  String log = F("CUSTOM SENSOR : PM2.5=");
  log += data[0];
  log += F(", PM10=");
  log += data[1];
  log += F(", CO2=");
  log += data[2];
  log += F(", HCHO=");
  log += data[3];
  log += F(", Temperature=");
  log += data[4]/256 + (float(data[4]%256))/100;
  log += F(" ℃, Humidity=");
  log += data[5]/256 + (float(data[5]%256))/100;
  log += F(" %");
  addLog(LOG_LEVEL_INFO, log);
*/

  // Compare checksums
  Plugin_200_SerialRead16(&checksum2, nullptr);
  Plugin_200_SerialFlush(); // Make sure no data is lost due to full buffer.
  if (checksum == checksum2)
  {
    // Data is checked and good, fill in output
    UserVar[event->BaseVarIndex]     = data[0];
    UserVar[event->BaseVarIndex + 1] = data[1];
    UserVar[event->BaseVarIndex + 2] = data[2];
    UserVar[event->BaseVarIndex + 3] = data[3];
    UserVar[event->BaseVarIndex + 4] = data[4]/256 + (float(data[4]%256))/100;
    UserVar[event->BaseVarIndex + 5] = data[5]/256 + (float(data[5]%256))/100;
    Plugin_200_values_received = true;
    return true;
  }
  return false;
}

boolean Plugin_200(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_200;
        Device[deviceCount].Type = DEVICE_TYPE_SERIAL_PLUS1; //DEVICE_TYPE_TRIPLE //change to triple if you want to use soft serial port, otherwise default using HW serial
                                                              //also the init part below needs to changed, too
        Device[deviceCount].VType = SENSOR_TYPE_HEXA;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].FormulaOption = true;
        Device[deviceCount].ValueCount = 6;
        Device[deviceCount].SendDataOption = true;
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].GlobalSyncOption = true;
        success = true;
        break;
      }

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_200);
        success = true;
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_200));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_200));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_200));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_200));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[4], PSTR(PLUGIN_VALUENAME5_200));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[5], PSTR(PLUGIN_VALUENAME6_200));
        ExtraTaskSettings.TaskDeviceValueDecimals[0] = 0;
        ExtraTaskSettings.TaskDeviceValueDecimals[1] = 0;
        ExtraTaskSettings.TaskDeviceValueDecimals[2] = 0;
        ExtraTaskSettings.TaskDeviceValueDecimals[3] = 0;
        ExtraTaskSettings.TaskDeviceValueDecimals[4] = 2; //Temperature default 2 decimals
        ExtraTaskSettings.TaskDeviceValueDecimals[5] = 2; //Humidity default 2 decimals
        success = true;
        break;
      }

    case PLUGIN_GET_DEVICEGPIONAMES:
      {
        serialHelper_getGpioNames(event);
        event->String3 = formatGpioName_output(F("Reset"));
        break;
      }

    case PLUGIN_WEBFORM_SHOW_CONFIG:
      {
        string += serialHelper_getSerialTypeLabel(event);
        success = true;
        break;
      }

    case PLUGIN_WEBFORM_LOAD: {
      serialHelper_webformLoad(event);
      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE: {
      serialHelper_webformSave(event);
      success = true;
      break;
    }

    case PLUGIN_INIT:
      {
        int rxPin = CONFIG_PIN1;
        int txPin = CONFIG_PIN2;
        int resetPin = CONFIG_PIN3;

        String log = F("CUSTOM_SENSOR : config ");
        log += rxPin;
        log += txPin;
        log += resetPin;
        addLog(LOG_LEVEL_DEBUG, log);

        if (P200_easySerial != nullptr) {
          // Regardless the set pins, the software serial must be deleted.
          delete P200_easySerial;
          P200_easySerial = nullptr;
        }

        // Hardware serial is RX on 3 and TX on 1
        if (rxPin == 3 && txPin == 1)
        {
          log = F("CUSTOM_SENSOR : using hardware serial");
          addLog(LOG_LEVEL_INFO, log);
        }
        else
        {
          log = F("CUSTOM_SENSOR: using software serial");
          addLog(LOG_LEVEL_INFO, log);
        }
        P200_easySerial = new ESPeasySerial(rxPin, txPin, false, 96); // 96 Bytes buffer, enough for up to 3 packets.
        P200_easySerial->begin(9600);
        P200_easySerial->flush();

        if (resetPin >= 0) // Reset if pin is configured
        {
          // Toggle 'reset' to assure we start reading header
          log = F("CUSTOM_SENSOR: resetting module");
          addLog(LOG_LEVEL_INFO, log);
          pinMode(resetPin, OUTPUT);
          digitalWrite(resetPin, LOW);
          delay(250);
          digitalWrite(resetPin, HIGH);
          pinMode(resetPin, INPUT_PULLUP);
        }

        Plugin_200_init = true;
        success = true;
        break;
      }

    case PLUGIN_EXIT:
      {
          if (P200_easySerial)
          {
            delete P200_easySerial;
            P200_easySerial=nullptr;
          }
          break;
      }

    // The update rate from the module is 200ms .. multiple seconds. Practise
    // shows that we need to read the buffer many times per seconds to stay in
    // sync.
    case PLUGIN_TEN_PER_SECOND:
      {
        if (Plugin_200_init)
        {
          // Check if a complete packet is available in the UART FIFO.
          if (Plugin_200_PacketAvailable())
          {
            addLog(LOG_LEVEL_DEBUG_MORE, F("CUSTOM_SENSOR : Packet available"));
            success = Plugin_200_process_data(event);
          }
        }
        break;
      }
    case PLUGIN_READ:
      {
        // When new data is available, return true
        success = Plugin_200_values_received;
        Plugin_200_values_received = false;
        break;
      }
  }
  return success;
}
#endif // USES_P200
