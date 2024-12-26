#include <main.h>
#include "hvacHmi.h"
#include "netService.h"
#include "serialDataM5.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include "DFRobot_SHT40.h"

uint32_t calculateLUX(uint8_t highByte, uint8_t lowByte);
void processAnswerFromServer(uint8_t *receivedBuffer);
void putDataIntoSendBuffer(uint8_t *tcpSendBuff);
void setFlagConfig(uint8_t flagToSet);

currentState_t _currentState;
nodeConfig_t _nodeConfig;

OneWire _ds(25);
DallasTemperature _dsSensors(&_ds);
DeviceAddress _thermometerAddress;

DFRobot_SHT40 _sensorSHT40(SHT40_AD1B_IIC_ADDR);
uint32_t _idSHT = 0;

void sendCurrentStateToServer(){
  uint8_t tcpSendBuffer[TCP_PACKET_SIZE];
  setAsRecipientServerIPAddress(tcpSendBuffer);
  tcpSendBuffer[eTcpPacketPosStartPayLoad] = getTcpIndexInConnTable();
  putDataIntoSendBuffer(tcpSendBuffer);
  Serial.printf("0x%02x: sendCurrentState to %02X %02X\n", PAN_TCP_HVAC, tcpSendBuffer[eTcpPacketPosRecipientAddr0],  tcpSendBuffer[eTcpPacketPosRecipientAddr1]);
  sendToServer(tcpSendBuffer, PAN_TCP_HVAC, TCP_PACKET_SIZE);
}

void sendCurrentState(byte recipientAddress0, byte recipientAddress1){
    uint8_t tcpSendBuffer[TCP_PACKET_SIZE];
    wifiState_t wifiState;
    getWiFiState(&wifiState);
    tcpSendBuffer[eTcpPacketPosRecipientAddr0] = recipientAddress0;
    tcpSendBuffer[eTcpPacketPosRecipientAddr1] = recipientAddress1;
    tcpSendBuffer[eTcpPacketPosStartPayLoad] = wifiState.tcpIndexInConnectionTable;
    putDataIntoSendBuffer(tcpSendBuffer);
    sendToServer(tcpSendBuffer, PAN_TCP_HVAC, TCP_PACKET_SIZE);
    Serial.printf("0x%02x: sendCurrentState to %02X %02X\n", PAN_TCP_HVAC, recipientAddress0, recipientAddress1);
    _currentState.validDataHVAC = DATAHVAC_VALID;
}

void putDataIntoSendBuffer(uint8_t *tcpSendBuffer){
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 1] = (byte)(_currentState.dacOutVoltage >> 8);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 2] = (byte)(_currentState.dacOutVoltage);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 3] = _currentState.ioState;
  if (_currentState.autoMode)
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 3] |= OUTBIT_TEMPAUTO;
  if (isFlagConfig(CONFBIT_ROOFLIGHT))
    tcpSendBuffer[eTcpPacketPosStartPayLoad + 3] |= OUTBIT_ROOFLIGHTAUTO;
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 4] = _currentState.fxFanSpeed;
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 5] = _currentState.fxDataState;
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 6] = ((uint16_t)(_currentState.roomTemperature * 100) >> 8);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 7] = (uint8_t)(_currentState.roomTemperature * 100);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 8] = (uint8_t)_currentState.roomHumidity;
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 9] = _currentState.reqTemperature;
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 10] = (byte)(_currentState.fxForcedVentilationTime >> 8);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 11] = (byte)(_currentState.fxForcedVentilationTime);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 12] = _currentState.fxForcedVentilationSpeed;
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 13] = _currentState.fxForcedVentilation;
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 14] = (byte)(_currentState.fxOutdoorTemperaure >> 8);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 15] = (byte)(_currentState.fxOutdoorTemperaure);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 16] = (byte)(_currentState.fxSupplyTemperature >> 8);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 17] = (byte)(_currentState.fxSupplyTemperature);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 18] = _currentState.fxRegulationFanSpeed;
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 19] = (byte)(_currentState.fxForcedVentLeftSeconds >> 8);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 20] = (byte)(_currentState.fxForcedVentLeftSeconds);
  tcpSendBuffer[eTcpPacketPosStartPayLoad + 21] = _currentState.fxReplaceFilterAlarm;
}

void processTcpDataReq(uint8_t *receivedBuffer){
  static uint8_t lastTcpPacketNumber = 0;
  if (receivedBuffer[eTcpPacketPosPacketNumber] == lastTcpPacketNumber)
    return;
  lastTcpPacketNumber = receivedBuffer[eTcpPacketPosPacketNumber];
  
  if (receivedBuffer[eTcpPacketPosMiWiCommand] == PAN_TCP_HVAC_A){
    processAnswerFromServer(receivedBuffer);
    return;
  }

  if (receivedBuffer[eTcpPacketPosMiWiCommand] != PAN_TCP_HVAC)
    return;
  Serial.printf("ProcessDataReq = %d value = %02x:%02x\n", receivedBuffer[eTcpPacketPosStartPayLoad + 1], receivedBuffer[eTcpPacketPosStartPayLoad + 2], receivedBuffer[eTcpPacketPosStartPayLoad + 3]);
  uint16_t value = receivedBuffer[eTcpPacketPosStartPayLoad + 2];
  value <<= 8;
  value |= receivedBuffer[eTcpPacketPosStartPayLoad + 3];

  switch (receivedBuffer[eTcpPacketPosStartPayLoad + 1]){
    case HVAC_CMD_GETCURRENTDATA:
      sendCurrentState(receivedBuffer[eTcpPacketPosSenderAddr0], receivedBuffer[eTcpPacketPosSenderAddr1]);
      break;
    case HVAC_CMD_SETVOLTAGE:
      rsSendSetDACVoltage(value);
      _currentState.validDataHVAC = DATAHVAC_TCPREQ;
      break;
    case HVAC_CMD_COOL_HEAT:
      rsSendSetHCState(value);
      _currentState.validDataHVAC = DATAHVAC_TCPREQ;
      break;
    case HVAC_CMD_AUTO_MAN:
      if (value == 2)
        _currentState.autoMode = !_currentState.autoMode;
      else
        _currentState.autoMode = (value == 1);
      switchAutoManMode();
      reDrawImageButtons();
      _currentState.validDataHVAC = DATAHVAC_TCPREQ;
      break;
      case HVAC_CMD_SET_TEMPERATURE:
        setReqTemperature(value);
        _currentState.validDataHVAC = DATAHVAC_TCPREQ;
      break;
    case HVAC_CMD_SETFANSPEED:
      rsSendSetFlexitFanSpeed(value);
      _currentState.validDataHVAC = DATAHVAC_TCPREQ;
    break;
    case HVAC_CMD_ROOFLIGHT:
      setRoofLight(value);
      _currentState.validDataHVAC = DATAHVAC_TCPREQ;
    break;
    case HVAC_CMD_AWAY:
      rsSendSetFlexitAwayMode(value);
      _currentState.validDataHVAC = DATAHVAC_TCPREQ;
    break;
    case HVAC_CMD_FIRE:
      rsSendSetFlexitFireMode(value);
      _currentState.validDataHVAC = DATAHVAC_TCPREQ;
    break;
    case HVAC_CMD_FORCEDVENT:
      rsSendSetFlexitForcedVent(value);
      _currentState.validDataHVAC = DATAHVAC_TCPREQ;
    break;

  }
}

void processAnswerFromServer(uint8_t *receivedBuffer){
  _currentState.isGaPoValid = 0;
  if (receivedBuffer[eTcpPacketPosStartPayLoad + 3] == 0){
    Serial.printf("No valid gapoLux = %d\n", receivedBuffer[eTcpPacketPosStartPayLoad + 3]);
    return;
  }
  _currentState.isGaPoValid = 1;
  _currentState.gapoLuxValue = receivedBuffer[eTcpPacketPosStartPayLoad + 1];
  _currentState.gapoLuxValue <<= 8;
  _currentState.gapoLuxValue |= receivedBuffer[eTcpPacketPosStartPayLoad + 2];
  Serial.printf("Received gapoLux = %d\n", _currentState.gapoLuxValue);

}

uint32_t calculateLUX(uint8_t highByte, uint8_t lowByte){

	uint16_t optConversion;
	float luxValue;

	optConversion = 1;
	optConversion <<= (highByte >> 4);
	luxValue = (float)(0.01 * optConversion);
	optConversion = (highByte & 0x0F);
	optConversion <<= 8;
	optConversion += lowByte;
	luxValue *= optConversion;
	return (uint32_t)(rintf(luxValue));
}

void setRoofLight(uint8_t roofLightMode){
  if (roofLightMode == HVAC_AUTO){
    setFlagConfig(CONFBIT_ROOFLIGHT);
    return;
  }
  rsSendSetRoofLight(roofLightMode);
  clearFlagConfig(CONFBIT_ROOFLIGHT);
}

luxLightState_t determineRoofLightByLux(){
if (_currentState.gapoLuxValue <= _nodeConfig.tresholdOn)
    return LUX_SWITCH_ON;
  else if (_currentState.gapoLuxValue >= _nodeConfig.tresholdOff)
    return LUX_SWITCH_OFF;
  else
    return LUX_BETWEEN_ON_OFF;
}

luxLightState_t determineRoofLightByTime(){

 if (_nodeConfig.hourOn != _nodeConfig.hourOff){
    uint8_t currentTime = _currentState.time.Hours * 6 + _currentState.time.Minutes / 10;
    if (currentTime > _nodeConfig.hourOn || currentTime <_nodeConfig.hourOff){
      return LUX_SWITCH_ON;
    }else{
      return LUX_SWITCH_OFF;
    }
  }else{
    return LUX_INITIALIZE;
  }
}

void controlRoofLight(){
  uint8_t roofLightState = LUX_INITIALIZE;
  if (_currentState.isGaPoValid == 0){
    roofLightState = determineRoofLightByTime();
    roofLightState |= LUX_NOGAPODATA;
  }else{
    roofLightState = determineRoofLightByLux();
  }

  if (roofLightState != _currentState.roofLightState){
    Serial.printf("_gapoLux = %d\n", _currentState.gapoLuxValue);
    Serial.printf("roofLightState = %d _currentState.roofLightState=%d \n", roofLightState, _currentState.roofLightState);
    _currentState.roofLightState = roofLightState;
    showRoofLightState();
  }
  if (isFlagConfig(CONFBIT_ROOFLIGHT)){
    if ((_currentState.roofLightState & ~LUX_NOGAPODATA) == LUX_SWITCH_ON && (_currentState.ioState & OUTBIT_ROOF_LIGHT) == 0)
      rsSendSetRoofLight(1);

    if ((_currentState.roofLightState & ~LUX_NOGAPODATA) == LUX_SWITCH_OFF && (_currentState.ioState & OUTBIT_ROOF_LIGHT) > 0)
      rsSendSetRoofLight(0);
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
    if (isFlagCurrentState(OUTBIT_COOL1HEAT0)){
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


