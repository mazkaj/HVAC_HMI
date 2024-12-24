#include <HardwareSerial.h>

#include "main.h"
#include "serialDataM5.h"

extern currentState_t _currentState;

HardwareSerial uartToM5Stack(2);

void initSerialDataM5Stack(){
    uartToM5Stack.begin(115200, SERIAL_8N1, 13, 14);
    _currentState.uartHMIState = UARTHMI_IDLE;
}

uint8_t processDataFromHVAC(){
    static uint8_t uartTry = 0;
    uint8_t receivedBuffer[RS_HVACBUFFER_SIZE];
    size_t charsAvailable = uartToM5Stack.available();
    if (charsAvailable > 0){
        if (charsAvailable > RS_HVACBUFFER_SIZE)
            charsAvailable = RS_HVACBUFFER_SIZE;
        uint8_t receivedBytes = uartToM5Stack.read(receivedBuffer, charsAvailable);
        _currentState.uartHMIState = UARTHMI_DATARECEIVED;
        uartTry = 0;
        return analizeReceivedData(receivedBuffer, receivedBytes);
    }else if (_currentState.uartHMIState == UARTHMI_WAITING_ACK){
        uartTry ++;
        if (uartTry > 3){
            _currentState.uartHMIState = UARTHMI_NOANSWER;
            uartTry = 0;
        }
    }
    return 0;
}

uint8_t analizeReceivedData(uint8_t *receivedBuffer, uint8_t receivedBytes){
    uint8_t iChar = 0;
    while (iChar < receivedBytes){
        if (receivedBuffer[iChar] == STX)
            break;
        iChar++;
    }
    if (iChar == receivedBytes)
        return 0;
    uint8_t posInPacket = 0;
    iChar++;
    _currentState.hvacWiFiSSID = "";
    while (iChar < receivedBytes){

//        Serial.printf("%02X ", receivedBuffer[iChar]);

        if (receivedBuffer[iChar] == ETX && posInPacket == RS_HVACBUFFER_SIZE - 2)
            break;
        
        if (posInPacket == 0){
            _currentState.hvacTcpIndexInConnTable = receivedBuffer[iChar];
        }
        if (posInPacket == 1){
            _currentState.dacOutVoltage = receivedBuffer[iChar];
            _currentState.dacOutVoltage <<= 8;
        }
        if (posInPacket == 2)
                _currentState.dacOutVoltage |= receivedBuffer[iChar];
        
        if (posInPacket == 3)
            _currentState.ioState = receivedBuffer[iChar];

        if (posInPacket >= 4 && posInPacket <= 18 && receivedBuffer[iChar] != 0){
            _currentState.hvacWiFiSSID += (char)receivedBuffer[iChar];
        }

        if (posInPacket == 19)
            _currentState.hvacipAddress = receivedBuffer[iChar];

        if (posInPacket == 20)
            _currentState.fxFanSpeed = receivedBuffer[iChar];

        if (posInPacket == 21)
            _currentState.fxDataState = receivedBuffer[iChar];

        if (posInPacket == 22){
            _currentState.fxForcedVentilationTime = receivedBuffer[iChar];
            _currentState.fxForcedVentilationTime <<= 8;
        }
        if (posInPacket == 23)
            _currentState.fxForcedVentilationTime |= receivedBuffer[iChar];

        if (posInPacket == 24){
            _currentState.fxForcedVentilationSpeed = receivedBuffer[iChar];
        }

        if (posInPacket == 25)
            _currentState.fxForcedVentilation = receivedBuffer[iChar];

        if (posInPacket == 26){
            _currentState.fxOutdoorTemperaure = receivedBuffer[iChar];
            _currentState.fxOutdoorTemperaure <<= 8;
        }
        if (posInPacket == 27)
            _currentState.fxOutdoorTemperaure |= receivedBuffer[iChar];

        if (posInPacket == 28){
            _currentState.fxSupplyTemperature = receivedBuffer[iChar];
            _currentState.fxSupplyTemperature <<= 8;
        }
        if (posInPacket == 29)
            _currentState.fxSupplyTemperature |= receivedBuffer[iChar];

        if (posInPacket == 30)
            _currentState.fxRegulationFanSpeed = receivedBuffer[iChar];

        posInPacket++;
        iChar++;
    }
  //  Serial.print("\n");

    if (iChar == receivedBytes) //no ETX
        return 0;

    return 1;
}

void sendToHVAC(uint8_t *rsSendBuffer){
    uartToM5Stack.write(rsSendBuffer, RS_BUFFER_SIZE);
    if (_currentState.uartHMIState == UARTHMI_DATARECEIVED)
        _currentState.uartHMIState = UARTHMI_WAITING_ACK;
    else
        _currentState.uartHMIState = UARTHMI_NOANSWER;
}

void rsSendSetDACVoltage(uint16_t setVoltage){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];

    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = HVAC_CMD_SETVOLTAGE;
    rsSendBuffer[2] = (uint8_t)(setVoltage >> 8);
    rsSendBuffer[3] = (uint8_t)(setVoltage);
    rsSendBuffer[4] = ETX;
    sendToHVAC(rsSendBuffer);
}

void rsSendSetHCState(uint8_t hcState){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];
    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = HVAC_CMD_COOL_HEAT;
    rsSendBuffer[2] = 0;
    rsSendBuffer[3] = hcState;
    rsSendBuffer[4] = ETX;
    sendToHVAC(rsSendBuffer);
}

void rsSendSetFlexitFanSpeed(uint16_t fxFanSpeed){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];
    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = HVAC_CMD_SETFANSPEED;
    rsSendBuffer[2] = fxFanSpeed >> 8;
    rsSendBuffer[3] = fxFanSpeed & 0xFF;
    rsSendBuffer[4] = ETX;
    sendToHVAC(rsSendBuffer);
}

void rsSendSetFlexitForcedVent(uint16_t forcedVentTime){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];
    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = HVAC_CMD_FORCEDVENT;
    rsSendBuffer[2] = forcedVentTime >> 8;
    rsSendBuffer[3] = forcedVentTime & 0xFF;
    rsSendBuffer[4] = ETX;
    sendToHVAC(rsSendBuffer);
}

void rsSendSetFlexitAwayMode(uint8_t awayMode){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];
    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = HVAC_CMD_AWAY;
    rsSendBuffer[2] = 0;
    rsSendBuffer[3] = awayMode;
    rsSendBuffer[4] = ETX;
    sendToHVAC(rsSendBuffer);
}

void rsSendSetFlexitFireMode(uint8_t fireMode){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];
    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = HVAC_CMD_FIRE;
    rsSendBuffer[2] = 0;
    rsSendBuffer[3] = fireMode;
    rsSendBuffer[4] = ETX;
    sendToHVAC(rsSendBuffer);
}

void rsSendSetRoofLight(uint8_t roofLight){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];
    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = HVAC_CMD_ROOFLIGHT;
    rsSendBuffer[2] = 0;
    rsSendBuffer[3] = roofLight;
    rsSendBuffer[4] = ETX;
    sendToHVAC(rsSendBuffer);
}


void rsSendGetCurrentState(){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];
    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = HVAC_CMD_GETCURRENTDATA;
    rsSendBuffer[2] = 0;
    rsSendBuffer[3] = 0;
    rsSendBuffer[4] = ETX;
    sendToHVAC(rsSendBuffer);
}
