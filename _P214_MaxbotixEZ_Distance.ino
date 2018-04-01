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
 * There tends to be some varaition in readings so the plugin provides for
 * taking multple readings for each sample, with a delay between individual
 * readings. The plugin will return the average, minimum, and maximum of these 
 * readings as it's result. The dafault is to take 10 readings with a 10us
 * delay between each. This has given me reasonable results. You might need
 * to adjust based on your sensor and environment.
 * 
 * If you get large variability, you might also add a capacitor across Vcc and
 * GND as described in Maxbotic's troubleshooting tips (see link below).
 *  
 * The Maxbotix sensors may partially work with the core plugin `_P103_HCSR04` - 
 * "Distance - HC-SR04" in the "continually ranging" setup, but the "command
 * ranging" timing is different, hence this plugin's existence.
 * 
 * Product details: https://www.maxbotix.com/Ultrasonic_Sensors.htm#LV-MaxSonar-EZ
 * Product data sheet: https://www.maxbotix.com/documents/LV-MaxSonar-EZ_Datasheet.pdf
 * 
 * Tested with 3.3V and Vcc on NodeMCU compatible ESP8266. I get about the same
 * results from both power sources.
 */

#ifdef PLUGIN_BUILD_DEV

#define PLUGIN_214
#define PLUGIN_ID_214        214
#define PLUGIN_NAME_214       "Distance - Maxbotix LV-EZx"
#define PLUGIN_VALUENAME1_214 "Distance"
#define PLUGIN_VALUENAME2_214 "Distance (min)"
#define PLUGIN_VALUENAME3_214 "Distance (max)"

// For Fast/Slow variant
#define PLUGIN_214_CONFIG_SPEED  0
// #define PLUGIN_214_SPEED_SLOW 1
// #define PLUGIN_214_SPEED_FAST 2

#define PLUGIN_214_CONFIG_UNITS 0
#define PLUGIN_214_UNITS_RAW    0
#define PLUGIN_214_UNITS_INCH   1
#define PLUGIN_214_UNITS_MM     2

#define PLUGIN_214_CONFIG_SAMPLES 1
#define PLUGIN_214_CONFIG_DELAY   2

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
        String log = F("LV-EZx: Distance: ");
        boolean success = Plugin_214_read_multiple(event);
        if (success) {
          log += UserVar[event->BaseVarIndex + 0];
          log += " min=";
          log += UserVar[event->BaseVarIndex + 1];
          log += " max=";
          log += UserVar[event->BaseVarIndex + 2];
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

        // Reporting units (raw pulse width, inches, or mm)
        // Default value is raw
        byte units_setting = Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_UNITS];
        if (units_setting <= 0)
        {
          Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_UNITS] = PLUGIN_214_UNITS_RAW;
        }

        String options[3];
        options[0] = F("raw");
        options[1] = F("inches");
        options[2] = F("mm");
        int optionValues[3] = { PLUGIN_214_UNITS_RAW, PLUGIN_214_UNITS_INCH, PLUGIN_214_UNITS_MM };
        addFormSelector(string, F("Units"), F("plugin_214_units"), 3, options, optionValues, units_setting);

        // Number of samples to collect for each reading
        // Default is 10 samples per reading
        if (Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_SAMPLES] <= 0)
        {
          Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_SAMPLES] = 10;
        }

        addFormNumericBox(string, F("Samples"), F("plugin_214_samples"), Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_SAMPLES]);

        // Delay between samples
        // Default is 10us
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
        Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_UNITS] = getFormItemInt(F("plugin_214_units"));
        Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_SAMPLES] = getFormItemInt(F("plugin_214_samples"));
        Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_DELAY] = getFormItemInt(F("plugin_214_delay"));
        return true;
      }
  }
  return false;
}

/*
 * Read Maxbotix LV-EZx sensor multiple times and average the result.
 * This attempts to mitigate wandering readings by averaging several samples
 * from the sensor.
 * 
 * Data is returned directly into the plugin's three value fields.
 * BaseVarIndex + 0 = the averaged pulse width reading
 * BaseVarIndex + 1 = the minimum pulse width reading
 * BaseVarIndex + 2 = the maximum pulse width reading
 * 
 * `data_pin` is the pin the sensor's PWM data is connected to.
 * `sample_count` is the number of samples (readings) to take.
 * `sample_delay` is the delay between each sample.
 */
boolean Plugin_214_read_multiple(struct EventStruct *event)
{
  int pulse = 0;
  int pulse_sum = -1;
  int pulse_min = -1;
  int pulse_max = -1;

  int data_pin = Settings.TaskDevicePin1[event->TaskIndex];
  int sample_count = Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_SAMPLES];
  int sample_delay = Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_DELAY];

  // sample_count must be > 0 to prevent divide by zero
  // sample_delay must be >= 0 to prevent negative delay
  if (sample_count <= 0 || sample_delay <= 0) {
    addLog(LOG_LEVEL_INFO, "LV-EZ: Samples and Sample Delay must be non-negative.");
    return false;
  }

  for (int i = 0; i < sample_count; i++)
  {
    // This is where the action happens. Read a pulse width
    pulse = pulseIn(data_pin, HIGH);

    // accumulate pulse widths to compute final value
    pulse_sum += pulse;

    // Compute min and max readings for this run
    if (pulse_min < 0 || pulse < pulse_min) {
      pulse_min = pulse;
    }
    if (pulse_max < 0 || pulse > pulse_max) {
      pulse_max = pulse;
    }

    // Delay, if we are delaying between readings.
    if (sample_delay > 0) {
      delay(sample_delay);
    }
  }

  // Return success only if there is a (positive) sum of pulses to average
  if (pulse_sum >= 0)
  {
    // Compute average pulse width reading
    float value = (float)pulse_sum / (float)sample_count;

    // convert values based on units selection
    // save values to the plugin result fields
    switch (Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_UNITS]) 
    {
      case PLUGIN_214_UNITS_INCH:
        UserVar[event->BaseVarIndex + 0] = PLUGIN_214_RAW2INCH(value);
        UserVar[event->BaseVarIndex + 1] = PLUGIN_214_RAW2INCH((float)pulse_min);
        UserVar[event->BaseVarIndex + 2] = PLUGIN_214_RAW2INCH((float)pulse_max);
        break;

      case PLUGIN_214_UNITS_MM:
        UserVar[event->BaseVarIndex + 0] = PLUGIN_214_RAW2MM(value);
        UserVar[event->BaseVarIndex + 1] = PLUGIN_214_RAW2MM((float)pulse_min);
        UserVar[event->BaseVarIndex + 2] = PLUGIN_214_RAW2MM((float)pulse_max);
        break;

      case PLUGIN_214_UNITS_RAW:
      default:
        UserVar[event->BaseVarIndex + 0] = (float)value;
        UserVar[event->BaseVarIndex + 1] = (float)pulse_min;
        UserVar[event->BaseVarIndex + 2] = (float)pulse_max;
        break;
    }

    return true;
  }

  return false;
}

#endif