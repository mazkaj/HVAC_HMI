#include <main.h>
#include "hvacHmi.h"
#include "netService.h"
#include "serialDataM5.h"

currentState_t _currentState;
nodeConfig_t _nodeConfig;


void sendCurrentStateToPAN(){
   sendCurrentState(0, 0);
}

void sendCurrentState(byte recipientAddress0, byte recipientAddress1){
    uint8_t tcpSendBuffer[TCP_HEADER_LENGTH + 4];
    tcpSendBuffer[eTcpPacketPosRecipientAddr0] = recipientAddress0;
    tcpSendBuffer[eTcpPacketPosRecipientAddr1] = recipientAddress1;
    tcpSendBuffer[eTcpPacketPosStartPayLoad] = _currentState.tcpIndexInConnTable;
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 1] = (byte)(_currentState.dacOutVoltage >> 8);
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 2] = (byte)(_currentState.dacOutVoltage);
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 3] = _currentState.currentInpOutState;
    sendToServer(tcpSendBuffer, PAN_TCP_HVAC, sizeof(tcpSendBuffer));
    Serial.printf("sendCurrentState to %02X %02X\n", recipientAddress0, recipientAddress1);
    _currentState.validDataHVAC = DATAHVAC_VALID;
}

void processTcpDataReq(uint8_t *receivedBuffer){
  static uint8_t lastTcpPacketNumber = 0;
  uint16_t setVoltage = 0;
  if (receivedBuffer[eTcpPacketPosPacketNumber] == lastTcpPacketNumber)
    return;
  lastTcpPacketNumber = receivedBuffer[eTcpPacketPosPacketNumber];
  if (receivedBuffer[eTcpPacketPosMiWiCommand] != PAN_TCP_HVAC)
    return;
  Serial.printf("ProcessDataReq = %d\n", receivedBuffer[eTcpPacketPosStartPayLoad + 1]);

  switch (receivedBuffer[eTcpPacketPosStartPayLoad + 1]){
    case HVAC_CMD_GETCURRENTDATA:
      sendCurrentState(receivedBuffer[eTcpPacketPosSenderAddr0], receivedBuffer[eTcpPacketPosSenderAddr1]);
      break;
    case HVAC_CMD_SETVOLTAGE:
      setVoltage = receivedBuffer[eTcpPacketPosStartPayLoad + 2];
      setVoltage <<= 8;
      setVoltage |= receivedBuffer[eTcpPacketPosStartPayLoad + 3];
      rsSendSetDACVoltage(setVoltage);
      _currentState.validDataHVAC = DATAHVAC_TCPREQ;
      break;
    case HVAC_CMD_COOL_HEAT:
      rsSendSetHCState(receivedBuffer[eTcpPacketPosStartPayLoad + 2]);
      _currentState.validDataHVAC = DATAHVAC_TCPREQ;
      break;
  }
}


