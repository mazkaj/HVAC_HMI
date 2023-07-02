#include <main.h>
#include <netService.h>
#include <sdService.h>

uint8_t _configuration = 0;
String _nodeName;

M5GFX gfx;
uint8_t _lastWiFiIconType = 0;
uint8_t _currentState = 0;
uint8_t _lastBatLevel = 0;
uint8_t _getCurrentTimeFlag = TIMEFLAG_WAITFORMIDNIGHT;
int16_t _gapoTempValue;

Zone lockImageZone = Zone(64,0,64,64);
Zone autoModeImageZone = Zone(0,0,64,64);
Zone lightImageZone = Zone(128,0,64,64);

Button intensityIncTButton(3, 120, 75, 75, false, "+",  {GREEN, BLACK, WHITE});
Button intensityDecTButton(80, 120, 75, 75, false, "-",  {GREEN, BLACK, WHITE});

 void initButtons(){
   intensityIncTButton.setFreeFont(&dodger320pt7b);
   intensityDecTButton.setFreeFont(&dodger320pt7b);
   intensityIncTButton.setTextSize(1);
   intensityDecTButton.setTextSize(1);
   M5.Buttons.draw();
}

void vibrate(){
  M5.Axp.SetLDOEnable(3, true);
  delay(100);
  M5.Axp.SetLDOEnable(3, false);
}

void applyReceivedData(){
  if (isFlagCurrentState(PAN_TCP_RSWITCH_LOCK))
    return;

    showGaPoData();
}

void intensityIncButtonPressed(){
}

void intensityDecButtonPressed(){
}

void drawManualIcon(){
  gfx.fillRect(0, 0, 64, 64, DISP_BACK_COLOR);
  if (isFlagCurrentState(PAN_TCP_RSWITCH_AUTO))
    gfx.drawPng(auto48, ~0u, 8, 8);
  else
    gfx.drawPng(manualMode64, ~0u, 0, 0);
}

void showStatusIcons(){
}

void showWiFiStrength(int8_t lastRSSIValue){
  uint8_t currentWiFiIconType;
  gfx.setFont(&fonts::efontCN_16_b);
  gfx.setTextColor(DISP_TEXT_COLOR, TFT_WHITE);
  gfx.setCursor(290, 192);
  gfx.printf("%d%%", 100+lastRSSIValue);

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
  gfx.print(_nodeName);
}

void showIpAddressAndSSID(wifiState_t wifiState){
  if (wifiState.tcpStatus == 0)
    return;
  gfx.setFont(WIFI_STATUS_FONT);
  gfx.setTextColor(DISP_TEXT_COLOR, DISP_BACK_COLOR);
  char buff[19];
  if (wifiState.tcpIndexInConnectionTable < TCP_CONNECTION_SIZE)
    sprintf(buff, "%d.%d.%d.%d:%d", wifiState.ipAddress[0], 
                                    wifiState.ipAddress[1], 
                                    wifiState.ipAddress[2], 
                                    wifiState.ipAddress[3], 
                                    wifiState.tcpIndexInConnectionTable);
  else
    sprintf(buff, "%d.%d.%d.%d:??", wifiState.ipAddress[0], 
                                    wifiState.ipAddress[1], 
                                    wifiState.ipAddress[2], 
                                    wifiState.ipAddress[3]);
  gfx.setCursor(145, 223);
  gfx.printf("%18s", buff);

  gfx.setCursor(175, 208);
  gfx.printf("%14s", wifiState.ssid);
}

void showTcpConnectionState(wifiState_t wifiState){
  static bool tcpConnected = false;
  if (tcpConnected != isTCPConneted()){
    tcpConnected = isTCPConneted();
    gfx.fillRect(305, 223, 16, 16, DISP_BACK_COLOR);

    if (tcpConnected)
      gfx.drawPng(plug16, ~0u, 305, 223);
    else
      gfx.drawPng(cross, ~0u, 305, 223);
  }
}

void showWiFiState(wifiState_t wifiState){
  static uint8_t lastConnectionState = 0xFF;
  if (lastConnectionState == wifiState.connectionState)
    return;
  lastConnectionState = wifiState.connectionState;
  if (wifiState.connectionState == WIFI_GETNEXTCONNECTION ||
      wifiState.connectionState == WIFI_CONNECTING ||
      wifiState.connectionState == WIFI_STARTCHECKCONNECTION)
    showConnectingInfo(wifiState);

  if (wifiState.connectionState == WIFI_CONNECTED ||
      wifiState.connectionState == WIFI_TCPCONNECTING ||
      wifiState.connectionState == WIFI_TCPCONNECTED ||
      wifiState.connectionState == WIFI_TCPREGISTERED){
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

void refreshTime(){
  RTC_TimeTypeDef currentTime;
  M5.Rtc.GetTime(&currentTime);
  gfx.setTextColor(DISP_TEXT_COLOR, DISP_BACK_COLOR);
  gfx.setCursor(200, 15);
  const lgfx::v1::IFont *defFont = gfx.getFont();
  gfx.setFont(&data_latin24pt7b);
  gfx.printf("%2d:%02d", currentTime.Hours, currentTime.Minutes);
  gfx.setFont(defFont);
  if (!_getCurrentTimeFlag  == TIMEFLAG_WAITFORMIDNIGHT && currentTime.Hours == 4 && currentTime.Minutes == 15)
    _getCurrentTimeFlag = TIMEFLAG_REQUIREPANTIME;
  if (_getCurrentTimeFlag  == TIMEFLAG_PANTIMERECEIVED && currentTime.Minutes != 0)
    _getCurrentTimeFlag = TIMEFLAG_WAITFORMIDNIGHT;
}

void checkBatCondition(){
  uint8_t batLevel = (uint8_t)M5.Axp.GetBatteryLevel();
  gfx.setFont(&fonts::efontCN_16_b);
  gfx.setTextColor(DISP_TEXT_COLOR, DISP_BACK_COLOR);
  gfx.setCursor(28, 210);
  gfx.printf("%3d%%", batLevel);

  if (M5.Axp.isCharging())
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

void setFlagCurrentState(uint8_t flagToSet){
  _currentState |= flagToSet;
}

void clearFlagCurrentState(uint8_t flagToClear){
  _currentState &= ~flagToClear;
}

bool isFlagCurrentState(uint8_t flagToCheck){
  return (_currentState & flagToCheck);
}

int16_t temperatBMEBytesToInt(uint8_t highByte, uint8_t lowByte){
	int16_t outTemperature = highByte;
	outTemperature <<= 8;
	outTemperature  |= lowByte;
	float tempBME680 = outTemperature / 100;
	outTemperature = (int16_t)rintf(tempBME680);
	return outTemperature;
}

void setup(){
  
  Serial.begin(115200);
  M5.begin();
  gfx.begin();
  M5.Rtc.begin();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); 
  gfx.clear(DISP_BACK_COLOR);
  initButtons();
  setFlagCurrentState(PAN_TCP_RSWITCH_AUTO);
  showStatusIcons();

  attachNetTicker();
  attachTcpTickers();
//  readConfiguration();
}

void loop() {

  refreshTime();
  netService();
  updateWiFiState();
  checkBatCondition();

  if (_getCurrentTimeFlag == TIMEFLAG_REQUIREPANTIME)
    getCurrentTime();

  M5.update();
  if (intensityIncTButton.wasPressed()){
    intensityIncButtonPressed();
  }
  if (intensityDecTButton.wasPressed()){
    intensityDecButtonPressed();
  }
  // if (M5.Touch.changed){
  //   Point pressedPoint = M5.Touch.getPressPoint();
  //   if (pressedPoint.in(lightImageZone))
  //     toggleOutputState();
  //   if (pressedPoint.in(lockImageZone))
  //     toggleLockState();
  //   if (pressedPoint.in(autoModeImageZone) && !isFlagCurrentState(PAN_TCP_RSWITCH_AUTO))
  //     autoMode();
  //  }
  delay(1);
}
