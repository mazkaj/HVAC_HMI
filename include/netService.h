#ifndef __NETSERVICE_H
	#define	__NETSERVICE_H

#define WIFI_IDLE               0
#define WIFI_STARTCHECKCONNECTION    1
#define WIFI_SEARCHING          2
#define WIFI_CONNECTING         3
#define WIFI_GETNEXTCONNECTION  4
#define WIFI_CONNECTED          5
#define WIFI_TCPCONNECTING      6
#define WIFI_TCPCONNECTED       7
#define WIFI_TCPREGISTERED      8
#define WIFI_TCPRECONNECT       9

typedef struct
{
    uint8_t connectionState;
    String ssid;
    int8_t rssi;
    uint8_t tcpStatus;
    IPAddress ipAddress;
    uint8_t tcpIndexInConnectionTable;
    uint8_t tcpCheckCounter;
} wifiState_t;

typedef struct
{
    uint8_t nodeAddrType;
    tcpNodeTypeEnum_t tcpNodeType;
} netNodeParameter_t;

typedef enum {
    TCPStatusWiFiConnectedEnum = 0x01,
    TCPStatusTcpConnectedEnum = 0x02
}tcpStatusEnum_t;

void initNodeNetParameters(netNodeParameter_t netNodeParam);
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
bool isTCPConneted();
bool isWiFiStatusConnected();
void connectToTCPServer();
void showIpAddressAndSSID();
void searchAvailableWiFi();
bool isRegisteredAsFailed(uint8_t iNet);
void connectToWiFi();
void reconnectTcpSocket();
void checkWiFiConnection();
void waitForServerAnswerTimeOut();
void setCheckWiFiConnection();
void setFlagReqGaPoData();
void attachTcpTickers();
void attachNetTicker();
void netService();
#endif