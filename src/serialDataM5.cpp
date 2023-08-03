#include <HardwareSerial.h>

#include "main.h"
#include "serialDataM5.h"

extern currentState_t _currentState;

HardwareSerial uartToM5Stack(2);

void initSerialDataM5Stack(){
    uartToM5Stack.begin(115200, SERIAL_8N1, 13, 14);
}

uint8_t processDataFromHVAC(){
    uint8_t receivedBuffer[RS_BUFFER_SIZE];
    size_t charsAvailable = uartToM5Stack.available();
    if (charsAvailable > 0){
        uint8_t receivedBytes = uartToM5Stack.read(receivedBuffer, charsAvailable);
        Serial.printf("RS received bytes = %d\n", receivedBytes);
        return analizeReceivedData(receivedBuffer, receivedBytes);
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
    uint8_t cmdToExec = 0;
    uint16_t cmdParameter = 0;
    iChar++;
    while (iChar < receivedBytes){
        if (receivedBuffer[iChar] == ETX)
            break;
        
        switch (posInPacket){
            case 0:
            break;
            case 1:
                _currentState.dacOutVoltage = receivedBuffer[iChar];
                _currentState.dacOutVoltage <<= 8;
                break;
            case 2:
                _currentState.dacOutVoltage |= receivedBuffer[iChar];
                break;
            case 3:
                _currentState.currentInpOutState = receivedBuffer[iChar];
                break;
        }        
        posInPacket++;
        iChar++;
    }
    if (iChar == receivedBytes) //no ETX
        return 0;
    return 1;
}

void rsSendSetDACVoltage(uint16_t setVoltage){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];
    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = _currentState.tcpIndexInConnTable;
    rsSendBuffer[2] = HVAC_CMD_SETVOLTAGE;
    rsSendBuffer[3] = (byte)(setVoltage >> 8);
    rsSendBuffer[4] = (byte)(setVoltage);
    rsSendBuffer[5] = ETX;
    uartToM5Stack.write(rsSendBuffer, RS_BUFFER_SIZE);
}

void rsSendSetHCState(uint8_t hcState){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];
    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = _currentState.tcpIndexInConnTable;
    rsSendBuffer[2] = HVAC_CMD_COOL_HEAT;
    rsSendBuffer[3] = 0;
    rsSendBuffer[4] = hcState;
    rsSendBuffer[5] = ETX;
    uartToM5Stack.write(rsSendBuffer, RS_BUFFER_SIZE);
}

void rsSendGetCurrentState(){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];
    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = _currentState.tcpIndexInConnTable;
    rsSendBuffer[2] = HVAC_CMD_GETCURRENTDATA;
    rsSendBuffer[3] = 0;
    rsSendBuffer[4] = 0;
    rsSendBuffer[5] = ETX;
    uartToM5Stack.write(rsSendBuffer, RS_BUFFER_SIZE);
}
