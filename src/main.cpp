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
  gfx.printf("%d%%", lastRSSIValue);

  if (lastRSSIValue > 60)
    currentWiFiIconType = 1;
  else if (lastRSSIValue > 45)
    currentWiFiIconType = 2;
  else if (lastRSSIValue > 30)
    currentWiFiIconType = 3;
  else
    currentWiFiIconType = 4;

  if (_lastWiFiIconType != currentWiFiIconType){
    _lastWiFiIconType = currentWiFiIconType;
    redrawWiFiIcon(currentWiFiIconType);
  }
}

void showConnectingInfo(){
  gfx.fillRect(287, 207, 32, 32, DISP_BACK_COLOR);
  gfx.drawPng(searchWiFi32, ~0u, 287, 207);
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

void showIpAddressAndSSID(uint8_t tcpIndexInConnectionTable){
  gfx.setFont(WIFI_STATUS_FONT);
  gfx.setTextColor(DISP_TEXT_COLOR, DISP_BACK_COLOR);
  IPAddress ipAddress = WiFi.localIP();
  char buff[19];
  if (tcpIndexInConnectionTable < TCP_CONNECTION_SIZE)
    sprintf(buff, "%d.%d.%d.%d:%d", ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3], tcpIndexInConnectionTable);
  else
    sprintf(buff, "%d.%d.%d.%d:--", ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]);
  gfx.setCursor(145, 223);
  gfx.printf("%18s", buff);

  gfx.setCursor(177, 210);
  gfx.printf("%14s", WiFi.SSID());
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
  attachTcpTickers();
//  readConfiguration();
}

void loop() {

  refreshTime();
  netService();
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
