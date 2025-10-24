#include <main.h>
#include <netService.h>
#include <sdService.h>
#include "SerialDataM5.h"
#include "hvacHmi.h"
#include "flexit.h"

extern currentState_t _currentState;
extern netNodeParameter_t _netNodeParam;
extern nodeConfig_t _nodeConfig;
uint8_t _configuration;
using namespace m5;
M5GFX gfx;
Ticker _reqHVACCurrentStateTicker;
Ticker _getCurrentTempTicker;
Ticker _hmiDimmerTicker;
Ticker _tcpPANUpdateTicker;

uint8_t _lastWiFiIconType = 0;
uint8_t _lastBatLevel = 0;
uint8_t _lastConnectionState = WIFI_IDLE;
uint8_t _getCurrentTimeFlag = TIMEFLAG_WAITFORMIDNIGHT;
uint8_t  _getCurrentHvacStateFlag = 0;
uint8_t _getCurrentTempFlag = 0;
uint8_t _updatePANFlag = 0;
uint8_t _hmiDimmValue = 100;
uint8_t _setHmiDimm = 1;
int16_t _gapoTempValue;
currentState_t _lastCurrentState;
bool _reDrawImageButtons = false;

// Zone offImageZone = Zone(240,120,64,64);
// Zone maxImageZone = Zone(10,120,64,64);
// Zone lightImageZone = Zone(244,120,64,64);

LGFX_Button intensityIncTButton;
LGFX_Button intensityDecTButton;
LGFX_Button setRoofLightTButton;
LGFX_Button setOffPowerTButton;
LGFX_Button switchHeatCoolTButton;
LGFX_Button manAutoTButton;
LGFX_Button fxFanSpeed;

// Button intensityDecTButton(160, 120, 75, 75, false, "-", {LIGHTGREY, BLACK, BLACK});
// Button setRoofLightTButton(3, 120, 75, 75, false, "", {LIGHTGREY, BLACK, BLACK});
// //Button setMaxPowerTButton(3, 120, 75, 75, false, "", {LIGHTGREY, BLACK, BLACK});
// Button setOffPowerTButton(240, 120, 75, 75, false, "", {LIGHTGREY, BLACK, BLACK});
// Button switchHeatCoolTButton(83, 48, 152, 66, false, "", {LIGHTGREY, BLACK, BLACK});
// Button manAutoTButton(3, 48, 75, 66, false, "", {LIGHTGREY, BLACK, BLACK});
// Button fxFanSpeed(240, 48, 75, 66, false, "", {LIGHTGREY, BLACK, BLACK});

void initButtons(){
  intensityIncTButton.initButton(&M5.Lcd, 83, 120, 75, 75, BLACK, LIGHTGREY, BLACK, "+", 1, 1);
  //intensityIncTButton.setFreeFont(&dodger320pt7b);
  intensityIncTButton.drawButton();
  intensityDecTButton.initButton(&M5.Lcd, 160, 120, 75, 75, BLACK, LIGHTGREY, BLACK, "-");
  intensityDecTButton.drawButton();
  setRoofLightTButton.initButton(&M5.Lcd, 3, 120, 75, 75, BLACK, LIGHTGREY, BLACK, "");
  setRoofLightTButton.drawButton();
  setOffPowerTButton.initButton(&M5.Lcd, 240, 120, 75, 75, BLACK, LIGHTGREY, BLACK, "");
  setOffPowerTButton.drawButton();
  switchHeatCoolTButton.initButton(&M5.Lcd, 83, 48, 152, 66, BLACK, LIGHTGREY, BLACK, "");
  switchHeatCoolTButton.drawButton();
  manAutoTButton.initButton(&M5.Lcd, 3, 48, 75, 66, BLACK, LIGHTGREY, BLACK, "");
  manAutoTButton.drawButton();
  fxFanSpeed.initButton(&M5.Lcd, 240, 48, 75, 66, BLACK, LIGHTGREY, BLACK, "");
  fxFanSpeed.drawButton();
   //intensityDecTButton.setFreeFont(&dodger320pt7b);
}


void setFlagConfig(uint8_t flagToSet){
  _nodeConfig.configuration |= flagToSet;
}

void clearFlagConfig(uint8_t flagToClear){
  _nodeConfig.configuration &= ~flagToClear;
}

bool isFlagConfig(uint8_t flagToCheck){
  return (_nodeConfig.configuration & flagToCheck);
}


void drawOffImageZone(){
   //gfx.fillRect(240, 120, 64, 64, TFT_MAGENTA);
   gfx.drawPng(stopSign48, ~0u, 252, 133);
   //gfx.drawPng(speedometerBWLow, ~0u, 252, 128);
}

void drawMaxImageZone(){
   //gfx.fillRect(10, 120, 64, 64, TFT_GREENYELLOW);
   gfx.drawPng(speedometerColor48, ~0u, 15, 133);
   //gfx.drawPng(speedometerBWHigh2, ~0u, 15, 128);
}

void drawRoofLightZone(){
   gfx.fillRect(5, 122, 73, 73, TFT_LIGHTGRAY);
  if ((_nodeConfig.configuration & CONFBIT_ROOFLIGHT) && (_currentState.ioState & OUTBIT_ROOF_LIGHT)){
    gfx.drawPng(bulbON48, ~0u, 15, 122);
    gfx.drawPng(autoGreenSign32, ~0u, 23, 166);
  }else if ((_nodeConfig.configuration & CONFBIT_ROOFLIGHT) && (_currentState.ioState & OUTBIT_ROOF_LIGHT) == 0){
    gfx.drawPng(bulbOFF48, ~0u, 15, 122);
    gfx.drawPng(autoGreenSign32, ~0u, 23, 166);
  }else if ((_nodeConfig.configuration & CONFBIT_ROOFLIGHT) == 0 && (_currentState.ioState & OUTBIT_ROOF_LIGHT)){
    gfx.drawPng(bulbON48, ~0u, 15, 133);
  }else if ((_nodeConfig.configuration & CONFBIT_ROOFLIGHT) == 0 && (_currentState.ioState & OUTBIT_ROOF_LIGHT) == 0){
    gfx.drawPng(bulbOFF48, ~0u, 15, 133);
  }
}

void drawManAutoImageZone(){
   gfx.fillRect(16, 56, 48, 48, TFT_LIGHTGRAY);
   if (_currentState.autoMode)
      gfx.drawPng(autoMode48, ~0u, 16, 56);
   else
      gfx.drawPng(manualMode48, ~0u, 16, 56);
}

void redrawAutoManMode(){
    gfx.fillRect(136, 56, 98, 42, LIGHTGREY);
    if (_currentState.autoMode){
      gfx.drawPng(celciusSign32, ~0u, 192, 67);
      displayReqTemperature();
    }else{
      gfx.setCursor(125, 204);
      gfx.setFont(&fonts::efontCN_24_b);
      gfx.setTextColor(DISP_TEXT_COLOR, DISP_BACK_COLOR);
      gfx.print("    ");
    }
}

void vibrate(){
  M5.Power.setVibration(100);
  delay(100);
  M5.Power.setVibration(0);
}


void setFlagCurrentState(uint8_t flagToSet){
  _currentState.ioState |= flagToSet;
}

void clearFlagCurrentState(uint8_t flagToClear){
  _currentState.ioState &= ~flagToClear;
}

bool isFlagCurrentState(uint8_t flagToCheck){
  return (_currentState.ioState & flagToCheck);
}

void applyReceivedData(){
  if (isFlagCurrentState(PAN_TCP_RSWITCH_LOCK))
    return;
}

void updateDisplayHvacData(){
  static uint8_t lastNodeConfiguraton = 0xFF;

  if (_currentState.ioState != _lastCurrentState.ioState || _currentState.fxReplaceFilterAlarm != _lastCurrentState.fxReplaceFilterAlarm){
    _lastCurrentState.ioState = _currentState.ioState;
    _lastCurrentState.fxReplaceFilterAlarm = _currentState.fxReplaceFilterAlarm;
    drawCoolHeatIcon();
    drawAwayFireFilterAlarmIcon();
    redrawAutoManMode();
    drawRoofLightZone();
    displayDacOutVoltage(TFT_GREEN, _currentState.dacOutVoltage);
  }

  if (_nodeConfig.configuration != lastNodeConfiguraton){
    lastNodeConfiguraton = _nodeConfig.configuration;
    drawRoofLightZone();
  }

  if (_currentState.fxForcedVentLeftSeconds > 0 && _currentState.fxCurrentTime != _lastCurrentState.fxCurrentTime){
    _currentState.fxForcedVentLeftSeconds -= (_currentState.fxCurrentTime - _lastCurrentState.fxCurrentTime);
    _lastCurrentState.fxCurrentTime = _currentState.fxCurrentTime;
    Serial.printf("fxForcedVentLeftSeconds = %d\n", _currentState.fxForcedVentLeftSeconds);
  }

  if (_currentState.fxFanSpeed != _lastCurrentState.fxFanSpeed
       || _currentState.fxForcedVentilation != _lastCurrentState.fxForcedVentilation
       || (_currentState.fxDataState & FXStepNoCorrectAnswer) != (_lastCurrentState.fxDataState & FXStepNoCorrectAnswer)){
    if (_currentState.fxForcedVentilation != _lastCurrentState.fxForcedVentilation){
      if (_currentState.fxForcedVentilation){
        _currentState.fxForcedVentLeftSeconds = _currentState.fxForcedVentilationTime * 60;
        _lastCurrentState.fxCurrentTime = _currentState.fxCurrentTime;
      }else{
        _currentState.fxForcedVentLeftSeconds = 0;
      }
    } 
    _lastCurrentState.fxFanSpeed = _currentState.fxFanSpeed;
    _lastCurrentState.fxForcedVentilation = _currentState.fxForcedVentilation;
    _lastCurrentState.fxDataState = _currentState.fxDataState;
    drawFlexItFanIcon();
  }

  if (_currentState.dacOutVoltage != _lastCurrentState.dacOutVoltage){
    _lastCurrentState.dacOutVoltage = _currentState.dacOutVoltage;
    displayDacOutVoltage(DISP_TEXT_COLOR, _currentState.dacOutVoltage);
  }

  if (_reDrawImageButtons){
    //drawMaxImageZone();
    drawRoofLightZone();
    drawOffImageZone();
    drawCoolHeatIcon();
    drawManAutoImageZone();
    drawFlexItFanIcon();
    _reDrawImageButtons = false;
  }
  if (_currentState.uartHMIState != _lastCurrentState.uartHMIState){
    _lastCurrentState.uartHMIState = _currentState.uartHMIState;
    displayHvacWiFiInfo();
  }
}

void displayHvacWiFiInfo(){
  gfx.setFont(WIFI_STATUS_FONT);
  gfx.setTextColor(DISP_TEXT_COLOR, DISP_BACK_COLOR);
  gfx.setCursor(2, 223);
  if (_currentState.uartHMIState == UARTHMI_NOANSWER){
    gfx.setTextColor(RED,  DISP_BACK_COLOR);
    gfx.print("UART HVAC ");
    gfx.setCursor(2, 208);
    gfx.print("** ERROR **");
    return;
  }
  gfx.setTextColor(DISP_TEXT_COLOR, DISP_BACK_COLOR);
  gfx.printf("10.%03d:%03d", _currentState.hvacipAddress, _currentState.hvacTcpIndexInConnTable);

  gfx.setCursor(2, 208);
  gfx.printf("%11s", _currentState.hvacWiFiSSID.substring(0,11).c_str());
}

void drawFlexItFanIcon(){
  uint8_t posYImage = 58;
  uint8_t posXImage = 254;
  gfx.fillRect(posXImage, posYImage, 48, 48, LIGHTGREY);
  gfx.setCursor(posXImage + 34, posYImage);
  gfx.setFont(&fonts::efontCN_16_b);
  gfx.setTextColor(DISP_TEXT_COLOR, LIGHTGREY);

  Serial.printf("FanSpeed = %d, FXDataState= %d\n", _currentState.fxFanSpeed, _currentState.fxDataState);
  if (_currentState.fxDataState & FXStepNoCorrectAnswer){
      gfx.drawPng(fanOFFCaution48, ~0u, posXImage, posYImage);
      return;
  }

  if (_currentState.fxForcedVentilation){
      gfx.drawPng(fanForcedVent48, ~0u, posXImage, posYImage);
      gfx.setTextColor(TFT_WHITE, TFT_RED);
      gfx.print("MAX");
      return;
  }

  switch (_currentState.fxFanSpeed){
    case 0:
      gfx.drawPng(fanOFF48, ~0u, posXImage, posYImage);
      //gfx.setTextColor(TFT_LIGHTGREY, DISP_BACK_COLOR);
      gfx.print("OFF");
    break;
    case 1:
      gfx.drawPng(fanMin48, ~0u, posXImage, posYImage);
      gfx.setTextColor(TFT_BLUE, LIGHTGREY);
      gfx.print("MIN");
    break;
    case 2:
      gfx.drawPng(fanMid48, ~0u, posXImage, posYImage);
      gfx.setTextColor(TFT_DARKGREEN, LIGHTGREY);
      gfx.print("MID");
    break;
    case 3:
      gfx.drawPng(fanMax48, ~0u, posXImage, posYImage);
      gfx.setTextColor(TFT_RED, LIGHTGREY);
      gfx.print("MAX");
    break;
  }
}

void drawCoolHeatIcon(){
  gfx.fillRect(87, 56, 48, 48, LIGHTGREY);
  if (isFlagCurrentState(OUTBIT_COOL1HEAT0)){
    gfx.drawPng(heatwave48, ~0u, 87, 56);
  }else{ 
    gfx.drawPng(freezingTemp48, ~0u, 87, 56);
  }
  displayDacOutVoltage(DISP_TEXT_COLOR, _currentState.dacOutVoltage);
}

void drawAwayFireFilterAlarmIcon(){
  uint8_t posYImage = 6;
  uint8_t posXImage = 124;

  if (_currentState.fxReplaceFilterAlarm){
    gfx.drawPng(filtrationAlarm32, ~0u, posXImage, posYImage);
  }else{
    if (_currentState.ioState & OUTBIT_FXHOME_AWAY)
      gfx.drawPng(secureHome32, ~0u, posXImage, posYImage);
    else
      gfx.fillRect(posXImage, posYImage, 32, 32, DISP_BACK_COLOR);
  }
  posYImage = 6;
  posXImage = 158;
  if (_currentState.ioState & OUTBIT_FXFIRE_ALARM)
    gfx.drawPng(flames32, ~0u, posXImage, posYImage);
  else
    gfx.fillRect(posXImage, posYImage, 32, 32, DISP_BACK_COLOR);
}

void displayDacOutVoltage(int dispColor, uint16_t dacOutVoltage){
  int bgColor = DISP_BACK_COLOR;
  if (_currentState.autoMode){
    gfx.setCursor(125, 204);
    gfx.setFont(&fonts::efontCN_24_b);
  }else{
    gfx.setCursor(138, 66);
    gfx.setFont(&data_latin24pt7b);
    bgColor = LIGHTGREY;
  }
  if (dacOutVoltage == 0){
    gfx.setTextColor(TFT_RED, bgColor);
    gfx.print("STOP");
  }else{
    gfx.setTextColor(dispColor, bgColor);
    gfx.printf("%3d%%", dacOutVoltage/100);
  }
  gfx.setTextColor(DISP_TEXT_COLOR, DISP_BACK_COLOR);
}

void displayReqTemperature(){
    gfx.setCursor(142, 66);
    gfx.setFont(&data_latin24pt7b);
    gfx.setTextColor(DISP_TEXT_COLOR, LIGHTGREY);
    gfx.printf("%2d", _currentState.reqTemperature);
    gfx.setTextColor(DISP_TEXT_COLOR, DISP_BACK_COLOR);

}

void displayCurrentTemp(){
  if (_currentState.idSHT > 0){
    gfx.setCursor(196, 2);
    gfx.setTextColor(DISP_TEXT_COLOR, DISP_BACK_COLOR);
    gfx.setFont(&prototype22pt7b);
    gfx.printf("%3.1f  ", _currentState.roomTemperature);
    gfx.drawPng(homeTemperature32, ~0u, 288, 4);
  }
}

void displayTime(){
  static uint8_t lastMinute = 60;
  rtc_time_t currentTime;
  M5.Rtc.getTime(&currentTime);
  if (lastMinute == currentTime.minutes)
    return;
  lastMinute = currentTime.minutes;
  gfx.setCursor(4, 4);
  const lgfx::v1::IFont *defFont = gfx.getFont();
  gfx.setFont(&data_latin24pt7b);
  gfx.setTextColor(DISP_TEXT_COLOR, DISP_BACK_COLOR);
  gfx.print("     ");
  gfx.setCursor(4, 4);
  gfx.printf("%2d:%02d", currentTime.hours, currentTime.minutes);
  gfx.setFont(defFont);
  if (!_currentState.getTimeFlag == TIMEFLAG_WAITFORMIDNIGHT && currentTime.hours == 4 && currentTime.minutes == 15)
    _currentState.getTimeFlag = TIMEFLAG_REQUIREPANTIME;
  if (_currentState.getTimeFlag  == TIMEFLAG_PANTIMERECEIVED && currentTime.minutes != 0)
    _currentState.getTimeFlag = TIMEFLAG_WAITFORMIDNIGHT;
}

void showRoofLightState(){
  uint8_t posXImage = 90;
  uint8_t posYImage = 204;

  gfx.fillRect(posXImage, posYImage, 32, 32, DISP_BACK_COLOR);
  if (_currentState.roofLightState & LUX_NOGAPODATA){
    gfx.drawPng(brightnessWarning32, ~0u, posXImage, posYImage);
    return;
  }
  switch(_currentState.roofLightState){
    case LUX_SWITCH_ON:
      gfx.drawPng(brightnessMin32, ~0u, posXImage, posYImage);
    break;
    case LUX_SWITCH_OFF:
      gfx.drawPng(brightnessMax32, ~0u, posXImage, posYImage);
    break;
    case LUX_BETWEEN_ON_OFF:
      gfx.drawPng(brightnessMid32, ~0u, posXImage, posYImage);
    break;
    case LUX_INITIALIZE:
      gfx.drawPng(brightnessWarning32, ~0u, posXImage, posYImage);
    break;
  }
}

void checkBatCondition(){
  uint8_t batLevel = (uint8_t)M5.Power.getBatteryLevel();
  gfx.setFont(&fonts::efontCN_16_b);
  gfx.setTextColor(TFT_BLACK, TFT_WHITE);
  gfx.setCursor(28, 210);
  gfx.printf("%3d%%", batLevel);

  if (M5.Power.isCharging())
    batLevel = 1;
  else if (batLevel > 70)
    batLevel = 2;
  else if (batLevel > 40)
    batLevel = 3;
  else
    batLevel = 4;

  if (batLevel != _lastBatLevel){
    _lastBatLevel = batLevel;
    if (batLevel == 1)
      gfx.drawPng(battery_charge, ~0u, 0, 207);
    else if (batLevel == 2)
      gfx.drawPng(battery_full, ~0u, 0, 207);
    else if (batLevel == 3)
      gfx.drawPng(battery_half, ~0u, 0, 207);
    else
      gfx.drawPng(battery_low, ~0u, 0, 207);
  }
}

void showWiFiStrength(int8_t lastRSSIValue){
  uint8_t currentWiFiIconType;
  // gfx.setFont(&fonts::efontCN_16_b);
  // gfx.setTextColor(DISP_TEXT_COLOR, TFT_WHITE);
  // gfx.setCursor(290, 192);
  // gfx.printf("%d%%", 100+lastRSSIValue);

  if (lastRSSIValue > -40)
    currentWiFiIconType = 1;
  else if (lastRSSIValue > -55)
    currentWiFiIconType = 2;
  else if (lastRSSIValue > -70)
    currentWiFiIconType = 3;
  else
    currentWiFiIconType = 4;

  if (_lastWiFiIconType != currentWiFiIconType){
    _lastWiFiIconType = currentWiFiIconType;
    redrawWiFiIcon(currentWiFiIconType);
    _lastConnectionState = WIFI_IDLE;
  }
}

void redrawWiFiIcon(uint8_t wifiIconType){
  gfx.fillRect(287, 207, 32, 32, DISP_BACK_COLOR);
  if (wifiIconType == 1)
    gfx.drawPng(wifiMax, ~0u, 287, 207);
  else if (wifiIconType == 2)
    gfx.drawPng(wifi75, ~0u, 287, 207);
  else if (wifiIconType == 3)
    gfx.drawPng(wifi25, ~0u, 287, 207);
  else
    gfx.drawPng(wifiMin, ~0u, 287, 207);
}

void showConnectingInfo(wifiState_t wifiState){
  gfx.fillRect(287, 207, 32, 32, DISP_BACK_COLOR);
  gfx.drawPng(searchWiFibw32, ~0u, 287, 207);
  gfx.setFont(WIFI_STATUS_FONT);
  gfx.setTextColor(DISP_TEXT_COLOR, DISP_BACK_COLOR);
  gfx.setCursor(170, 223);
  gfx.print("Connecting ... ");
}

void showNodeName(){
  gfx.setFont(&fonts::efontCN_16_i);
  gfx.setTextColor(DISP_TEXT_COLOR, DISP_BACK_COLOR);
  gfx.setCursor(35, 223);
  gfx.print(_nodeConfig.nodeName);
}

void showIpAddressAndSSID(wifiState_t wifiState){
  if (wifiState.tcpStatus == 0)
    return;
  gfx.setFont(WIFI_STATUS_FONT);
  gfx.setTextColor(DISP_TEXT_COLOR, DISP_BACK_COLOR);
  gfx.setCursor(175, 208);
  gfx.printf("%14s", wifiState.ssid);

  char buff[19];
    sprintf(buff, "%d.%d.%d.%d", wifiState.ipAddress[0], 
                                    wifiState.ipAddress[1], 
                                    wifiState.ipAddress[2], 
                                    wifiState.ipAddress[3]);
  gfx.setCursor(155, 223);
  gfx.printf("%16s", buff);
}

void showTcpConnectionState(wifiState_t wifiState){
  if (_lastConnectionState == wifiState.connectionState)
    return;
  _lastConnectionState = wifiState.connectionState;
  gfx.fillRect(305, 223, 16, 16, DISP_BACK_COLOR);

  if (wifiState.connectionState == WIFI_TCPREGISTERED || 
      wifiState.connectionState == WIFI_TCPCONNECTED ||
      wifiState.connectionState == WIFI_TCPTIMEOUT_NOANSWER || 
      wifiState.connectionState == WIFI_TCPWAITINGFORANSWER || 
      wifiState.connectionState == WIFI_TCPRECEIVEDDATA
      )
    gfx.drawPng(plug16, ~0u, 305, 223);
  else
    gfx.drawPng(cross, ~0u, 305, 223);
}

void showWiFiState(wifiState_t wifiState){
  static uint8_t lastTcpCheckCounter = 0xFF;
  if (lastTcpCheckCounter == wifiState.tcpCheckCounter)
    return;

  lastTcpCheckCounter = wifiState.tcpCheckCounter;
  if (wifiState.connectionState == WIFI_GETNEXTCONNECTION ||
      wifiState.connectionState == WIFI_SEARCHING ||
      wifiState.connectionState == WIFI_CONNECTING ||
      wifiState.connectionState == WIFI_STARTCHECKCONNECTION)
    showConnectingInfo(wifiState);

  if (wifiState.connectionState == WIFI_CONNECTED ||
      wifiState.connectionState == WIFI_TCPCONNECTING ||
      wifiState.connectionState == WIFI_TCPCONNECTED ||
      wifiState.connectionState == WIFI_TCPREGISTERED || 
      wifiState.connectionState == WIFI_TCPTIMEOUT_NOANSWER || 
      wifiState.connectionState == WIFI_TCPWAITINGFORANSWER || 
      wifiState.connectionState == WIFI_TCPSENTTOSERVER || 
      wifiState.connectionState == WIFI_IDLE || 
      wifiState.connectionState == WIFI_TCPRECEIVEDDATA
      ){
    showIpAddressAndSSID(wifiState);
    showWiFiStrength(wifiState.rssi);
    showNodeName();
    showTcpConnectionState(wifiState);
  }
}

void updateWiFiState(){
  wifiState_t wifiState;
  getWiFiState(&wifiState);  
  showWiFiState(wifiState);
}

int16_t temperatBMEBytesToInt(uint8_t highByte, uint8_t lowByte){
	int16_t outTemperature = highByte;
	outTemperature <<= 8;
	outTemperature  |= lowByte;
	float tempBME680 = outTemperature / 100;
	outTemperature = (int16_t)rintf(tempBME680);
	return outTemperature;
}

void incDACVoltage(){
  uint16_t dacOutVoltage = _currentState.dacOutVoltage;
  Serial.printf("dacOuVoltage = %d\n", dacOutVoltage);
  if (dacOutVoltage < 10000u){
    dacOutVoltage += 1000;
    rsSendSetDACVoltage(dacOutVoltage);
    displayDacOutVoltage(TFT_GREEN, dacOutVoltage);
  }
}

void setReqTemperature(uint8_t reqTemperature){
    _currentState.reqTemperature = reqTemperature;
    displayReqTemperature();
}

void incReqTemperature(){
  if (_currentState.reqTemperature < 30){
    _currentState.reqTemperature++;
    displayReqTemperature();
  }
}

void intensityIncButtonPressed(){
  vibrate();
  if (_currentState.autoMode)
    incReqTemperature();
  else
    incDACVoltage();
}

void decDACVoltage(){
  uint16_t dacOutVoltage = _currentState.dacOutVoltage;
  Serial.printf("dacOuVoltage = %d\n", dacOutVoltage);
  if (dacOutVoltage > 0u){
    dacOutVoltage -= 1000;
    rsSendSetDACVoltage(dacOutVoltage);
    displayDacOutVoltage(TFT_GREEN, dacOutVoltage);
  }
}

void decReqTemperature(){
  if (_currentState.reqTemperature > 16){
    _currentState.reqTemperature--;
    displayReqTemperature();
  }
}

void intensityDecButtonPressed(){
  vibrate();
  if (_currentState.autoMode)
    decReqTemperature();
  else
    decDACVoltage();
}

void setMaxPowerTButtonPressed(){
  vibrate();
  rsSendSetDACVoltage(10000);
  _currentState.autoMode = false;
  displayDacOutVoltage(TFT_GREEN, _currentState.dacOutVoltage);
  redrawAutoManMode();
}

void setRoofLightTButtonPressed(){
  vibrate();
  if (isFlagConfig(CONFBIT_ROOFLIGHT)){
      rsSendSetRoofLight(0);
      clearFlagConfig(CONFBIT_ROOFLIGHT);
  }else{
    if (!isFlagCurrentState(OUTBIT_ROOF_LIGHT)){
      rsSendSetRoofLight(1);
    }else{
      setFlagConfig(CONFBIT_ROOFLIGHT);
    }
  }
}

void setOffPowerTButtonPressed(){
  vibrate();
  rsSendSetDACVoltage(0);
  _currentState.autoMode = false;
  displayDacOutVoltage(TFT_GREEN, _currentState.dacOutVoltage);
  redrawAutoManMode();
}

void switchHeatCoolTButtonPressed(){
  rsSendSetDACVoltage(0);
  if (isFlagCurrentState(OUTBIT_COOL1HEAT0))
    rsSendSetHCState(0);
  else 
    rsSendSetHCState(1);
}

void fxFanSpeedTButtonPressed(){
  vibrate();
  if (_currentState.fxForcedVentilation){
    rsSendSetFlexitForcedVent(0);
    return;
  }

  if (_currentState.fxFanSpeed < 3)
    rsSendSetFlexitFanSpeed(_currentState.fxFanSpeed + 1);
  else
    rsSendSetFlexitFanSpeed(0);
}

void switchAutoManMode(){
    displayDacOutVoltage(TFT_GREEN, _currentState.dacOutVoltage);
    redrawAutoManMode();
}

void reDrawImageButtons(){
    _reDrawImageButtons = true;
}
void reqHVACCurrentState(){
  _getCurrentHvacStateFlag = 1;
}

void reqUpdatePAN(){
  _updatePANFlag = 1;
}

void getCurrentTemp(){
  if (_getCurrentTempFlag == 0)
    _getCurrentTempFlag = 1;
}

void hmiDimmerTick(){
  if (_hmiDimmValue > 25){
    _hmiDimmValue -= 25;
    _setHmiDimm = 1;
  }
}

void setup(){
  
  Serial.begin(115200);
  auto cfgM5 = M5.config();
  //bool LCDEnable = true, bool SDEnable = true, bool SerialEnable = true, bool I2CEnable = false, mbus_mode_t mode = KMBusModeOutput
  cfgM5.internal_rtc =true;
  cfgM5.disable_rtc_irq = true;
  M5.begin(cfgM5);
  M5.Rtc.begin();
  WiFi.mode(WIFI_STA);
  gfx.begin();
  gfx.clear(DISP_BACK_COLOR);
  gfx.setTextColor(DISP_TEXT_COLOR, DISP_BACK_COLOR);
  _netNodeParam.nodeAddrType = ADDR_HVACHMI;
  _netNodeParam.tcpNodeType = tcpNodeTypeHVACDispEnum;
  //initDS18B20();
  initSHT40();
  if (_currentState.idSHT > 0)
    gfx.drawPng(homeTemperature32, ~0u, 288, 4);
  else
    gfx.drawPng(homeTemperatureWarning32, ~0u, 288, 4);

  initNetwork();
  initSerialDataM5Stack();
  initButtons();
  //drawMaxImageZone();
  drawRoofLightZone();
  drawOffImageZone();
  drawManAutoImageZone();
  redrawAutoManMode();
  readConfiguration();
  _reqHVACCurrentStateTicker.attach_ms(TIMER_HVAC_UPDATE, reqHVACCurrentState);
  _getCurrentTempTicker.attach_ms(TIMER_GET_TEMP, getCurrentTemp);
  _hmiDimmerTicker.attach(10, hmiDimmerTick);
  _tcpPANUpdateTicker.attach_ms(TIMER_UPDATE_PAN, reqUpdatePAN);
  _lastCurrentState.ioState = 0xFF;
  _lastCurrentState.dacOutVoltage = 0xFFFF;
  _currentState.roofLightState = LUX_INITIALIZE;
}

void loop() {

  uint8_t receivedBuffer[TCP_BUFFER_SIZE];

  displayTime();
  if (_setHmiDimm == 1){
    gfx.setBrightness(_hmiDimmValue);
    _setHmiDimm = 0;
  }

  if (_getCurrentTempFlag ==1){
    _getCurrentTempFlag = 0;
    shtGetParameters();
    displayCurrentTemp();
    if (_currentState.autoMode){
      adjustTemperature();
    }
    if (_currentState.reqTemperature == 0xFF)
      _currentState.reqTemperature = _currentState.roomTemperature;
  }
  netService(receivedBuffer);
  processTcpDataReq(receivedBuffer);
  updateWiFiState();
  controlRoofLight();
  updateDisplayHvacData();

  if (processDataFromHVAC() > 0){
    if (_currentState.validDataHVAC == DATAHVAC_TCPREQ && netServiceReadyToSendNextPacket()){
      sendCurrentStateToServer();
      _updatePANFlag = 0;
    }
    _currentState.uartHMIState = UARTHMI_DATARECEIVED;
  }

  if (_updatePANFlag == 1 && netServiceReadyToSendNextPacket()){
    _updatePANFlag = 0;
    sendCurrentStateToServer();
  }

  if (_getCurrentHvacStateFlag == 1){
    _getCurrentHvacStateFlag = 0;
    rsSendGetCurrentState();
  }

  if (_getCurrentTimeFlag == TIMEFLAG_REQUIREPANTIME)
    getCurrentTime();

  M5.update();
  if (intensityIncTButton.justPressed()){
    intensityIncButtonPressed();
  }
  if (intensityDecTButton.justPressed()){
    intensityDecButtonPressed();
  }

  if (setRoofLightTButton.justPressed()){
    setRoofLightTButtonPressed();
  }

  if (setRoofLightTButton.justReleased()){
    _reDrawImageButtons = true;
  }

  if (setOffPowerTButton.justPressed()){
    setOffPowerTButtonPressed();
  }

  if (setOffPowerTButton.justReleased()){
    _reDrawImageButtons = true;
  }

  if (fxFanSpeed.justPressed()){
    fxFanSpeedTButtonPressed();
  }
  
  if (fxFanSpeed.justReleased()){
    _reDrawImageButtons = true;
  }
  
  if (switchHeatCoolTButton.justPressed()){
    vibrate();
    switchHeatCoolTButtonPressed();
  }

  if (switchHeatCoolTButton.justReleased()){
    _reDrawImageButtons = true;
  }

  if (manAutoTButton.justPressed()){
    vibrate();
    _currentState.autoMode = !_currentState.autoMode;
    switchAutoManMode();
  }

  if (manAutoTButton.justReleased()){
    reDrawImageButtons();
  }
  if (M5.Touch.getDetail().wasPressed()){
    if (_hmiDimmValue < 100){
      _hmiDimmValue = 100;
      _setHmiDimm = 1;
    }
  }
  delay(1);
}
