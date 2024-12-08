#include <main.h>
#include "hvacHmi.h"
#include "netService.h"
#include "serialDataM5.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include "DFRobot_SHT40.h"

currentState_t _currentState;
nodeConfig_t _nodeConfig;

OneWire _ds(25);
DallasTemperature _dsSensors(&_ds);
DeviceAddress _thermometerAddress;

DFRobot_SHT40 _sensorSHT40(SHT40_AD1B_IIC_ADDR);
uint32_t _idSHT = 0;

void sendCurrentStateToServer(){
  uint8_t tcpSendBuffer[TCP_HEADER_LENGTH + 8];
  setAsRecipientServerIPAddress(tcpSendBuffer);
  tcpSendBuffer[eTcpPacketPosStartPayLoad] = getTcpIndexInConnTable();
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 1] = (byte)(_currentState.dacOutVoltage >> 8);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 2] = (byte)(_currentState.dacOutVoltage);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 3] = _currentState.ioState;
  if (_currentState.autoMode)
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 3] |= (1 << 4);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 4] = ((uint16_t)(_currentState.roomTemperature * 100) >> 8);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 5] = (uint8_t)(_currentState.roomTemperature * 100);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 6] = (uint8_t)_currentState.roomHumidity;
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 7] = _currentState.reqTemperature;
  Serial.printf("%d: sendCurrentState to %02X %02X\n", PAN_TCP_HVAC, tcpSendBuffer[eTcpPacketPosRecipientAddr0],  tcpSendBuffer[eTcpPacketPosRecipientAddr1]);
  sendToServer(tcpSendBuffer, PAN_TCP_HVAC, sizeof(tcpSendBuffer));
}

void sendCurrentState(byte recipientAddress0, byte recipientAddress1){
    uint8_t tcpSendBuffer[TCP_HEADER_LENGTH + 8];
    wifiState_t wifiState;
    getWiFiState(&wifiState);
    tcpSendBuffer[eTcpPacketPosRecipientAddr0] = recipientAddress0;
    tcpSendBuffer[eTcpPacketPosRecipientAddr1] = recipientAddress1;
    tcpSendBuffer[eTcpPacketPosStartPayLoad] = wifiState.tcpIndexInConnectionTable;
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 1] = (byte)(_currentState.dacOutVoltage >> 8);
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 2] = (byte)(_currentState.dacOutVoltage);
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 3] = _currentState.ioState;
    if (_currentState.autoMode)
      tcpSendBuffer[eTcpPacketPosStartPayLoad + 3] |= (1 << 4);
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 4] = ((uint16_t)(_currentState.roomTemperature * 100) >> 8);
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 5] = (uint8_t)(_currentState.roomTemperature * 100);
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 6] = (uint8_t)_currentState.roomHumidity;
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 7] = _currentState.reqTemperature;

    sendToServer(tcpSendBuffer, PAN_TCP_HVAC, sizeof(tcpSendBuffer));
    Serial.printf("%d: sendCurrentState to %02X %02X\n", PAN_TCP_HVAC, recipientAddress0, recipientAddress1);
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
    case HVAC_CMD_AUTO_MAN:
      if (receivedBuffer[eTcpPacketPosStartPayLoad + 2] == 2)
        _currentState.autoMode = !_currentState.autoMode;
      else
        _currentState.autoMode = (receivedBuffer[eTcpPacketPosStartPayLoad + 2] == 1);
      switchAutoManMode();
      reDrawImageButtons();
      _currentState.validDataHVAC = DATAHVAC_TCPREQ;
      break;
      case HVAC_CMD_SET_TEMPERATURE:
        setReqTemperature(receivedBuffer[eTcpPacketPosStartPayLoad + 2]);
        _currentState.validDataHVAC = DATAHVAC_TCPREQ;
      break;
  }
}

void initDS18B20(){
  _ds.reset_search();
  if (_ds.search(_thermometerAddress)){
    Serial.print("Found DS address: ");
    for (uint8_t i = 0; i < 8; i++){
      Serial.printf("%02X ", _thermometerAddress[i]);
    }
    Serial.println();
    _dsSensors.setResolution(_thermometerAddress, 12);
    _dsSensors.setWaitForConversion(false);
    _dsSensors.begin();
  }else{
    Serial.println("No DS18B20 found");
  }
}

void dsReqTemperature(){
  Serial.print("Request temperature\n");
  _dsSensors.requestTemperatures();
}

bool dsGetTemperature(){
  if (_dsSensors.isConversionComplete()){
    _currentState.roomTemperature = _dsSensors.getTempC(_thermometerAddress);
    Serial.printf("Temperature = %f\n", _currentState.roomTemperature);
    return true;
  }
  return false;
}

void initSHT40(){
    _sensorSHT40.begin();
    uint8_t numTryInitSHT = 3;
    while((_currentState.idSHT = _sensorSHT40.getDeviceID()) == 0 && numTryInitSHT > 0){
      Serial.println("ID retrieval error, please check whether the device is connected correctly!!!");
      delay(500);
      numTryInitSHT--;
    }
  Serial.print("SHT id :0x"); 
  Serial.println(_currentState.idSHT, HEX);
}

void shtGetParameters(){
    if (_currentState.idSHT > 0){
      _currentState.roomTemperature = _sensorSHT40.getTemperature(PRECISION_LOW);
      _currentState.roomHumidity = _sensorSHT40.getHumidity(PRECISION_LOW);
      Serial.printf("Temperature = %f\n", _currentState.roomTemperature);
      Serial.printf("Humidity = %f\n", _currentState.roomHumidity);
    }
}

void adjustTemperature(){

    float divTemperature;
    if (isFlagCurrentState(HVAC_IS_HEATING)){
      divTemperature = _currentState.reqTemperature - _currentState.roomTemperature;
    }else{
      divTemperature = _currentState.roomTemperature - _currentState.reqTemperature;
    }

    if (divTemperature < 0)
      rsSendSetDACVoltage(0);
    else if (divTemperature > 5)
      rsSendSetDACVoltage(10000);
    else if (divTemperature > 4.5)
      rsSendSetDACVoltage(9000);
    else if (divTemperature > 4.0)
      rsSendSetDACVoltage(8000);
    else if (divTemperature > 3.5)
      rsSendSetDACVoltage(7000);
    else if (divTemperature > 3.0)
      rsSendSetDACVoltage(6000);
    else if (divTemperature > 2.5)
      rsSendSetDACVoltage(5000);
    else if (divTemperature > 2.0)
      rsSendSetDACVoltage(4000);
    else if (divTemperature > 1.5)
      rsSendSetDACVoltage(3000);
    else if (divTemperature > 1.0)
      rsSendSetDACVoltage(2000);
    else if (divTemperature > 0.5)
      rsSendSetDACVoltage(1000);
    else
      rsSendSetDACVoltage(0);


}