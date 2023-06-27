#include <main.h>
#include <netService.h>
#include <sdService.h>

extern uint8_t _configuration;
extern String _nodeName;

extern M5GFX gfx;
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
uint8_t _wifiConnectionCheckTimer = 1;
uint8_t _tcpClientPANUpdateTimer = 1;

void sendToServer(uint8_t *tcpSendBuffer, uint8_t cmdTCP, uint8_t bytesToSend){
  //fill header
  if (_waitingForTcpServerAnswer > 0)
    return; //wait for previous transmition

  IPAddress ipAddress = WiFi.localIP();
  tcpSendBuffer[eTcpPacketPosAddressType] = ADDR_RSWI;
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
  tcpSendBuffer[eTcpPacketPosStartPayLoad] = tcpNodeTypeHVACDispEnum;
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
      if (receivedBuffer[eTcpPacketPosStartPayLoad] >= TCP_CONNECTION_SIZE){
        Serial.printf("PAN_TCP_REGISTERNODE_A: receivedBuffer[eTcpSendPacketPayLoad + 1] > TCP_CONNECTION_SIZE\n");
        reconnectTcpSocket();
        break;
      }
      _tcpIndexInConnectionTable = receivedBuffer[eTcpPacketPosStartPayLoad];
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
    case PAN_WRITE_REG:
      sendWriteRegistersAck(storeConfigurationToSD(receivedBuffer), receivedBuffer);
      readConfiguration();
      break;
    case PAN_READ_REG:
      sendConfiguration(receivedBuffer);
      break;
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

void connectToTCPServer(){
  if (!_clientTCP.connected()){
    gfx.drawPng(cross, ~0u, 305, 223);
    if (_clientTCP.connect(_ipAddressHoA, 28848) != 0){
      registerInHoa();
      gfx.fillRect(305, 223, 16, 16, DISP_BACK_COLOR);
    }
  }
  if (_clientTCP.connected()){
    gfx.drawPng(plug16, ~0u, 305, 223);
    if (_tcpIndexInConnectionTable >= TCP_CONNECTION_SIZE)
      registerInHoa();
  }
}

void connectToWiFi(){
  showConnectingInfo();
  WiFi.disconnect();
  int numFoundNets = WiFi.scanNetworks();
  if (numFoundNets == 0)
    return;
  int maxRSSI = 0;
  String netSSID;
  for (int iNet = 0; iNet < numFoundNets; iNet ++){
    if (WiFi.RSSI(iNet) + 100 > maxRSSI){
      maxRSSI = WiFi.RSSI(iNet) + 100;
      netSSID = WiFi.SSID(iNet);
      Serial.printf("Found: %s / %d\n", netSSID.c_str(), maxRSSI);
    }
  }

  WiFi.begin(netSSID.c_str(), "Rysia70bSiara");
}

void reconnectTcpSocket(){
  _clientTCP.stop();
  gfx.fillRect(305, 223, 16, 16, DISP_BACK_COLOR);
  gfx.drawPng(cross, ~0u, 305, 223);
  checkWiFiConnection();
}

void checkWiFiConnection(){
  if (WiFi.isConnected()){
    _wifiCheckTicker.detach();
    _wifiCheckTicker.attach(TIMER_WIFI_CHECK, setCheckWiFiConnection);
    _lastRSSIValue = 100 + WiFi.RSSI();
    showWiFiStrength(_lastRSSIValue);
    showIpAddressAndSSID(_tcpIndexInConnectionTable);
    connectToTCPServer();
  }
  if (!WiFi.isConnected() || _lastRSSIValue < 20){
    connectToWiFi();
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
  _wifiConnectionCheckTimer = 1;
}

void attachTcpTickers(){
    _tcpTicker.attach_ms(TIMER_PAN_UPDATE, refreshCurrentState);
    _wifiCheckTicker.attach(5, setCheckWiFiConnection);
}

void netService(){

  getDataFromTcp();

  if (_tcpClientPANUpdateTimer > 0)
  {
    _tcpClientPANUpdateTimer = 0;
    sendCurrentState(0);
  }

    if (_wifiConnectionCheckTimer > 0){
     checkWiFiConnection();
     _wifiConnectionCheckTimer = 0;
   }

  if (_noAnswerCounter > 3){
    _noAnswerCounter = 0;
    reconnectTcpSocket();
  }
  
}