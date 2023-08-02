#include <HardwareSerial.h>

#include "main.h"
#include "serialDataM5.h"
#include "hvac.h"

extern currentState_t _currentState;

HardwareSerial uartToM5Stack(2);

void initSerialDataM5Stack(){
    uartToM5Stack.begin(115200, SERIAL_8N1, 16, 17);
}

void processDataFromHMI(){
    uint8_t receivedBuffer[RS_BUFFER_SIZE];
    size_t charsAvailable = uartToM5Stack.available();
    if (charsAvailable > 0){
        uint8_t receivedBytes = uartToM5Stack.read(receivedBuffer, charsAvailable);
        Serial.printf("RS received bytes = %d\n", receivedBytes);
        analizeReceivedData(receivedBuffer, receivedBytes);
    }
}

void analizeReceivedData(uint8_t *receivedBuffer, uint8_t receivedBytes){
    uint8_t iChar = 0;
    while (iChar < receivedBytes){
        if (receivedBuffer[iChar] == STX)
            break;
        iChar++;
    }
    if (iChar == receivedBytes)
        return;
    uint8_t posInPacket = 0;
    uint8_t cmdToExec = 0;
    uint16_t cmdParameter = 0;
    iChar++;
    while (iChar < receivedBytes){
        if (receivedBuffer[iChar] == ETX)
            break;
        
        switch (posInPacket){
            case 0:
                _currentState.tcpIndexInConnTable = receivedBuffer[iChar];
            break;
            case 1:
                cmdToExec = receivedBuffer[iChar];
                break;
            case 2:
                cmdParameter = receivedBuffer[iChar];
                cmdParameter <<= 8;
                break;
            case 3:
                cmdParameter |= receivedBuffer[iChar];
                break;
        }        
        posInPacket++;
        iChar++;
    }
    if (iChar == receivedBytes) //no ETX
        return;

    switch (cmdToExec){
        case HVAC_CMD_SETVOLTAGE:
            setDACVoltage(cmdParameter);
            break;
        case HVAC_CMD_COOL_HEAT:
            setHCState(cmdParameter);
            break;
        case HVAC_CMD_GETCURRENTDATA:
            sendHVACStateToHMI();
            break;
    }
}

void sendHVACStateToHMI(){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];
    updateCurrentInpState();
    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = _currentState.tcpIndexInConnTable;
    rsSendBuffer[2] = (byte)(_currentState.dacOutVoltage >> 8);
    rsSendBuffer[3] = (byte)(_currentState.dacOutVoltage);
    rsSendBuffer[4] = _currentState.currentInpOutState;
    rsSendBuffer[5] = ETX;

}
