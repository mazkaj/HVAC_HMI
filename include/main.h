#pragma once
#include <Arduino.h>

//#include <M5Core2.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <Ticker.h>
#include <M5Unified.h>
#include <M5GFX.h>

#include "myMiWiProtocol.h"
#include <graphics.h>
#include "myMiWiProtocol.h"
#include "netService.h"

#define DISP_BACK_COLOR     TFT_WHITE
#define DISP_TEXT_COLOR     TFT_BLACK

#define USE_SDCARD 
#define USE_RTC
#define USE_M5

#define TIMER_WIFI_CHECK    1000      //ms
#define TIMER_HVAC_UPDATE   500      //ms
#define TIMER_GET_TEMP      1000      //ms
#define TIMEOUT_TCP_ANSWER   100    //ms
#define TIMER_UPDATE_PAN    5000    //ms

#define TIMEFLAG_WAITFORMIDNIGHT    0
#define TIMEFLAG_REQUIREPANTIME     1
#define TIMEFLAG_PANTIMERECEIVED    2

#define DATAHVAC_VALID             0
#define DATAHVAC_TCPREQ            1

#define CONFBIT_ROOFLIGHT   1

#define WIFI_STATUS_FONT    &fonts::efontCN_16_b

#define UARTHMI_IDLE          0
#define UARTHMI_WAITING_ACK   1
#define UARTHMI_NOANSWER      2
#define UARTHMI_DATARECEIVED  3

enum luxLightState_t{
    LUX_INITIALIZE = 0,  
    LUX_SWITCH_ON = 0x01,   
    LUX_SWITCH_OFF = 0x02,
    LUX_BETWEEN_ON_OFF = 0x04,
    LUX_NOGAPODATA = 0x08  
};

using namespace m5;
typedef struct
{
    rtc_time_t time;
    rtc_date_t date;
    uint8_t getTimeFlag = TIMEFLAG_WAITFORMIDNIGHT;
    uint8_t validDataHVAC = DATAHVAC_VALID;
    uint16_t dacOutVoltage;
    float roomTemperature;
    float roomHumidity;
    uint8_t reqTemperature = 24;
    uint8_t ioState;
    String hvacWiFiSSID;
    uint8_t hvacipAddress;
    uint8_t hvacTcpIndexInConnTable;
    uint32_t idSHT;
    uint8_t fxFanSpeed;
    uint8_t fxDataState;
    uint16_t fxOutdoorTemperaure;
    uint16_t fxSupplyTemperature;
    uint16_t fxForcedVentilationTime;
    uint8_t fxForcedVentilationSpeed;
    uint8_t fxRegulationFanSpeed;
    bool fxForcedVentilation;
    uint32_t fxCurrentTime;
    uint16_t fxForcedVentLeftSeconds;
    bool fxReplaceFilterAlarm;
    bool fxHeatingBatteryActive;
    uint16_t gapoLuxValue;
    uint8_t roofLightState;
    bool isGaPoValid = 0;
    bool autoMode = false;
    uint8_t uartHMIState;
} currentState_t;

typedef struct
{
    uint8_t configuration;
    uint16_t tresholdOn = 0;
    uint16_t tresholdOff = 0;
    uint8_t hourOn = 0; //as 10 minutes from midnight ie: 6:00 = 6x60 = 360 / 10 = 36
    uint8_t hourOff = 0;
String nodeName;
} nodeConfig_t;

void setFlagConfig(uint8_t flagToClear);
void clearFlagConfig(uint8_t flagToClear);
bool isFlagConfig(uint8_t flagToCheck);
void initButtons();
void drawOffImageZone();
void drawMaxImageZone();
void drawRoofLightZone();
void vibrate();
void applyReceivedData();
void updateDisplayHvacData();
void drawFlexItFanIcon();
void drawCoolHeatIcon();
void drawAwayFireFilterAlarmIcon();
void displayHvacWiFiInfo();
void displayDacOutVoltage(int dispColor, uint16_t dacOutVoltage);
void displayReqTemperature();
void showRoofLightState();
void doActionSwitchOnButtonPressed();
void doActionSwitchOffButtonPressed();
void showStatusIcons();
void checkBatCondition();
void redrawWiFiIcon(uint8_t wifiIconType);
void showConnectingInfo();
void showNodeName();
void showWiFiStrength(int8_t lastRSSIValue);
void updateWiFiState();
void showIpAddressAndSSID(wifiState_t wifiState);
void showTcpConnectionState(wifiState_t wifiState);
void showWiFiState(wifiState_t wifiState);
void setFlagCurrentState(uint8_t flagToSet);
void clearFlagCurrentState(uint8_t flagToClear);
bool isFlagCurrentState(uint8_t flagToCheck);
int16_t temperatBMEBytesToInt(uint8_t highByte, uint8_t lowByte);
void reqHVACCurrentState();
void setReqTemperature(uint8_t reqTemperature);
void switchAutoManMode();
void reDrawImageButtons();
void refreshCurrentState();
void setup();
void loop();
