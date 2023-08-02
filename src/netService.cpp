#include <main.h>
#include <netService.h>
#ifdef USE_SDCARD
  #include <sdService.h>
#endif

extern uint8_t _configuration;
extern String _nodeName;

extern uint8_t _currentState;
extern uint8_t _getCurrentTimeFlag;

IPAddress _ipAddressHoA = {192, 168, 10, 190};

WiFiClient _clientTCP;
Ticker _tcpTicker;
Ticker _wifiCheckTicker;
Ticker _tcpServerAnswerTimeOutTicker;

uint8_t _tcpIndexInConnectionTable = 0xFFU;
uint8_t _waitingForTcpServerAnswer = 0;
int8_t _lastRSSIValue = 0;
uint8_t _noAnswerCounter = 0;
uint8_t _tcpPacketNumber = 0U;
uint8_t _checkWiFi = 0;
uint8_t _checkWiFiConnectionState = WIFI_STARTCHECKCONNECTION;
uint8_t _wifiConnectionCheckTimer = 1;
uint8_t _wifiCheckCounter = 0;
uint8_t _tcpClientPANUpdateTimer = 1;
String _ssidFailedConnection[5];
uint8_t _ssidFailedConnectionIndex = 0;

netNodeParameter_t _netNodeParam;

void initNodeNetParameters(netNodeParameter_t netNodeParam){
  _netNodeParam = netNodeParam;
}

void sendToServer(uint8_t *tcpSendBuffer, uint8_t cmdTCP, uint8_t bytesToSend){
  //fill header
  if (_waitingForTcpServerAnswer > 0)
    return; //wait for previous transmition

  IPAddress ipAddress = WiFi.localIP();
  tcpSendBuffer[eTcpPacketPosAddressType] = _netNodeParam.nodeAddrType;
  tcpSendBuffer[eTcpPacketPosSenderAddr0] = ipAddress[3];
  tcpSendBuffer[eTcpPacketPosSenderAddr1] = (ipAddress[2] | 0x80);
  tcpSendBuffer[eTcpPacketPosPayLoadLength] = bytesToSend - TCP_HEADER_LENGTH;
  tcpSendBuffer[eTcpPacketPosPacketNumber] = _tcpPacketNumber;
  tcpSendBuffer[eTcpPacketPosMiWiCommand] = cmdTCP;
  _tcpPacketNumber++;
  Serial.printf("Sent packet: %x tcpPacket:%d\r\n", cmdTCP, _tcpPacketNumber);

  _clientTCP.write(tcpSendBuffer, bytesToSend);
  _waitingForTcpServerAnswer = 1;
  _tcpServerAnswerTimeOutTicker.attach_ms(TIMEOUT_TCP_ANSWER, waitForServerAnswerTimeOut);
}

void registerInHoa(){
  uint8_t tcpSendBuffer[TCP_HEADER_LENGTH + 7];
  tcpSendBuffer[eTcpPacketPosRecipientAddr0] = 0;
  tcpSendBuffer[eTcpPacketPosRecipientAddr1] = 0;
  tcpSendBuffer[eTcpPacketPosStartPayLoad] = _netNodeParam.tcpNodeType;
  String macAsString = WiFi.macAddress();
  char macChars[18];
  macAsString.toCharArray(macChars, 18);
  uint8_t macBytes[6];
  sscanf(macChars, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &macBytes[5], &macBytes[4], &macBytes[3], &macBytes[2], &macBytes[1], &macBytes[0]);
  for (uint8_t iMac = 0; iMac < 6; iMac++){
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 1 + iMac] = macBytes[iMac];
  }
  sendToServer(tcpSendBuffer, PAN_TCP_REGISTERNODE, sizeof(tcpSendBuffer));
}

void getCurrentTime(){
  uint8_t tcpSendBuffer[TCP_HEADER_LENGTH];
  tcpSendBuffer[eTcpPacketPosRecipientAddr0] = 0;
  tcpSendBuffer[eTcpPacketPosRecipientAddr1] = 0;
  sendToServer(tcpSendBuffer, PAN_GET_RTCC, sizeof(tcpSendBuffer));
}

void sendCurrentState(ushort recipientAddress){
  if (_tcpIndexInConnectionTable < TCP_CONNECTION_SIZE && _clientTCP.connected()){
    uint8_t tcpSendBuffer[TCP_HEADER_LENGTH + 2];
    tcpSendBuffer[eTcpPacketPosRecipientAddr0] = (byte)recipientAddress;
    tcpSendBuffer[eTcpPacketPosRecipientAddr1] = (byte)(recipientAddress >> 8);
    tcpSendBuffer[eTcpPacketPosStartPayLoad] = _tcpIndexInConnectionTable;
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 1] = _currentState;
    sendToServer(tcpSendBuffer, PAN_TCP_RSWITCH, sizeof(tcpSendBuffer));
  }
}

void sendWriteRegistersAck(uint8_t numRegisters, uint8_t *receivedBuffer){
    uint8_t tcpSendBuffer[TCP_HEADER_LENGTH + 2];
    tcpSendBuffer[eTcpPacketPosRecipientAddr0] = receivedBuffer[eTcpPacketPosSenderAddr0];
    tcpSendBuffer[eTcpPacketPosRecipientAddr1] = receivedBuffer[eTcpPacketPosSenderAddr1];
    tcpSendBuffer[eTcpPacketPosStartPayLoad] = receivedBuffer[eTcpPacketPosStartPayLoad];
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 1] = numRegisters;
    sendToServer(tcpSendBuffer, PAN_WRITE_REG_A, sizeof(tcpSendBuffer));

}

void sendConfiguration(uint8_t *receivedBuffer){
    uint8_t tcpSendBuffer[TCP_HEADER_LENGTH + 26];
    tcpSendBuffer[eTcpPacketPosRecipientAddr0] = receivedBuffer[eTcpPacketPosSenderAddr0];
    tcpSendBuffer[eTcpPacketPosRecipientAddr1] = receivedBuffer[eTcpPacketPosSenderAddr1];
    tcpSendBuffer[eTcpPacketPosStartPayLoad] = 0;
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 1] = _configuration;
    for (size_t i = 0; i < _nodeName.length(); i++)
    {
      tcpSendBuffer[eTcpPacketPosStartPayLoad + 6 + i] = _nodeName.c_str()[i];
    }
    sendToServer(tcpSendBuffer, PAN_READ_REG_A, sizeof(tcpSendBuffer));
}

void proceedAnswer(uint8_t* receivedBuffer){
  RTC_TimeTypeDef currentTime;
  RTC_DateTypeDef currentDate;
  Serial.printf("Received: %x, %d %x \r\n", receivedBuffer[eTcpPacketPosMiWiCommand], receivedBuffer[eTcpPacketPosStartPayLoad], receivedBuffer[eTcpPacketPosStartPayLoad]);
  switch (receivedBuffer[eTcpPacketPosMiWiCommand]){
    case PAN_TCP_REGISTERNODE_A:
      _tcpIndexInConnectionTable = 0xFF;
      if (receivedBuffer[eTcpPacketPosStartPayLoad] >= TCP_CONNECTION_SIZE){
        Serial.printf("PAN_TCP_REGISTERNODE_A: receivedBuffer[eTcpSendPacketPayLoad + 1] > TCP_CONNECTION_SIZE\n");
      _checkWiFiConnectionState = WIFI_STARTCHECKCONNECTION;
        break;
      }
      Serial.printf("PAN_TCP_REGISTERNODE_A as node #: %d\n", receivedBuffer[eTcpPacketPosStartPayLoad]);
      _tcpIndexInConnectionTable = receivedBuffer[eTcpPacketPosStartPayLoad];
      _checkWiFiConnectionState = WIFI_TCPREGISTERED;
      getCurrentTime();
      break;
    case PAN_GET_RTCC_A:
      if ((receivedBuffer[eTcpPacketPosStartPayLoad + 7] & CS_VALIDTIME) == 0)
        break;
      currentTime.Hours = receivedBuffer[eTcpPacketPosStartPayLoad + 4];
      currentTime.Minutes = receivedBuffer[eTcpPacketPosStartPayLoad + 5];
      currentTime.Seconds = receivedBuffer[eTcpPacketPosStartPayLoad + 6];
      M5.Rtc.SetTime(&currentTime);

      currentDate.Year = 2000 + receivedBuffer[eTcpPacketPosStartPayLoad];
      currentDate.Month = receivedBuffer[eTcpPacketPosStartPayLoad + 1];
      currentDate.WeekDay = receivedBuffer[eTcpPacketPosStartPayLoad + 2];
      currentDate.Date = receivedBuffer[eTcpPacketPosStartPayLoad + 3];
      M5.Rtc.SetDate(&currentDate);
      _getCurrentTimeFlag = TIMEFLAG_PANTIMERECEIVED;
      break;
#ifdef USE_SDCARD
    case PAN_WRITE_REG:
      sendWriteRegistersAck(storeConfigurationToSD(receivedBuffer), receivedBuffer);
      readConfiguration();
      break;
    case PAN_READ_REG:
      sendConfiguration(receivedBuffer);
      break;
#endif
  }
}

void getDataFromTcp(){
  uint8_t receivedBuffer[TCP_BUFFER_SIZE];
  int charsAvailable = _clientTCP.available();
  if (charsAvailable > 0)
  {
    _tcpServerAnswerTimeOutTicker.detach();
    _noAnswerCounter = 0;
    _waitingForTcpServerAnswer = 0;
    _clientTCP.readBytes(receivedBuffer, charsAvailable);
    proceedAnswer(receivedBuffer);
  }
}

void getWiFiState(wifiState_t *wifiState){
  wifiState->connectionState = _checkWiFiConnectionState;
  wifiState->ssid = WiFi.SSID();
  wifiState->rssi = WiFi.RSSI();
  wifiState->ipAddress = WiFi.localIP();
  wifiState->tcpIndexInConnectionTable = _tcpIndexInConnectionTable;
  wifiState->tcpStatus = 0;
  wifiState->tcpCheckCounter = _wifiCheckCounter;
  if (WiFi.isConnected())
    wifiState->tcpStatus |= TCPStatusWiFiConnectedEnum;
  if (_clientTCP.connected())
    wifiState->tcpStatus |= TCPStatusTcpConnectedEnum;
}

bool isTCPConneted(){
  return _clientTCP.connected();
}

bool isWiFiStatusConnected(){
  return WiFi.isConnected();
}

void connectToTCPServer(){
  if (!_clientTCP.connected()){
    _clientTCP.connect(_ipAddressHoA, 28848);
  }
}

void searchAvailableWiFi(){
  WiFi.disconnect();
  delay(100);
  Serial.printf("Scanning WiFi ...");
  WiFi.scanNetworks(true, true);
}

bool isRegisteredAsFailed(uint8_t iNet){
  for (int i = 0; i < _ssidFailedConnectionIndex; i++){
    if (WiFi.SSID(iNet) == _ssidFailedConnection[i]){
      Serial.printf("Skip: %s\n", WiFi.SSID(iNet).c_str());
      return true;
    }
  }
  return false;
}

void connectToWiFi(){
  int maxRSSI = -100;
  String netSSID;
  for (int iNet = 0; iNet < WiFi.scanComplete(); iNet ++){
    if (isRegisteredAsFailed(iNet))
      continue;
    if (WiFi.RSSI(iNet) > maxRSSI){
      maxRSSI = WiFi.RSSI(iNet);
      netSSID = WiFi.SSID(iNet);
      Serial.printf("Found: %s / %d\n", netSSID.c_str(), maxRSSI);
    }
  }
  if (maxRSSI == -100){
    Serial.printf("No found WiFi to connect\n");
    _ssidFailedConnectionIndex = sizeof(_ssidFailedConnection);
    return;
  }
  Serial.printf("Try connect to: %s \n", netSSID.c_str());
  _ssidFailedConnection[_ssidFailedConnectionIndex] = netSSID;
  WiFi.begin(netSSID.c_str(), "Rysia70bSiara");
}

void reconnectTcpSocket(){
  _clientTCP.stop();
  _checkWiFiConnectionState = WIFI_TCPRECONNECT;
}

void checkWiFiConnection(){

  if (_wifiCheckCounter > 30 || _checkWiFiConnectionState == WIFI_TCPRECONNECT){
    _checkWiFiConnectionState = WIFI_STARTCHECKCONNECTION;
    Serial.printf("Restart wifi connecting\n");
  }
  wl_status_t wifiStatus;
  int16_t wifiScanComplete;
  switch (_checkWiFiConnectionState){
    case WIFI_STARTCHECKCONNECTION:
      _wifiCheckCounter = 0;
      _ssidFailedConnectionIndex = 0;
      if (!WiFi.isConnected() || _lastRSSIValue < 20){
        _checkWiFiConnectionState = WIFI_SEARCHING;
        WiFi.scanDelete();
        searchAvailableWiFi();
      }else{
        _checkWiFiConnectionState = WIFI_CONNECTED;
      }
    break;
    case WIFI_SEARCHING:
      wifiScanComplete = WiFi.scanComplete();
      if (wifiScanComplete == WIFI_SCAN_RUNNING){
        Serial.printf(".");
        break;
      }
      if (wifiScanComplete == WIFI_SCAN_FAILED){
        Serial.printf("WiFi scan failed\n");
        _checkWiFiConnectionState = WIFI_STARTCHECKCONNECTION;
        break;
      }
      if (wifiScanComplete == 0){
        Serial.printf("No WiFi found\n");
        _checkWiFiConnectionState = WIFI_STARTCHECKCONNECTION;
        break;
      }
      Serial.printf(" (%d) WiFi found\n", wifiScanComplete);
      _checkWiFiConnectionState = WIFI_GETNEXTCONNECTION;
      break;
    case WIFI_GETNEXTCONNECTION:
        if (_ssidFailedConnectionIndex < sizeof(_ssidFailedConnection)){
          _checkWiFiConnectionState = WIFI_CONNECTING;
          connectToWiFi();
        }else{
          _ssidFailedConnectionIndex = 0;
          _checkWiFiConnectionState = WIFI_STARTCHECKCONNECTION;
        }
    break;
    case WIFI_CONNECTING:
      wifiStatus = WiFi.status();
      Serial.printf("wifi connecting status %d\n", wifiStatus);
      if (wifiStatus == WL_CONNECTED){
        _checkWiFiConnectionState = WIFI_CONNECTED;
      }else if (wifiStatus == WL_CONNECT_FAILED || wifiStatus == WL_NO_SSID_AVAIL || wifiStatus == WL_CONNECTION_LOST || wifiStatus == WL_DISCONNECTED){
        _ssidFailedConnectionIndex++;
        _checkWiFiConnectionState = WIFI_GETNEXTCONNECTION;
      }
    break;
    case WIFI_CONNECTED:
      if (_clientTCP.connected()){
        _checkWiFiConnectionState = WIFI_TCPCONNECTED;
      }
      else{
          _checkWiFiConnectionState = WIFI_TCPCONNECTING;
          connectToTCPServer();
      }
    break;
    case WIFI_TCPCONNECTING:
        if (_clientTCP.connected()){
          _checkWiFiConnectionState = WIFI_TCPCONNECTED;
        }
        else{
          _checkWiFiConnectionState = WIFI_TCPRECONNECT;
        }
    break;
    case WIFI_TCPCONNECTED:
        _wifiCheckCounter = 0;
      if (_clientTCP.connected() && _tcpIndexInConnectionTable >= TCP_CONNECTION_SIZE){
        registerInHoa();
      }
    break;
  }

  if (WiFi.isConnected()){
    _lastRSSIValue = 100 + WiFi.RSSI();
  }
  
}

void waitForServerAnswerTimeOut(){
  _noAnswerCounter++;
  _waitingForTcpServerAnswer = 0;
  _tcpServerAnswerTimeOutTicker.detach();
}

void refreshCurrentState(){
  _tcpClientPANUpdateTimer = 1;
}

void setCheckWiFiConnection(){
  _checkWiFi = 1;
  _wifiCheckCounter++;
}

void attachTcpTickers(){
    _tcpTicker.attach_ms(TIMER_PAN_UPDATE, refreshCurrentState);
}

void attachNetTicker(){
    _wifiCheckTicker.attach_ms(TIMER_WIFI_CHECK, setCheckWiFiConnection);
}

void netService(){

  if (_checkWiFi){
    checkWiFiConnection();
    _checkWiFi = 0;
  }
  getDataFromTcp();

  if (_tcpClientPANUpdateTimer > 0 && _checkWiFiConnectionState == WIFI_TCPREGISTERED)
  {
    _tcpClientPANUpdateTimer = 0;
    sendCurrentState(0);
  }

  if (_noAnswerCounter > 3){
    _noAnswerCounter = 0;
    reconnectTcpSocket();
  }
  
}