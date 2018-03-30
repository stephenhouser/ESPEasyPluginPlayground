//#######################################################################################################
//#################################### Plugin 214: Maxbotix LV MaxSonar EZ Distance Sensors ##############################################
//#######################################################################################################
/*
 * Plugin to read distance from Maxbotix LV-MaxSonar-EZx series sensors in raw
 * pulse width, inches, and millimeters.
 * 
 * This plugin is designed to use the pulse width representation of range. This 
 * is Pin 2 on the sensor (LV-MaxSonar-EZ0). The PWM output of the LV-EZx
 * provides good resolution and is therefore used by this plugin rather than
 * the serial or analog methods. The serial data would need to be decoded and
 * thus add unnecessary complexity to the plugin. The resolution of the analog
 * output seems (via experimentation) to be less precise than the pulse width
 * method.
 * 
 * Pulse Width, from the data sheet:
 *    Pin 2-PW- This pin outputs a pulse width representation of range. 
 *    The distance can be calculated using the scale factor of 147uS per inch.
 * 
 * This means that:
 *    inches ~= measured_value / 147.0
 *      - OR -
 *    inches ~= measured_value * 0.0068027211
 * 
 *    millimeters ~= measured_value / 5.7874015764
 *      - OR - 
 *    millimeters ~= measured_value * 0.1727891156
 * 
 * Utility macros (below) are provided to convert the raw pulse width to
 * inches and millimeters using these conversion factors.
 * 
 * Currently this plugin "continually ranges" by leaving the sensor's RX pin
 * (pin 4) unconnected. A future version of this plugin may enable the
 * "command ranging" option to save power. 
 * 
 * Command Ranging, from the data sheet: 
 *    Pin 4-RX– This pin is internally pulled high. The LV-MaxSonar-EZ will
 *    continually measure range and output if RX data is left unconnected or 
 *    held high. If held low the sensor will stop ranging. Bring high for 
 *    20uS or more to command a range reading.
 * 
 * The Maxbotix sensors may partially work with the core plugin `_P103_HCSR04` - 
 * "Distance - HC-SR04" in the "continually ranging" setup, but the "command
 * ranging" timing is different, hence this plugin's existence.
 * 
 * Product details: https://www.maxbotix.com/Ultrasonic_Sensors.htm#LV-MaxSonar-EZ
 * Product data sheet: https://www.maxbotix.com/documents/LV-MaxSonar-EZ_Datasheet.pdf
 * 
 * Tested with 3.3V on NodeMCU compatible ESP8266.
 */

#ifdef PLUGIN_BUILD_DEV

#define PLUGIN_214
#define PLUGIN_ID_214        214
#define PLUGIN_NAME_214       "Distance - Maxbotix LV-EZx"
#define PLUGIN_VALUENAME1_214 "Distance (raw)"
#define PLUGIN_VALUENAME2_214 "Distance (inch)"
#define PLUGIN_VALUENAME3_214 "Distance (mm)"

// For Fast/Slow variant
#define PLUGIN_214_CONFIG_SPEED  0
// #define PLUGIN_214_SPEED_SLOW 1
// #define PLUGIN_214_SPEED_FAST 2

#define PLUGIN_214_CONFIG_SAMPLES 0
#define PLUGIN_214_CONFIG_DELAY   1

/* Convert pulse width representation of range to inches */
#define PLUGIN_214_INCH_CONVERSION  147.0
#define PLUGIN_214_RAW2INCH(value) (value / PLUGIN_214_INCH_CONVERSION);

/* Convert pulse width representation of range to millimeters */
#define PLUGIN_214_MM_CONVERSION 0.1727891156
#define PLUGIN_214_RAW2MM(value) (value * PLUGIN_214_MM_CONVERSION);

/*
 * EPSEasy Plugin
 */
boolean Plugin_214(byte function, struct EventStruct *event, String &string)
{
  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_214;
        Device[deviceCount].Type = DEVICE_TYPE_SINGLE;
        Device[deviceCount].VType = SENSOR_TYPE_SINGLE;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].FormulaOption = true;
        Device[deviceCount].ValueCount = 3;
        Device[deviceCount].SendDataOption = true;
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].GlobalSyncOption = true;
        return true;
      }

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_214);
        return true;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_214));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_214));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_214));
        return true;
      }

    case PLUGIN_INIT:
      {
        addLog(LOG_LEVEL_INFO, "LV-EZx: Init");

        int data_pin = Settings.TaskDevicePin1[event->TaskIndex];
        pinMode(data_pin, INPUT);
        return true;
      }

    case PLUGIN_READ:
      {
        boolean success = false;
        String log = F("LV-EZx: Distance: ");

        // For custom samples and delay between samples
        int data_pin = Settings.TaskDevicePin1[event->TaskIndex];
        int sample_count = Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_SAMPLES];
        int sample_delay = Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_DELAY];
        float value = Plugin_214_read_multiple(data_pin, sample_count, sample_delay);
        if (value > 0)
        {
          UserVar[event->BaseVarIndex + 0] = value;
          UserVar[event->BaseVarIndex + 1] = PLUGIN_214_RAW2INCH(value);
          UserVar[event->BaseVarIndex + 2] = PLUGIN_214_RAW2MM(value);
          log += UserVar[event->BaseVarIndex + 0];
          log += "us ";
          log += UserVar[event->BaseVarIndex + 1];
          log += "in ";
          log += UserVar[event->BaseVarIndex + 2];
          log += "mm";
          success = true;
        }
        else 
        {
          log += "No value";
        }

        addLog(LOG_LEVEL_INFO, log);
        return success;
      }

    // For Fast/Slow variant
    // case PLUGIN_TEN_PER_SECOND: // called 10 times per second for speedy response in polling scenario's
    // {
    //   if (Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_SPEED] == PLUGIN_214_SPEED_FAST)
    //   {
    //   }
    //   else {
    //     return false;
    //   }
    // }
    // case PLUGIN_ONCE_A_SECOND:  // called once per second for not so speedy stuff
    // {
    //   if (Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_SPEED] == PLUGIN_214_SPEED_SLOW) 
    //   {
    //   }
    //   else {
    //     return false;
    //   }
    // }

    case PLUGIN_WEBFORM_LOAD:
      {
        // For Fast/Slow variant
        // byte speed_setting = Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_SPEED];
        // String options[2];
        // options[0] = F("Slow");
        // options[1] = F("Fast");
        // int optionValues[2] = { PLUGIN_214_SPEED_SLOW, PLUGIN_214_SPEED_FAST };
        // addFormSelector(string, F("Speed"), F("plugin_013_speed"), 2, options, optionValues, choice);

        // For custom samples and delay between samples
        // Default values; 10 samples, 10us delay between samples
        if (Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_SAMPLES] <= 0)
        {
          Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_SAMPLES] = 10;
        }
        addFormNumericBox(string, F("Samples"), F("plugin_214_samples"), Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_SAMPLES]);

        if (Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_DELAY] <= 0)
        {
          Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_DELAY] = 10;
        }
        addFormNumericBox(string, F("Delay"), F("plugin_214_delay"), Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_DELAY]);
        
        return true;
      }

    case PLUGIN_WEBFORM_SAVE:
      {
        // For Fast/Slow variant
        // Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_SPEED] = getFormItemInt(F("plugin_013_speed"));

        // For custom samples and delay between samples
        Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_SAMPLES] = getFormItemInt(F("plugin_214_samples"));
        Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_DELAY] = getFormItemInt(F("plugin_214_delay"));
        return true;
      }
  }
  return false;
}

/*
 * Read raw pulse width of Maxbotix LV-EZx sensor on `data_pin`.
 * This call is the lowest level of the sensor data collection.
 * 
 * Return value is the width of the pulse received (which is proportinal to the distance)
 */
float Plugin_214_read(int data_pin) {
  return (float)pulseIn(data_pin, HIGH);
}

/*
 * Read Maxbotix LV-EZx sensor multiple times and average the result.
 * This attempts to mitigate wandering readings by averaging several samples
 * from the sensor.
 * 
 * `data_pin` is the pin the sensor's PWM data is connected to.
 * `sample_count` is the number of samples (readings) to take.
 * `sample_delay` is the delay between each sample.
 */
float Plugin_214_read_multiple(int data_pin, int sample_count, int sample_delay)
{
  float value = 0;
  for (int i = 0; i < sample_count; i++)
  {
    value += Plugin_214_read(data_pin);
    delay(sample_delay);
  }

  value /= sample_count;
  return value;
}

#endif