/*##########################################################################################
  ##################### Plugin 168: OLED SSD1306 display for Thermostat ####################
  ##########################################################################################

   This is a modification to Plugin_036 with graphics library provided from squix78 github
   https://github.com/squix78/esp8266-oled-ssd1306

  Features :
    - Displays and use current temperature from specified Device/Value (can be a Dummy for example)
    - Displays and maintains setpoint value
    - on power down/up this plugin maintains and reloads RELAY and SETPOINT values from SPIFFS
    - Supports 3 buttons, LEFT, RIGHT and MODE selection (MODE button cycles modes below, 
      LEFT/RIGHT increases-decreases setpoint OR timeout (Mode sensitive)
    - one output relay need to be specified, currently only HIGH level active supported
    - 3 mode is available: 
        - 0 or X: set relay permanently off no matter what
        - 1 or A: set relay ON if current temperature below setpoint, and set OFF when 
                  temperature+hysteresis reached - comparison made at setted Plugin interval (AUTO MODE)
        - 2 or M: set relay ON for specified time in minutes (MANUAL ON MODE), after timeout, mode switch to "A"
  
  List of commands :
  - oledframedcmd,[OLED_STATUS]               Inherited command from P036 status can be: 
                                              [off/on/low/med/high]
  - thermo,setpoint,[target_temperature]      Target setpoint, only used in Mode "A"
  - thermo,heating,[RELAY_STATUS]             Manually forcing relay status [off/on]
  - thermo,mode,[MODE],[TIMEOUT]              Set to either mode X/A/M, if M selected, 
                                              then TIMEOUT can be specified in minutes
  
  Command Examples :
  -  /control?cmd=thermo,setpoint,23          Set target setpoint to 23 Celsius
  -  /control?cmd=thermo,mode,1               Set mode to AUTOMATIC so it starts to maintain setpoint temperature
  -  /control?cmd=thermo,mode,2,5             Starts pre-heat for 5 minute, does not care about TEMP, then go to AUTO mode after timeout
  -  /control?cmd=thermo,mode,0               Switch heating off, absolutely do nothing until further notice

  ------------------------------------------------------------------------------------------
  Copyleft Nagy Sándor 2018 - https://bitekmindenhol.blog.hu/
  ------------------------------------------------------------------------------------------
*/

#define PLUGIN_168
#define PLUGIN_ID_168         168
#define PLUGIN_NAME_168       "Display - OLED SSD1306/SH1106 Thermo"
#define PLUGIN_VALUENAME1_168 "setpoint"
#define PLUGIN_VALUENAME2_168 "heating"
#define PLUGIN_VALUENAME3_168 "mode"
#define PLUGIN_VALUENAME4_168 "timeout"

#define P168_Nlines 1
#define P168_Nchars 32

#define P168_CONTRAST_OFF    1
#define P168_CONTRAST_LOW    64
#define P168_CONTRAST_MED  0xCF
#define P168_CONTRAST_HIGH 0xFF

#include "SSD1306.h"
#include "SH1106Wire.h"
#include "Dialog_Plain_12_font.h"
#include "Dialog_Plain_18_font.h"

const char flameimg[] PROGMEM = {
  0x00, 0x20, 0x00,
  0x00, 0x70, 0x00,
  0x00, 0x78, 0x00,
  0x00, 0x7c, 0x00,
  0x00, 0x7c, 0x00,
  0x80, 0x7f, 0x00,
  0xc0, 0xff, 0x00,
  0xc0, 0xff, 0x00,
  0xe0, 0xff, 0x04,
  0xe0, 0xff, 0x05,
  0xf0, 0xff, 0x0f,
  0xf0, 0xff, 0x0f,
  0xf8, 0xff, 0x0f,
  0xf8, 0xff, 0x1f,
  0xf8, 0xff, 0x1f,
  0xf8, 0xff, 0x3f,
  0xf8, 0xff, 0x3f,
  0xf8, 0xf3, 0x3f,
  0xf8, 0xf1, 0x3f,
  0xf8, 0xf1, 0x3f,
  0xf8, 0xe1, 0x3f,
  0xf0, 0x21, 0x3f,
  0xf0, 0x01, 0x1f,
  0xe0, 0x01, 0x0f,
  0xc0, 0x01, 0x0f,
  0x80, 0x01, 0x07,
  0x00, 0x03, 0x01
};

#define P168_WIFI_STATE_UNSET          -2
#define P168_WIFI_STATE_NOT_CONNECTED  -1

float Plugin_168_prev_temp = 99;
float Plugin_168_prev_setpoint;
float Plugin_168_prev_timeout;
byte Plugin_168_prev_heating;
byte Plugin_168_prev_mode;

byte Plugin_168_taskindex;
byte Plugin_168_varindex;
byte Plugin_168_changed;
boolean Plugin_168_init = false;
unsigned long Plugin_168_lastsavetime = 0;
byte Plugin_168_saveneeded = 0;

static unsigned long Plugin_168_buttons[3];

static int8_t P168_lastWiFiState = P168_WIFI_STATE_UNSET;

// Instantiate display here - does not work to do this within the INIT call

OLEDDisplay *P168_display = NULL;

char P168_deviceTemplate[P168_Nlines][P168_Nchars];

boolean Plugin_168(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {

    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_168;
        Device[deviceCount].Type = DEVICE_TYPE_I2C;
        Device[deviceCount].VType = SENSOR_TYPE_QUAD;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].FormulaOption = true;
        Device[deviceCount].ValueCount = 4;
        Device[deviceCount].SendDataOption = true;
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].TimerOptional = false;
        Device[deviceCount].GlobalSyncOption = true;
        break;
      }

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_168);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_168));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_168));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_168));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_168));
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {
        byte choice5 = Settings.TaskDevicePluginConfig[event->TaskIndex][2];
        String options5[2];
        options5[0] = F("SSD1306");
        options5[1] = F("SH1106");
        int optionValues5[2] = { 1, 2 };
        addFormSelector(F("Controller"), F("plugin_168_controler"), 2, options5, optionValues5, choice5);

        byte choice0 = Settings.TaskDevicePluginConfig[event->TaskIndex][0];
        int optionValues0[2];
        optionValues0[0] = 0x3C;
        optionValues0[1] = 0x3D;
        addFormSelectorI2C(F("plugin_168_adr"), 2, optionValues0, choice0);

        byte choice1 = Settings.TaskDevicePluginConfig[event->TaskIndex][1];
        String options1[2];
        options1[0] = F("Normal");
        options1[1] = F("Rotated");
        int optionValues1[2] = { 1, 2 };
        addFormSelector(F("Rotation"), F("plugin_168_rotate"), 2, options1, optionValues1, choice1);

        LoadCustomTaskSettings(event->TaskIndex, (byte*)&P168_deviceTemplate, sizeof(P168_deviceTemplate));
        for (byte varNr = 0; varNr < P168_Nlines; varNr++)
        {
          if (varNr == 0) {
            addFormTextBox(String(F("Temperature source ")), String(F("Plugin_168_template")) + (varNr + 1), P168_deviceTemplate[varNr], P168_Nchars);
          } else {
            addFormTextBox(String(F("Line ")) + (varNr + 1), String(F("Plugin_168_template")) + (varNr + 1), P168_deviceTemplate[varNr], P168_Nchars);
          }
        }

        addFormPinSelect(F("Button left"), F("taskdevicepin1"), Settings.TaskDevicePin1[event->TaskIndex]);
        addFormPinSelect(F("Button right"), F("taskdevicepin2"), Settings.TaskDevicePin2[event->TaskIndex]);
        addFormPinSelect(F("Button mode"), F("taskdevicepin3"), Settings.TaskDevicePin3[event->TaskIndex]);

        addFormPinSelect(F("Relay"), F("heatrelay"), Settings.TaskDevicePluginConfig[event->TaskIndex][4]);

        byte choice6 = Settings.TaskDevicePluginConfig[event->TaskIndex][3];
        if (choice6 == 0) choice6 = P168_CONTRAST_HIGH;
        String options6[3];
        options6[0] = F("Low");
        options6[1] = F("Medium");
        options6[2] = F("High");
        int optionValues6[3];
        optionValues6[0] = P168_CONTRAST_LOW;
        optionValues6[1] = P168_CONTRAST_MED;
        optionValues6[2] = P168_CONTRAST_HIGH;
        addFormSelector(F("Contrast"), F("plugin_168_contrast"), 3, options6, optionValues6, choice6);

        byte choice4 = (Settings.TaskDevicePluginConfigFloat[event->TaskIndex][0] * 10);
        String options4[3];
        options4[0] = F("0.2");
        options4[1] = F("0.5");
        options4[2] = F("1");
        int optionValues4[3];
        optionValues4[0] = 2;
        optionValues4[1] = 5;
        optionValues4[2] = 10;
        addFormSelector(F("Hysteresis"), F("plugin_168_hyst"), 3, options4, optionValues4, choice4);

        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {
        Settings.TaskDevicePluginConfig[event->TaskIndex][0] = getFormItemInt(F("plugin_168_adr"));
        Settings.TaskDevicePluginConfig[event->TaskIndex][1] = getFormItemInt(F("plugin_168_rotate"));
        Settings.TaskDevicePluginConfig[event->TaskIndex][2] = getFormItemInt(F("plugin_168_controler"));
        Settings.TaskDevicePluginConfig[event->TaskIndex][3] = getFormItemInt(F("plugin_168_contrast"));
        Settings.TaskDevicePluginConfig[event->TaskIndex][4] = getFormItemInt(F("heatrelay"));
        Settings.TaskDevicePluginConfigFloat[event->TaskIndex][0] = (getFormItemInt(F("plugin_168_hyst")) / 10.0);

        String argName;

        for (byte varNr = 0; varNr < P168_Nlines; varNr++)
        {
          argName = F("Plugin_168_template");
          argName += varNr + 1;
          strncpy(P168_deviceTemplate[varNr], WebServer.arg(argName).c_str(), sizeof(P168_deviceTemplate[varNr]));
        }

        SaveCustomTaskSettings(event->TaskIndex, (byte*)&P168_deviceTemplate, sizeof(P168_deviceTemplate));

        success = true;
        break;
      }

    case PLUGIN_INIT:
      {
        P168_lastWiFiState = P168_WIFI_STATE_UNSET;
        // Load the custom settings from flash
        LoadCustomTaskSettings(event->TaskIndex, (byte*)&P168_deviceTemplate, sizeof(P168_deviceTemplate));

        //      Init the display and turn it on
        if (P168_display)
        {
          P168_display->end();
          delete P168_display;
        }
        Plugin_168_taskindex = event->TaskIndex;
        Plugin_168_varindex = event->BaseVarIndex;

        uint8_t OLED_address = Settings.TaskDevicePluginConfig[event->TaskIndex][0];
        if (Settings.TaskDevicePluginConfig[event->TaskIndex][2] == 1) {
          P168_display = new SSD1306Wire(OLED_address, Settings.Pin_i2c_sda, Settings.Pin_i2c_scl);
        } else {
          P168_display = new SH1106Wire(OLED_address, Settings.Pin_i2c_sda, Settings.Pin_i2c_scl);
        }
        P168_display->init();		// call to local override of init function
        P168_display->displayOn();

        uint8_t OLED_contrast = Settings.TaskDevicePluginConfig[event->TaskIndex][3];
        P168_setContrast(OLED_contrast);

        String logstr = F("Thermo : Btn L:");
        logstr += Settings.TaskDevicePin1[event->TaskIndex];
        logstr += F("R:");
        logstr += Settings.TaskDevicePin2[event->TaskIndex];
        logstr += F("M:");
        logstr += Settings.TaskDevicePin3[event->TaskIndex];
        addLog(LOG_LEVEL_INFO, logstr);

        if (Settings.TaskDevicePin1[event->TaskIndex] != -1)
        {
          pinMode(Settings.TaskDevicePin1[event->TaskIndex], INPUT_PULLUP);
        }
        if (Settings.TaskDevicePin2[event->TaskIndex] != -1)
        {
          pinMode(Settings.TaskDevicePin2[event->TaskIndex], INPUT_PULLUP);
        }
        if (Settings.TaskDevicePin3[event->TaskIndex] != -1)
        {
          pinMode(Settings.TaskDevicePin3[event->TaskIndex], INPUT_PULLUP);
        }

        Plugin_168_prev_temp = 99;

        if (SPIFFS.exists("thermo.dat"))
        {
          fs::File f = SPIFFS.open("thermo.dat", "r");
          if (f)
          {
            f.read( ((uint8_t *)&UserVar[event->BaseVarIndex] + 0), 16 );
            f.close();
          }
        }
        Plugin_168_lastsavetime = millis();
        if (UserVar[event->BaseVarIndex] < 1) {
          UserVar[event->BaseVarIndex] = 19; // setpoint
          UserVar[event->BaseVarIndex + 2] = 1; // mode (X=0,A=1,M=2)
        }
        //UserVar[event->BaseVarIndex + 1] = 0; // heating (0=off,1=heating in progress)
        //UserVar[event->BaseVarIndex + 3] = 0; // timeout (manual on for minutes)
        if (Settings.TaskDevicePluginConfig[event->TaskIndex][4] != -1)
        {
          //pinMode(Settings.TaskDevicePluginConfig[event->TaskIndex][4], OUTPUT);
          P168_setHeatRelay(byte(UserVar[event->BaseVarIndex + 1]));          
        }
       
        logstr = F("Thermo : Starting status S:");
        logstr += String(UserVar[event->BaseVarIndex]);
        logstr += F(", R:");
        logstr += String(UserVar[event->BaseVarIndex + 1]);
        addLog(LOG_LEVEL_INFO, logstr);

        Plugin_168_changed = 1;
        Plugin_168_buttons[0] = 0; Plugin_168_buttons[1] = 0; Plugin_168_buttons[2] = 0;

        //      flip screen if required
        if (Settings.TaskDevicePluginConfig[event->TaskIndex][1] == 2) P168_display->flipScreenVertically();

        //      Display the device name, logo, time and wifi
        P168_display_header();
        P168_display_page();
        P168_display->display();
        Plugin_168_init = true;
        
        success = true;
        break;
      }

    case PLUGIN_EXIT:
      {
        if (P168_display)
        {
          P168_display->end();
          delete P168_display;
          P168_display = NULL;
        }
        break;
      }

    case PLUGIN_TEN_PER_SECOND:
      {
        unsigned long current_time;
       if (Plugin_168_init){
        if (Settings.TaskDevicePin1[event->TaskIndex] != -1)
        {
          if (!digitalRead(Settings.TaskDevicePin1[event->TaskIndex]))
          {
            current_time = millis();
            if (Plugin_168_buttons[0] + 300 < current_time) {
              Plugin_168_buttons[0] = current_time;
              switch (int(UserVar[event->BaseVarIndex + 2])) {
                case 0: { // off mode, no func
                    break;
                  }
                case 1: { // auto mode, setpoint dec
                    P168_setSetpoint(F("-0.5"));
                    break;
                  }
                case 2: { // manual on mode, timer dec
                    UserVar[event->BaseVarIndex + 3] = UserVar[event->BaseVarIndex + 3] - 300;
                    if (UserVar[event->BaseVarIndex + 3] < 0) {
                      UserVar[event->BaseVarIndex + 3] = 5400;
                    }
                    Plugin_168_prev_timeout = 32768;
                    Plugin_168_changed = 1;
                    break;
                  }
              }
            }
          }
        }
        if (Settings.TaskDevicePin2[event->TaskIndex] != -1)
        {
          if (!digitalRead(Settings.TaskDevicePin2[event->TaskIndex]))
          {
            current_time = millis();
            if (Plugin_168_buttons[1] + 300 < current_time) {
              Plugin_168_buttons[1] = current_time;
              switch (int(UserVar[event->BaseVarIndex + 2])) {
                case 0: { // off mode, no func
                    break;
                  }
                case 1: { // auto mode, setpoint inc
                    P168_setSetpoint(F("+0.5"));
                    break;
                  }
                case 2: { // manual on mode, timer dec
                    UserVar[event->BaseVarIndex + 3] = UserVar[event->BaseVarIndex + 3] + 300;
                    if (UserVar[event->BaseVarIndex + 3] > 5400) {
                      UserVar[event->BaseVarIndex + 3] = 60;
                    }
                    Plugin_168_prev_timeout = 32768;
                    Plugin_168_changed = 1;
                    break;
                  }
              }
            }
          }
        }

        if (Settings.TaskDevicePin3[event->TaskIndex] != -1)
        {
          if (!digitalRead(Settings.TaskDevicePin3[event->TaskIndex]))
          {
            current_time = millis();
            if (Plugin_168_buttons[2] + 300 < current_time) {
              Plugin_168_buttons[2] = current_time;

              switch (int(UserVar[event->BaseVarIndex + 2])) {
                case 0: { // off mode, next
                    P168_setMode(F("a"), F("0"));
                    break;
                  }
                case 1: { // auto mode, next
                    P168_setMode(F("m"), F("5"));
                    break;
                  }
                case 2: { // manual on mode, next
                    P168_setMode(F("x"), F("0"));
                    break;
                  }
              }
            }
          }
        }
       }
        break;
      }

    // Switch off display after displayTimer seconds
    case PLUGIN_ONCE_A_SECOND:
      {
       if (Plugin_168_init){
        if (P168_display && P168_display_wifibars()) {
          // WiFi symbol was updated.
          P168_display->display();
        }
        if (UserVar[event->BaseVarIndex + 2] == 2) { // manual timeout
          if (UserVar[event->BaseVarIndex + 3] > 0) {
            UserVar[event->BaseVarIndex + 3] = UserVar[event->BaseVarIndex + 3] - 1;
            P168_display_timeout();
          } else {
            UserVar[event->BaseVarIndex + 3] = 0;
            P168_setMode(F("a"), F("0")); //heater to auto
            P168_display_setpoint_temp(1);
          }
        }
        if (Plugin_168_changed == 1) {
          sendData(event);
          Plugin_168_saveneeded = 1;
          Plugin_168_changed = 0;
        }
        if (Plugin_168_saveneeded == 1) {
         if ((Plugin_168_lastsavetime+30000) < millis()) {
           Plugin_168_saveneeded = 0;
           Plugin_168_lastsavetime = millis();         
           fs::File f = SPIFFS.open("thermo.dat", "w");
           if (f)
           {
            f.write( ((uint8_t *)&UserVar[event->BaseVarIndex] + 0), 16 );
            f.close();
            flashCount();
           }
           String logstr = F("Thermo : Save UserVars to SPIFFS");
           addLog(LOG_LEVEL_INFO, logstr);
        }}
        success = true;
        }
        break;
      }

    case PLUGIN_READ:
      {
       if (Plugin_168_init){
        //      Update display
        P168_display_header();

        if (UserVar[event->BaseVarIndex + 2] == 1) {
          String atempstr2 = P168_deviceTemplate[0];
          String atempstr = parseTemplate(atempstr2, 20);
          atempstr.trim();
          if ((atempstr.length() > 0) && (Plugin_168_prev_temp != 99)) { // do not switch until the first temperature data arrives
            float atemp = atempstr.toFloat();
            if (atemp != 0.0) {
              if ((UserVar[event->BaseVarIndex] > atemp) && (UserVar[event->BaseVarIndex + 1] < 1))
              {
                P168_setHeater(F("1"));
                Plugin_168_changed = 1;
              } else if (((((float)atemp - (float)Settings.TaskDevicePluginConfigFloat[event->TaskIndex][0]) >= (float)UserVar[event->BaseVarIndex])) && (UserVar[event->BaseVarIndex + 1] > 0)) {
                P168_setHeater(F("0"));
                Plugin_168_changed = 1;
              } else {
                P168_display_heat();
              }
            }
          } else {
            P168_display_heat();
          }
        }

        P168_display_current_temp();
        P168_display_timeout();

        P168_display->display();

        success = true;
       }
        break;
      }

    case PLUGIN_WRITE:
      {
        String command = parseString(string, 1);
        String subcommand = parseString(string, 2);
        String logstr = "";
       if (Plugin_168_init){        
        if (command == F("oledframedcmd"))
        {
          success = true;
          if (subcommand == F("off"))
            P168_setContrast(P168_CONTRAST_OFF);
          else if (subcommand == F("on"))
            P168_display->displayOn();
          else if (subcommand == F("low"))
            P168_setContrast(P168_CONTRAST_LOW);
          else if (subcommand == F("med"))
            P168_setContrast(P168_CONTRAST_MED);
          else if (subcommand == F("high"))
            P168_setContrast(P168_CONTRAST_HIGH);
          logstr = F("\nOk");
          SendStatus(event->Source, logstr);
        }

        if (command == F("thermo"))
        {
          success = true;
          String par1 = parseString(string, 3);
          if (subcommand == F("setpoint"))
            P168_setSetpoint(par1);
          else if (subcommand == F("heating")) {
            P168_setHeater(par1); Plugin_168_changed = 1;
          }
          else if (subcommand == F("mode"))
            P168_setMode(par1, parseString(string, 4));
          logstr = F("\nOk");
          SendStatus(event->Source, logstr);
        }}
        break;
      }

  }
  return success;
}

// Set the display contrast
// really low brightness & contrast: contrast = 10, precharge = 5, comdetect = 0
// normal brightness & contrast:  contrast = 100
void P168_setContrast(uint8_t OLED_contrast) {
  char contrast = 100;
  char precharge = 241;
  char comdetect = 64;
  switch (OLED_contrast) {
    case P168_CONTRAST_OFF:
      if (P168_display) {
        P168_display->displayOff();
      }
      return;
    case P168_CONTRAST_LOW:
      contrast = 10; precharge = 5; comdetect = 0;
      break;
    case P168_CONTRAST_MED:
      contrast = P168_CONTRAST_MED; precharge = 0x1F; comdetect = 64;
      break;
    case P168_CONTRAST_HIGH:
    default:
      contrast = P168_CONTRAST_HIGH; precharge = 241; comdetect = 64;
      break;
  }
  if (P168_display) {
    P168_display->displayOn();
    P168_display->setContrast(contrast, precharge, comdetect);
  }
}

void P168_display_header() {
  static boolean showWiFiName = true;
  if (showWiFiName && (wifiStatus == ESPEASY_WIFI_SERVICES_INITIALIZED) ) {
    String newString = WiFi.SSID();
    newString.trim();
    P168_display_title(newString);
  } else {
    String dtime = "%sysname%";
    String newString = parseTemplate(dtime, 10);
    newString.trim();
    P168_display_title(newString);
  }
  showWiFiName = !showWiFiName;
  // Display time and wifibars both clear area below, so paint them after the title.
  P168_display_time();
  P168_display_wifibars();
}

void P168_display_time() {
  String dtime = "%systime%";
  String newString = parseTemplate(dtime, 10);
  P168_display->setTextAlignment(TEXT_ALIGN_LEFT);
  P168_display->setFont(Dialog_plain_12);
  P168_display->setColor(BLACK);
  P168_display->fillRect(0, 0, 28, 13);
  P168_display->setColor(WHITE);
  P168_display->drawString(0, 0, newString.substring(0, 5));
}

void P168_display_title(String& title) {
  P168_display->setTextAlignment(TEXT_ALIGN_CENTER);
  P168_display->setFont(Dialog_plain_12);
  P168_display->setColor(BLACK);
  P168_display->fillRect(0, 0, 128, 15); // Underscores use a extra lines, clear also.
  P168_display->setColor(WHITE);
  P168_display->drawString(64, 0, title);
}

//Draw Signal Strength Bars, return true when there was an update.
bool P168_display_wifibars() {
  const bool connected = wifiStatus == ESPEASY_WIFI_SERVICES_INITIALIZED;
  const int nbars_filled = (WiFi.RSSI() + 100) / 8;
  const int newState = connected ? nbars_filled : P168_WIFI_STATE_UNSET;
  if (newState == P168_lastWiFiState)
    return false; // nothing to do.

  int x = 105;
  int y = 0;
  int size_x = 15;
  int size_y = 10;
  int nbars = 5;
  int16_t width = (size_x / nbars);
  size_x = width * nbars - 1; // Correct for round errors.

  //  x,y are the x,y locations
  //  sizex,sizey are the sizes (should be a multiple of the number of bars)
  //  nbars is the number of bars and nbars_filled is the number of filled bars.

  //  We leave a 1 pixel gap between bars
  P168_display->setColor(BLACK);
  P168_display->fillRect(x , y, size_x, size_y);
  P168_display->setColor(WHITE);
  if (wifiStatus == ESPEASY_WIFI_SERVICES_INITIALIZED) {
    for (byte ibar = 0; ibar < nbars; ibar++) {
      int16_t height = size_y * (ibar + 1) / nbars;
      int16_t xpos = x + ibar * width;
      int16_t ypos = y + size_y - height;
      if (ibar <= nbars_filled) {
        // Fill complete bar
        P168_display->fillRect(xpos, ypos, width - 1, height);
      } else {
        // Only draw top and bottom.
        P168_display->fillRect(xpos, ypos, width - 1, 1);
        P168_display->fillRect(xpos, y + size_y - 1, width - 1, 1);
      }
    }
  } else {
    // Draw a not connected sign.
  }
  return true;
}

void P168_display_current_temp() {
  String tmpString = P168_deviceTemplate[0];
  String atempstr = parseTemplate(tmpString, 20);
  atempstr.trim();
  if (atempstr.length() > 0) {
    float atemp = atempstr.toFloat();
    atemp = (round(atemp * 10)) / 10.0;
    if (Plugin_168_prev_temp != atemp) {
      P168_display->setColor(BLACK);
      P168_display->fillRect(3, 19, 47, 25);
      P168_display->setColor(WHITE);
      tmpString = toString(atemp, 1);
      P168_display->setFont(ArialMT_Plain_24);
      P168_display->drawString(3, 19, tmpString.substring(0, 5));
      Plugin_168_prev_temp = atemp;
    }
  }
}

void P168_display_setpoint_temp(byte force) {
  if (UserVar[Plugin_168_varindex + 2] == 1) {
    float stemp = (round(UserVar[Plugin_168_varindex] * 10)) / 10.0;
    if ((Plugin_168_prev_setpoint != stemp) || (force == 1)) {
      P168_display->setColor(BLACK);
      P168_display->fillRect(86, 35, 41, 21);
      P168_display->setColor(WHITE);
      String tmpString = toString(stemp, 1);
      P168_display->setFont(Dialog_plain_18);
      P168_display->drawString(86, 35, tmpString.substring(0, 5));
      Plugin_168_prev_setpoint = stemp;
      Plugin_168_changed = 1;
    }
  }
}

void P168_display_timeout() {

  if (UserVar[Plugin_168_varindex + 2] == 2) {
    if (Plugin_168_prev_timeout >= (UserVar[Plugin_168_varindex + 3] + 60)) {
      float timeinmin = UserVar[Plugin_168_varindex + 3] / 60;
      String thour = toString(((int)(timeinmin / 60) ), 0);
      thour += F(":");
      String thour2 = toString(((int)timeinmin % 60), 0);
      if (thour2.length() < 2) {
        thour += "0" + thour2;
      } else {
        thour += thour2;
      }
      P168_display->setColor(BLACK);
      P168_display->fillRect(86, 35, 41, 21);
      P168_display->setColor(WHITE);
      P168_display->setFont(Dialog_plain_18);
      P168_display->drawString(86, 35, thour.substring(0, 5));
      Plugin_168_prev_timeout = UserVar[Plugin_168_varindex + 3];
    }
  }
}

void P168_display_mode() {
  if (Plugin_168_prev_mode != UserVar[Plugin_168_varindex + 2]) {
    String tmpString = F("");
    switch (int(UserVar[Plugin_168_varindex + 2]))
    {
      case 0:
        {
          tmpString = F("X");
          break;
        }
      case 1:
        {
          tmpString = F("A");
          break;
        }
      case 2:
        {
          tmpString = F("M");
          break;
        }
    }
    P168_display->setColor(BLACK);
    P168_display->fillRect(61, 49, 12, 17);
    P168_display->setColor(WHITE);
    P168_display->setFont(ArialMT_Plain_16);
    P168_display->drawString(61, 49, tmpString.substring(0, 5));
    Plugin_168_prev_mode = UserVar[Plugin_168_varindex + 2];
  }
}

void P168_display_heat() {

  if (Plugin_168_prev_heating != UserVar[Plugin_168_varindex + 1]) {
    P168_display->setColor(BLACK);
    P168_display->fillRect(54, 19, 24, 27);
    P168_display->setColor(WHITE);
    if (UserVar[Plugin_168_varindex + 1] == 1) {
      P168_display->drawXbm(54, 19, 24, 27, flameimg);
    }
    Plugin_168_prev_heating = UserVar[Plugin_168_varindex + 1];
  }

}

void P168_display_page() {
  // init with full clear
  P168_display->setColor(BLACK);
  P168_display->fillRect(0, 15, 128, 49);
  P168_display->setColor(WHITE);

  Plugin_168_prev_temp = 99;
  Plugin_168_prev_setpoint = 0;
  Plugin_168_prev_heating = 255;
  Plugin_168_prev_mode = 255;
  Plugin_168_prev_timeout = 32768;

  P168_display->setFont(Dialog_plain_12);
  P168_display->setTextAlignment(TEXT_ALIGN_LEFT);
  String tstr = "{D}C";
  String newString = parseTemplate(tstr, 10);
  P168_display->drawString(18, 46, newString.substring(0, 5));

  P168_display_heat();

  P168_display->drawHorizontalLine(0, 15, 128);
  P168_display->drawVerticalLine(52, 14, 49);

  P168_display->drawCircle(107, 47, 26);
  P168_display->drawHorizontalLine(78, 47, 8);
  P168_display->drawVerticalLine(107, 19, 10);

  P168_display_mode();
  P168_display_setpoint_temp(0);
  P168_display_current_temp();

}

void P168_setSetpoint(String sptemp) {
  float stemp = (round(UserVar[Plugin_168_varindex] * 10)) / 10.0;
  if ((sptemp.charAt(0) == '+') || (sptemp.charAt(0) == 'p'))  {
    stemp = stemp + sptemp.substring(1).toFloat();
  } else if ((sptemp.charAt(0) == '-') || (sptemp.charAt(0) == 'm')) {
    stemp = stemp - sptemp.substring(1).toFloat();
  } else {
    stemp = sptemp.toFloat();
  }
  UserVar[Plugin_168_varindex] = stemp;
  P168_display_setpoint_temp(0);
}

void P168_setHeatRelay(byte state) {
  uint8_t relaypin = Settings.TaskDevicePluginConfig[Plugin_168_taskindex][4];
  String logstr = F("Thermo : Set Relay");
  logstr += relaypin;
  logstr += F("=");
  logstr += state;
  addLog(LOG_LEVEL_INFO, logstr);
  if (relaypin != -1) {
    pinMode(relaypin, OUTPUT);
    digitalWrite(relaypin, state);
  }
}

void P168_setHeater(String heater) {
  if ((heater == "1") || (heater == "on")) {
    UserVar[Plugin_168_varindex + 1] = 1;
    P168_setHeatRelay(HIGH);
  } else if ((heater == "0") || (heater == "off")) {
    UserVar[Plugin_168_varindex + 1] = 0;
    P168_setHeatRelay(LOW);
  } else if (UserVar[Plugin_168_varindex + 1] == 0) {
    UserVar[Plugin_168_varindex + 1] = 1;
    P168_setHeatRelay(HIGH);
  } else {
    UserVar[Plugin_168_varindex + 1] = 0;
    P168_setHeatRelay(LOW);
  }
  P168_display_heat();
}

void P168_setMode(String amode, String atimeout) {
  if ((amode == "0") || (amode == "x")) {
    UserVar[Plugin_168_varindex + 2] = 0;
    P168_setHeater(F("0"));
    P168_display->setColor(BLACK);
    P168_display->fillRect(86, 35, 41, 21);
    Plugin_168_prev_setpoint = 0;
  } else if ((amode == "1") || (amode == "a")) {
    UserVar[Plugin_168_varindex + 2] = 1;
    P168_display_setpoint_temp(1);
  } else if ((amode == "2") || (amode == "m")) {
    UserVar[Plugin_168_varindex + 2] = 2;
    UserVar[Plugin_168_varindex + 3] = (atimeout.toFloat() * 60);
    Plugin_168_prev_timeout = 32768;
    P168_display_timeout();
    P168_setHeater(F("1"));
  } else {
    UserVar[Plugin_168_varindex + 2] = 0;
  }
  Plugin_168_changed = 1;
  P168_display_mode();

}
