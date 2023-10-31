#include <Arduino.h>

#include <M5Core2.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <M5GFX.h>
#include <Ticker.h>

#include "myMiWiProtocol.h"
#include <graphics.h>
#include "netService.h"
#include "hvacHmi.h"

#define DISP_BACK_COLOR     TFT_WHITE
#define DISP_TEXT_COLOR     TFT_BLACK

#define USE_SDCARD 
#define USE_RTC

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

#define WIFI_STATUS_FONT    &fonts::efontCN_16_b

typedef struct
{
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
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
} currentState_t;

typedef struct
{
    uint8_t configuration;
    String nodeName;
} nodeConfig_t;

void initButtons();
void drawOffImageZone();
void drawMaxImageZone();
void vibrate();
void applyReceivedData();
void updateDisplayHvacData();
void drawFlexItFanIcon();
void drawCoolHeatIcon();
void displayHvacWiFiInfo();
void displayDacOutVoltage(int dispColor, uint16_t dacOutVoltage);
void displayReqTemperature();
void doActionSwitchOnButtonPressed();
void doActionSwitchOffButtonPressed();
void showStatusIcons();
void checkBatCondition();
void redrawWiFiIcon(uint8_t wifiIconType);
void showConnectingInfo();
void showNodeName();
void showGaPoData();
void hideGaPoData();
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
