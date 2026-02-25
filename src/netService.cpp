#include "main.h"
#include "netService.h"
#ifdef USE_SDCARD
  #include "sdService.h"
#endif

extern uint8_t _configuration;
extern nodeConfig_t _nodeConfig;

extern currentState_t _currentState;

IPAddress _ipAddressHoA = {192, 168, 10, 191};

WiFiClient _clientTCP;
Ticker _wifiCheckTicker;
Ticker _tcpServerAnswerTimeOutTicker;

uint8_t _receivedBuffer[TCP_BUFFER_SIZE];
uint8_t _tcpIndexInConnectionTable = 0xFFU;
uint8_t _noAnswerCounter = 0;
uint8_t _tcpPacketNumber = 0U;
uint8_t _checkWiFi = 0;
uint8_t _wifiConnectionCheckTimer = 1;
uint8_t _wifiCheckCounter = 0;
int _netFailedConnection[WIFI_FAILEDCONNECTIONS];
uint8_t _netFailedConnectionIndex = 0;
uint8_t _lastReceivedBytesNumber;

netNodeParameter_t _netNodeParam;

void initNodeNetParameters(netNodeParameter_t netNodeParam){
  _netNodeParam = netNodeParam;
}

void initNetwork(){
  Serial.printf("Init wifi\n");
  WiFi.mode(WIFI_STA);
  attachNetTicker(TIMER_WIFI_CHECK);
  clearSsidFailedConnection();
  _netNodeParam.wiFiConnectionState = WIFI_STARTUP_APPL;
}

uint8_t sendToServer(uint8_t *tcpSendBuffer, uint8_t cmdTCP, uint8_t bytesToSend){
  
  if (_clientTCP.connected() == 0){
    Serial.printf("Send requirement abort - client not conneceted to server\r\n");
    reconnect();
    return 0; 
  }

  if (_netNodeParam.wiFiConnectionState == WIFI_TCPWAITINGFORANSWER){
    Serial.printf("Waiting for answer, can`t send next packet: %x tcpPacket:%d - packet lost!\r\n", cmdTCP, _tcpPacketNumber);
    return 0; //wait for previous transmition
  }

  Serial.printf("Sent packet: %x seq#: %02x\r\n", cmdTCP, _tcpPacketNumber);
  IPAddress ipAddress = WiFi.localIP();
  tcpSendBuffer[eTcpPacketPosAddressType] = _netNodeParam.nodeAddrType;
  tcpSendBuffer[eTcpPacketPosSenderAddr0] = (ipAddress[2] | 0x80);
  tcpSendBuffer[eTcpPacketPosSenderAddr1] = ipAddress[3];
  tcpSendBuffer[eTcpPacketPosPayLoadLength] = bytesToSend - TCP_HEADER_LENGTH;
  tcpSendBuffer[eTcpPacketPosPacketNumber] = _tcpPacketNumber++;
  tcpSendBuffer[eTcpPacketPosMiWiCommand] = cmdTCP;

  if (_clientTCP.write(tcpSendBuffer, bytesToSend) == 0)
  {
    _noAnswerCounter++;
    Serial.printf("Wrote 0 bytes! noAnsCounter = %d", _noAnswerCounter);
    return 0;
  }

  if ((cmdTCP & 0x80) == 0){ //sent requirement
    _netNodeParam.wiFiConnectionState = WIFI_TCPWAITINGFORANSWER;
    _tcpServerAnswerTimeOutTicker.attach_ms(TIMEOUT_TCP_ANSWER, waitForServerAnswerTimeOut);
  }else{
    _netNodeParam.wiFiConnectionState = WIFI_TCPSENTTOSERVER;
  }
  return bytesToSend;
}

uint8_t retransmitToServer(uint8_t *tcpSendBuffer, uint8_t cmdTCP, uint8_t bytesToSend){
  
  if (_clientTCP.connected() == 0){
    Serial.printf("Send requirement abort - client not conneceted to server");
    return 0; //wait for previous transmition
  }

  if (_netNodeParam.wiFiConnectionState == WIFI_TCPWAITINGFORANSWER){
    Serial.printf("Waiting for answer, can`t send next packet: %x tcpPacket:%d - packet lost!\r\n", cmdTCP, _tcpPacketNumber);
    return 0; //wait for previous transmition
  }

  Serial.printf("Sent packet: %x seq#: %02x\r\n", cmdTCP, _tcpPacketNumber);
  tcpSendBuffer[eTcpPacketPosPacketNumber] = _tcpPacketNumber++;

  if (_clientTCP.write(tcpSendBuffer, bytesToSend) == 0)
  {
    _noAnswerCounter++;
    Serial.printf("Wrote 0 bytes! noAnsCounter = %d", _noAnswerCounter);
    return 0;
  }

  if ((cmdTCP & 0x80) == 0){ //answer requirement
    _netNodeParam.wiFiConnectionState = WIFI_TCPWAITINGFORANSWER;
    _tcpServerAnswerTimeOutTicker.attach_ms(TIMEOUT_TCP_ANSWER, waitForServerAnswerTimeOut);
  }else{
    _netNodeParam.wiFiConnectionState = WIFI_TCPSENTTOSERVER;
  }
  return bytesToSend;
}

void setAsRecipientServerIPAddress(uint8_t *tcpSendBuffer){
  IPAddress serverIP = _clientTCP.remoteIP();
  tcpSendBuffer[eTcpPacketPosRecipientAddr0] = (serverIP[2] | 0x80);
  tcpSendBuffer[eTcpPacketPosRecipientAddr1] = serverIP[3];
}

bool netServiceReadyToSendNextPacket(){
  return (_netNodeParam.wiFiConnectionState == WIFI_TCPRECEIVEDDATA ||
          _netNodeParam.wiFiConnectionState == WIFI_TCPREGISTERED ||
          _netNodeParam.wiFiConnectionState == WIFI_TCPTIMEOUT_NOANSWER ||
          _netNodeParam.wiFiConnectionState == WIFI_TCPSENTTOSERVER ||
          _netNodeParam.wiFiConnectionState == WIFI_IDLE
          );
}

bool isNetServiceConnectedToHoA(){
  return (_netNodeParam.wiFiConnectionState == WIFI_TCPRECEIVEDDATA ||
          _netNodeParam.wiFiConnectionState == WIFI_TCPREGISTERED ||
          _netNodeParam.wiFiConnectionState == WIFI_TCPTIMEOUT_NOANSWER ||
          _netNodeParam.wiFiConnectionState == WIFI_TCPSENTTOSERVER ||
          _netNodeParam.wiFiConnectionState == WIFI_TCPWAITINGFORANSWER ||
          _netNodeParam.wiFiConnectionState == WIFI_IDLE
          );
}

void registerInHoa(){
  uint8_t tcpSendBuffer[TCP_HEADER_LENGTH + 7];
  setAsRecipientServerIPAddress(tcpSendBuffer);
  tcpSendBuffer[eTcpPacketPosStartPayLoad] = _netNodeParam.nodeAddrType;
  String macAsString = WiFi.macAddress();
  char macChars[18];
  macAsString.toCharArray(macChars, 18);
  uint8_t macBytes[6];
  sscanf(macChars, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &macBytes[5], &macBytes[4], &macBytes[3], &macBytes[2], &macBytes[1], &macBytes[0]);
  for (uint8_t iMac = 0; iMac < 6; iMac++){
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 1 + iMac] = macBytes[iMac];
  }
  sendToServer(tcpSendBuffer, PAN_TCP_REGISTERNODE, sizeof(tcpSendBuffer));
  Serial.printf("Require register %s in HOA\r\n", macChars);
}

void getCurrentTime(){
  uint8_t tcpSendBuffer[TCP_HEADER_LENGTH];
  setAsRecipientServerIPAddress(tcpSendBuffer);
  sendToServer(tcpSendBuffer, PAN_GET_RTCC, sizeof(tcpSendBuffer));
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
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 1] = _nodeConfig.configuration;
    for (size_t i = 0; i < _nodeConfig.nodeName.length(); i++)
    {
      tcpSendBuffer[eTcpPacketPosStartPayLoad + 6 + i] = _nodeConfig.nodeName.c_str()[i];
    }
    sendToServer(tcpSendBuffer, PAN_READ_REG_A, sizeof(tcpSendBuffer));
}

void proceedAnswer(){
  Serial.printf("Received: %x, seq# %02x, payload ... %d %x \r\n", _receivedBuffer[eTcpPacketPosMiWiCommand], _receivedBuffer[eTcpPacketPosPacketNumber], _receivedBuffer[eTcpPacketPosStartPayLoad], _receivedBuffer[eTcpPacketPosStartPayLoad + 1]);
  switch (_receivedBuffer[eTcpPacketPosMiWiCommand]){
    case PAN_TCP_REGISTERNODE_A:
      _tcpIndexInConnectionTable = 0xFF;
      if (_receivedBuffer[eTcpPacketPosStartPayLoad] >= TCP_CONNECTION_SIZE){
        Serial.printf("PAN_TCP_REGISTERNODE_A: receivedBuffer[eTcpPacketPosStartPayLoad] = %02x > TCP_CONNECTION_SIZE\n", _receivedBuffer[eTcpPacketPosStartPayLoad]);
        _netNodeParam.wiFiConnectionState = WIFI_CONNECTED;
        break;
      }
      Serial.printf("PAN_TCP_REGISTERNODE_A as node #: %d\n", _receivedBuffer[eTcpPacketPosStartPayLoad]);
      _tcpIndexInConnectionTable = _receivedBuffer[eTcpPacketPosStartPayLoad];
      _netNodeParam.wiFiConnectionState = WIFI_TCPREGISTERED;
  #ifdef USE_RTC
      getCurrentTime();
  #endif
      break;
    case PAN_GET_RTCC_A:
      if ((_receivedBuffer[eTcpPacketPosStartPayLoad + 7] & CS_VALIDTIME) == 0)
        break;
      _currentState.time.hours = _receivedBuffer[eTcpPacketPosStartPayLoad + 4];
      _currentState.time.minutes = _receivedBuffer[eTcpPacketPosStartPayLoad + 5];
      _currentState.time.seconds = _receivedBuffer[eTcpPacketPosStartPayLoad + 6];

      _currentState.date.year = 2000 + _receivedBuffer[eTcpPacketPosStartPayLoad];
      _currentState.date.month = _receivedBuffer[eTcpPacketPosStartPayLoad + 1];
      _currentState.date.date = _receivedBuffer[eTcpPacketPosStartPayLoad + 2];
      _currentState.date.weekDay = _receivedBuffer[eTcpPacketPosStartPayLoad + 3];
      _currentState.getTimeFlag = TIMEFLAG_PANTIMERECEIVED;
      Serial.printf("Time received: %02d:%02d:%02d %02d-%02d-%04d\n", 
        _currentState.time.hours,
        _currentState.time.minutes,
        _currentState.time.seconds,
        _currentState.date.date,
        _currentState.date.month,
        _currentState.date.year);
  #ifdef USE_M5
      M5.Rtc.setTime(&_currentState.time);
      M5.Rtc.setDate(&_currentState.date);
  #endif
      break;
#ifdef USE_SDCARD
    case PAN_WRITE_REG:
      sendWriteRegistersAck(storeConfigurationToSD(_receivedBuffer), _receivedBuffer);
      readConfiguration();
      break;
    case PAN_READ_REG:
      sendConfiguration(_receivedBuffer);
      break;
#endif
    default:
      //processDataReq(_receivedBuffer);
      break;
  }
}

int getDataFromTcp(uint8_t *receivedBuffer){
  int charsAvailable = _clientTCP.available();
  if (charsAvailable > 0)
  {
    _tcpServerAnswerTimeOutTicker.detach();
    _noAnswerCounter = 0;
    _netNodeParam.wiFiConnectionState = WIFI_TCPRECEIVEDDATA;
    _lastReceivedBytesNumber = charsAvailable;
    _clientTCP.readBytes(_receivedBuffer, charsAvailable);
    for(uint8_t i = 0; i < charsAvailable; i++){
      receivedBuffer[i] = _receivedBuffer[i];
    }
    proceedAnswer();
  }
  return charsAvailable;
}

void getWiFiState(wifiState_t *wifiState){
  wifiState->connectionState = _netNodeParam.wiFiConnectionState;
  wifiState->tcpStatus = 0;
  if (_netNodeParam.wiFiConnectionState == WIFI_STARTCONNECTION ||
      _netNodeParam.wiFiConnectionState == WIFI_SEARCHING ||
      _netNodeParam.wiFiConnectionState == WIFI_CONNECTING ||
      _netNodeParam.wiFiConnectionState == WIFI_STARTSEARCHCONNECTION)
      return;

  wifiState->ssid = WiFi.SSID();
  wifiState->rssi = WiFi.RSSI();
  wifiState->tcpIndexInConnectionTable = _tcpIndexInConnectionTable;
  wifiState->tcpCheckCounter = _wifiCheckCounter;
  wifiState->lastReceivedBytesNumber = _lastReceivedBytesNumber;
  if (WiFi.isConnected()){
    wifiState->tcpStatus |= TCPStatusWiFiConnectedEnum;
    wifiState->ipAddress = WiFi.localIP();
  }
  if (_clientTCP.connected()){
    wifiState->tcpStatus |= TCPStatusTcpConnectedEnum;
    wifiState->remoteIpAddress = _clientTCP.remoteIP();
  }
}

void resetSentRecvDataFlag(){
  if (_netNodeParam.wiFiConnectionState == WIFI_TCPRECEIVEDDATA || _netNodeParam.wiFiConnectionState == WIFI_TCPSENTTOSERVER)
    _netNodeParam.wiFiConnectionState = WIFI_IDLE;
}

uint8_t getTcpIndexInConnTable(){
  return _tcpIndexInConnectionTable;
}

bool isNodeRegistered(){
  return (_tcpIndexInConnectionTable < TCP_CONNECTION_SIZE);
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
  WiFi.scanDelete();
  delay(100);
  Serial.printf("Scanning WiFi ...");
  WiFi.scanNetworks(true, true);
}

void clearSsidFailedConnection(){
  _netFailedConnectionIndex = 0;
  for (uint8_t iPos = 0; iPos < WIFI_FAILEDCONNECTIONS; iPos++) 
    _netFailedConnection[iPos] = -1;
}

bool isRegisteredAsFailed(int iNet){
  for (int i = 0; i < _netFailedConnectionIndex; i++){
    if (iNet == _netFailedConnection[i]){
      Serial.printf("Skip: %s\n", WiFi.SSID(iNet).c_str());
      return true;
    }
  }
  return false;
}

void connectToWiFi(){
  int maxRSSI = -100;
  int netNumToConnect = -1;

  String netSSID;
  Serial.printf("Checking [%d] WiFi ...\n", WiFi.scanComplete());
  for (int iNet = 0; iNet < WiFi.scanComplete(); iNet ++){
    if (WiFi.SSID(iNet).isEmpty() 
        || !WiFi.SSID(iNet).equals("MAZKAJ")
        || isRegisteredAsFailed(iNet))
      continue;
    if (WiFi.RSSI(iNet) > maxRSSI){
      maxRSSI = WiFi.RSSI(iNet);
      netSSID = WiFi.SSID(iNet);
      netNumToConnect = iNet;
      Serial.printf("Found: %s RSSI = %d\n", netSSID.c_str(), maxRSSI);
    }
  }
  if (netNumToConnect == -1){
    Serial.printf("No found MAZKAJ WiFi to connect\n");
    _netNodeParam.wiFiConnectionState = WIFI_STARTSEARCHCONNECTION;
    return;
  }

  Serial.printf("Try connect to [%d]: %s \n", netNumToConnect, netSSID.c_str());
  _netFailedConnection[_netFailedConnectionIndex] = netNumToConnect;
  _netFailedConnectionIndex++;
  if (WiFi.begin(netSSID.c_str(), "Rysia70bSiara") == WL_CONNECT_FAILED){
    Serial.printf("WiFi connection failed\n");
    _netNodeParam.wiFiConnectionState = WIFI_STARTCONNECTION;
  }
}

void reconnect(){
  if (_clientTCP.connected()){
    _clientTCP.stop();
  }
  _netNodeParam.wiFiConnectionState = WIFI_CONNECTED;
  _tcpIndexInConnectionTable = TCP_CONNECTION_SIZE;
  _noAnswerCounter = 0;
  if (WiFi.isConnected() == false){
    _netNodeParam.wiFiConnectionState = WIFI_STARTSEARCHCONNECTION;
  }
}

void checkStrengthSignalWiFi(){
    _wifiCheckCounter = 0;
    if (WiFi.RSSI() < -85){
        Serial.printf("WiFi signal too low\n");
        reconnect();
    }
}

void checkTcpConnectionStilValid(){
  if (!_clientTCP.connected()){
    Serial.printf("Client TCP not connected - needs reconnect: wiFiConnectionState=%d _clientTCP.connected=%d\n", _netNodeParam.wiFiConnectionState, _clientTCP.connected());
    reconnect();
//  }else{
//    checkStrengthSignalWiFi();
  }
}

void checkWiFiConnection(){

  wl_status_t wifiStatus;
  int16_t wifiScanComplete;
  static uint8_t numTryConnect = 0;
  switch (_netNodeParam.wiFiConnectionState){
    case WIFI_STARTUP_APPL:
      reconnect();
    break;
    case WIFI_STARTSEARCHCONNECTION:
      Serial.printf("Start check connection wifi\n");
      _netNodeParam.wiFiConnectionState = WIFI_SEARCHING;
      _wifiCheckCounter = 0;
      clearSsidFailedConnection();
      searchAvailableWiFi();
    break;
    case WIFI_SEARCHING:
      wifiScanComplete = WiFi.scanComplete();
      if (wifiScanComplete == WIFI_SCAN_RUNNING){
        Serial.printf("WiFi is scanning ...\n");
        break;
      }
      if (wifiScanComplete == WIFI_SCAN_FAILED){
        Serial.printf("WiFi scan failed\n");
        _netNodeParam.wiFiConnectionState = WIFI_STARTSEARCHCONNECTION;
        break;
      }
      if (wifiScanComplete == 0){
        Serial.printf("No WiFi found\n");
        _netNodeParam.wiFiConnectionState = WIFI_STARTSEARCHCONNECTION;
        break;
      }
      Serial.printf(" (%d) WiFi found\n", wifiScanComplete);
      _netNodeParam.wiFiConnectionState = WIFI_STARTCONNECTION;
      break;
    case WIFI_STARTCONNECTION:
        if (_netFailedConnectionIndex < WIFI_FAILEDCONNECTIONS){
          _netNodeParam.wiFiConnectionState = WIFI_CONNECTING;
          for (int iNet = 0; iNet < WiFi.scanComplete(); iNet ++){
            Serial.printf("SSID[%d]: %s / RSSI: %d dBm\n", iNet, WiFi.SSID(iNet).c_str(), WiFi.RSSI(iNet));
          }
          connectToWiFi();
        }else{
          _netNodeParam.wiFiConnectionState = WIFI_STARTSEARCHCONNECTION;
        }
    break;
    case WIFI_CONNECTING:
      //attachNetTicker(TIMER_WIFI_CHECK);
      wifiStatus = WiFi.status();
      Serial.printf("wifi connecting status %d\n", wifiStatus);
      if (wifiStatus == WL_CONNECTED){
        _netNodeParam.wiFiConnectionState = WIFI_CONNECTED;
        Serial.printf("wifi connected\n");
      }else if (wifiStatus == WL_CONNECT_FAILED || wifiStatus == WL_NO_SSID_AVAIL || wifiStatus == WL_CONNECTION_LOST){
        _netNodeParam.wiFiConnectionState = WIFI_STARTCONNECTION;
        Serial.printf("wifi get next connection\n");
      }else if (wifiStatus == WL_DISCONNECTED || wifiStatus == WL_IDLE_STATUS){
        if (numTryConnect < 10){
          numTryConnect++;
          break;
        }
        numTryConnect = 0;
        _netNodeParam.wiFiConnectionState = WIFI_STARTCONNECTION;
      }else{
        _netNodeParam.wiFiConnectionState = WIFI_STARTSEARCHCONNECTION;
      }
    break;
    case WIFI_CONNECTED:
      Serial.println(WiFi.localIP());
      if (_clientTCP.connected()){
        _clientTCP.stop();
      }
      _netNodeParam.wiFiConnectionState = WIFI_TCPCONNECTING;
      _tcpIndexInConnectionTable = TCP_CONNECTION_SIZE;
      Serial.printf("Connecting to TCP server\n");
      connectToTCPServer();
    break;
    case WIFI_TCPCONNECTING:
        if (_clientTCP.connected()){
          _netNodeParam.wiFiConnectionState = WIFI_TCPCONNECTED;
        }
        else{
          Serial.printf("Can't connect to TCP server - try again\n");
          reconnect();
        }
    break;
    case WIFI_TCPCONNECTED:
        Serial.printf("Connected to TCP server\n");
        _wifiCheckCounter = 0;
      if (_tcpIndexInConnectionTable >= TCP_CONNECTION_SIZE){
        Serial.printf("Registering node to PAN\n");
        registerInHoa();
      }else{
          _netNodeParam.wiFiConnectionState = WIFI_TCPREGISTERED;
      }
    break;
    case WIFI_TCPSENTTOSERVER:
      checkTcpConnectionStilValid();
    break;
    case WIFI_TCPREGISTERED:
      checkTcpConnectionStilValid();
    break;
    case WIFI_TCPWAITINGFORANSWER:
      checkTcpConnectionStilValid();
    break;
    case WIFI_TCPRECEIVEDDATA:
      checkTcpConnectionStilValid();
    break;
    case WIFI_TCPTIMEOUT_NOANSWER:
      _noAnswerCounter++;
      _netNodeParam.wiFiConnectionState = WIFI_IDLE;
      Serial.printf("No answer timeout noAnsCounter = %d\n", _noAnswerCounter);
      if (_noAnswerCounter > 3){
        Serial.printf("No answer timeout - reconnecting to TCP server\n");
        reconnect();
      }
    break;
    case WIFI_IDLE:
      checkTcpConnectionStilValid();
      break;
  }
}


void waitForServerAnswerTimeOut(){
  _netNodeParam.wiFiConnectionState = WIFI_TCPTIMEOUT_NOANSWER;
  _tcpServerAnswerTimeOutTicker.detach();
}

void setCheckWiFiConnection(){
  if (_checkWiFi > 0)
    _checkWiFi --;
}

void attachNetTicker(uint16_t timeToExec){
    _wifiCheckTicker.detach();
    _wifiCheckTicker.attach_ms(timeToExec, setCheckWiFiConnection);
}

int netService(uint8_t *receivedBuffer){

  if (_checkWiFi == 0){
    _wifiCheckCounter++;
    checkWiFiConnection();
    _checkWiFi = 1;
  }
  
  return getDataFromTcp(receivedBuffer);
  
}