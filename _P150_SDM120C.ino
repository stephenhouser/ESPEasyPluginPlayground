//#######################################################################################################
//############################# Plugin 150: SDM120C Eastron Energy Meter ################################
//#######################################################################################################
/*
  Plugin written by: Sergio Faustino sjfaustino__AT__gmail.com

  This plugin reads available values of an Eastron SDM120C Energy Meter.
  It will also work with all the other superior model such as SDM220 AND SDM630 series.
*/

#ifdef PLUGIN_BUILD_DEV

#define PLUGIN_150
#define PLUGIN_ID_150         150
#define PLUGIN_NAME_150       "Energy (AC) - Eastron SDM120C"
#define PLUGIN_VALUENAME1_150 "Voltage"

boolean Plugin_150_init = false;

#include <SDM.h>                        // Requires SDM library from Reaper7 - https://github.com/reaper7/SDM_Energy_Meter/
SDM<2400, D6, D7> Plugin_150_SDM;

boolean Plugin_150(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {

    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_150;
        Device[deviceCount].Type = DEVICE_TYPE_DUAL;     // connected through 2 datapins
        Device[deviceCount].VType = SENSOR_TYPE_SINGLE;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].FormulaOption = true;
        Device[deviceCount].ValueCount = 1;
        Device[deviceCount].SendDataOption = true;
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].GlobalSyncOption = true;
        break;
      }

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_150);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_150));
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {
          byte meter_model = Settings.TaskDevicePluginConfig[event->TaskIndex][1];
          byte meter_baudrate = Settings.TaskDevicePluginConfig[event->TaskIndex][2];
          byte query = Settings.TaskDevicePluginConfig[event->TaskIndex][3];
          
          String options_model[3] = { F("SDM120C"), F("SDM220T"), F("SDM630") };
          addFormSelector(string, F("Model Type"), F("plugin_150_meter_model"), 3, options_model, NULL, meter_model );

          String options_baudrate[6] = { F("1200"), F("2400"), F("4800"), F("9600"), F("19200"), F("38400") };
          addFormSelector(string, F("Baud Rate"), F("plugin_150_meter_baudrate"), 6, options_baudrate, NULL, meter_baudrate );

        if (meter_model == 0 && meter_baudrate > 3)
          string += F("<span style=\"color:red\"> SDM120 only allows up to 9600 baud with default 2400!</span>");

        if (meter_model == 2 && meter_baudrate == 0)
          string += F("<span style=\"color:red\"> SDM630 only allows 2400 to 38400 baud with default 9600!</span>");

          String options_query[10] = { F("Voltage (V)"),
                                       F("Current (A)"),
                                       F("Power (W)"),
                                       F("Active Apparent Power (VA)"),
                                       F("Reactive Apparent Power (VAr)"),
                                       F("Power Factor (cfi?)"),
                                       F("Frequency (Hz)"),
                                       F("Import Active Energy (Wh)"),
                                       F("Export Active Energy (Wh)"),
                                       F("Total Active Energy (Wh)") };
          addFormSelector(string, F("Query"), F("plugin_150_query"), 10, options_query, NULL, query );

          success = true;
          break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {
          Settings.TaskDevicePluginConfig[event->TaskIndex][0] = getFormItemInt(F("plugin_150"));
          Settings.TaskDevicePluginConfig[event->TaskIndex][1] = getFormItemInt(F("plugin_150_meter_model"));
          Settings.TaskDevicePluginConfig[event->TaskIndex][2] = getFormItemInt(F("plugin_150_meter_baudrate"));
          Settings.TaskDevicePluginConfig[event->TaskIndex][3] = getFormItemInt(F("plugin_150_query"));

          Plugin_150_init = false; // Force device setup next time
          success = true;
          break;
      }

    case PLUGIN_INIT:
      {
        Plugin_150_init = true;

//        SDM<2400, Settings.TaskDevicePin1[event->TaskIndex],
//                  Settings.TaskDevicePin2[event->TaskIndex]> Plugin_150_SDM;
        Plugin_150_SDM.begin();
        success = true;
        break;
      }

    case PLUGIN_READ:
      {

        if (Plugin_150_init)
        {
          float _tempvar = 0;
          String log = F("EASTRON: ");
          switch(Settings.TaskDevicePluginConfig[event->TaskIndex][3])
          {
              case 0:
              {
                  _tempvar = Plugin_150_SDM.readVal(SDM120C_VOLTAGE);
                  log += F("Voltage ");
                  break;
              }
              case 1:
              {
                  _tempvar = Plugin_150_SDM.readVal(SDM120C_CURRENT);
                  log += F("Current ");
                  break;
              }
              case 2:
              {
                  _tempvar = Plugin_150_SDM.readVal(SDM120C_POWER);
                  log += F("Power ");
                  break;
              }
              case 3:
              {
                  _tempvar = Plugin_150_SDM.readVal(SDM120C_ACTIVE_APPARENT_POWER);
                  log += F("Active Apparent Power ");
                  break;
              }
              case 4:
              {
                  _tempvar = Plugin_150_SDM.readVal(SDM120C_REACTIVE_APPARENT_POWER);
                  log += F("Reactive Apparent Power ");
                  break;
              }
              case 5:
              {
                  _tempvar = Plugin_150_SDM.readVal(SDM120C_POWER_FACTOR);
                  log += F("Power Factor ");
                  break;
              }
              case 6:
              {
                  _tempvar = Plugin_150_SDM.readVal(SDM120C_FREQUENCY);
                  log += F("Frequency ");
                  break;
              }
              case 7:
              {
                  _tempvar = Plugin_150_SDM.readVal(SDM120C_IMPORT_ACTIVE_ENERGY);
                  log += F("Import Active Energy ");
                  break;
              }
              case 8:
              {
                  _tempvar = Plugin_150_SDM.readVal(SDM120C_EXPORT_ACTIVE_ENERGY);
                  log += F("Export Active Energy ");
                  break;
              }
              case 9:
              {
                  _tempvar = Plugin_150_SDM.readVal(SDM120C_TOTAL_ACTIVE_ENERGY);
                  log += F("Total Active Energy ");
                  break;
              }
          }
          
          UserVar[event->BaseVarIndex] = _tempvar;
          log += _tempvar;
          addLog(LOG_LEVEL_INFO, log);

          success = true;
          break;
        }
        break;
      }
  }
  return success;
}

#endif
