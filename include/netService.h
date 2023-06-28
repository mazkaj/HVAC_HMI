typedef struct
{
    String ssid;
    int8_t rssi;
    uint8_t tcpStatus;
    IPAddress ipAddress;
    uint8_t tcpIndexInConnectionTable;
} wifiState_t;

typedef enum {
    TCPStatusWiFiConnectedEnum = 0x01,
    TCPStatusTcpConnectedEnum = 0x02
}tcpStatusEnum_t;

void sendToServer(uint8_t *tcpSendBuffer, uint8_t cmdTCP, uint8_t bytesToSend);
void registerInHoa();
void getCurrentTime();
void sendCurrentState(ushort recipientAddress);
void sendWriteRegistersAck(uint8_t numRegisters, uint8_t *receivedBuffer);
void sendConfiguration(uint8_t *receivedBuffer);
void resetShrtGapoAddress();
void sendGetGapoShortAddres();
void processNodeInTable(uint8_t *receivedBuffer);
void sendGetGaPoData();
void processGapoData(uint8_t *receivedBuffer);
void proceedAnswer(uint8_t *receivedBuffer);
void getDataFromTcp();
void getWiFiState(wifiState_t *_wifiState);
void connectToTCPServer();
void showIpAddressAndSSID();
void connectToWiFi();
void reconnectTcpSocket();
void checkWiFiConnection();
void waitForServerAnswerTimeOut();
void setCheckWiFiConnection();
void setFlagReqGaPoData();
void attachTcpTickers();
void netService();