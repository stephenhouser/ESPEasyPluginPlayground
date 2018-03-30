//#######################################################################################################
//#################################### Plugin 214: Maxbotix LV MaxSonar EZ Distance Sensors ##############################################
//#######################################################################################################

#ifdef PLUGIN_BUILD_STEPHEN

#define PLUGIN_214
#define PLUGIN_ID_214        214
#define PLUGIN_NAME_214       "Maxbotix LV-EZx - Distance"
#define PLUGIN_VALUENAME1_214 "Distance (raw)"
#define PLUGIN_VALUENAME2_214 "Distance (inch)"
#define PLUGIN_VALUENAME3_214 "Distance (mm)"

// For Fast/Slow variant
#define PLUGIN_214_CONFIG_SPEED  0
// #define PLUGIN_214_SPEED_SLOW 1
// #define PLUGIN_214_SPEED_FAST 2

#define PLUGIN_214_CONFIG_SAMPLES 0
#define PLUGIN_214_CONFIG_DELAY   1

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
        addLog(LOG_LEVEL_INFO, "Init: LV-EZx");

        int data_pin = Settings.TaskDevicePin1[event->TaskIndex];
        pinMode(data_pin, INPUT);
        return true;
      }

    case PLUGIN_READ:
      {
        boolean success = false;
        String log = F("LV-EZx : Distance: ");

        // For custom samples and delay between samples
        int data_pin = Settings.TaskDevicePin1[event->TaskIndex];
        int sample_count = Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_SAMPLES];
        int sample_delay = Settings.TaskDevicePluginConfig[event->TaskIndex][PLUGIN_214_CONFIG_DELAY];
        float value = Plugin_214_read_multiple(data_pin, sample_count, sample_delay);
        if (value > 0)
        {
          UserVar[event->BaseVarIndex + 0] = value;
          UserVar[event->BaseVarIndex + 1] = Plugin_214_raw2inch(value);
          UserVar[event->BaseVarIndex + 2] = Plugin_214_raw2mm(value);
          log += UserVar[event->BaseVarIndex];
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

float Plugin_214_read(int data_pin) {
  return (float)pulseIn(data_pin, HIGH);
}

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

float Plugin_214_raw2inch(float value) {
  return value / 147.0;
}

float Plugin_214_raw2mm(float value) {
  return value * 0.1727891156; // result in mm 
}

#endif