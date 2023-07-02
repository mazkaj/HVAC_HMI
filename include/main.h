#include <Arduino.h>

#include <M5Core2.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <M5GFX.h>
#include <Ticker.h>

#include "myMiWiProtocol.h"
#include <graphics.h>
#include "netService.h"

#define DISP_BACK_COLOR     TFT_WHITE
#define DISP_TEXT_COLOR     TFT_BLACK

#define TIMER_WIFI_CHECK    1000      //ms
#define TIMER_PAN_UPDATE   800      //ms
#define TIMEOUT_TCP_ANSWER   100    //ms

#define TIMEFLAG_WAITFORMIDNIGHT    0
#define TIMEFLAG_REQUIREPANTIME     1
#define TIMEFLAG_PANTIMERECEIVED    2

#define WIFI_STATUS_FONT    &fonts::efontCN_16_b

#define OUT_PORT_NUMBER     27

#define LUX_INITIALIZE    0
#define LUX_SWITCH_ON    1
#define LUX_SWITCH_OFF   2   
#define LUX_BETWEEN_ON_OFF  3

void initButtons();
void vibrate();
void lightON();
void lightOFF();
void applyReceivedData();
void toggleOutputState();
void toggleLockState();
void toggleAutoState();
void doActionSwitchOnButtonPressed();
void doActionSwitchOffButtonPressed();
void manulaMode();
void autoMode();
void lockSwitch();
void unLockSwitch();
void drawOuputIcon();
void drawLockIcon();
void drawManualIcon();
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
uint32_t calculateLUX(uint8_t highByte, uint8_t lowByte);
int16_t temperatBMEBytesToInt(uint8_t highByte, uint8_t lowByte);
void refreshCurrentState();
void setup();
void loop();
