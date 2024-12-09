#include <HardwareSerial.h>

#include "main.h"
#include "serialDataM5.h"

extern currentState_t _currentState;

HardwareSerial uartToM5Stack(2);

void initSerialDataM5Stack(){
    uartToM5Stack.begin(115200, SERIAL_8N1, 13, 14);
}

uint8_t processDataFromHVAC(){
    uint8_t receivedBuffer[RS_HVACBUFFER_SIZE];
    size_t charsAvailable = uartToM5Stack.available();
    if (charsAvailable > 0){
        if (charsAvailable > RS_HVACBUFFER_SIZE)
            charsAvailable = RS_HVACBUFFER_SIZE;
        uint8_t receivedBytes = uartToM5Stack.read(receivedBuffer, charsAvailable);
        //Serial.printf("RS received bytes = %d : ", receivedBytes);
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
    iChar++;
    _currentState.hvacWiFiSSID = "";
    while (iChar < receivedBytes){

//        Serial.printf("%02X ", receivedBuffer[iChar]);

        if (receivedBuffer[iChar] == ETX && posInPacket == 22)
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

        posInPacket++;
        iChar++;
    }
  //  Serial.print("\n");

    if (iChar == receivedBytes) //no ETX
        return 0;
    return 1;
}

void rsSendSetDACVoltage(uint16_t setVoltage){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];

    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = HVAC_CMD_SETVOLTAGE;
    rsSendBuffer[2] = (uint8_t)(setVoltage >> 8);
    rsSendBuffer[3] = (uint8_t)(setVoltage);
    rsSendBuffer[4] = ETX;
/*    Serial.printf("rsSendSetDACVoltage = %d\n", setVoltage);
    for (int i = 0; i < RS_BUFFER_SIZE; i++)
        Serial.printf("%02X ", rsSendBuffer[i]);
    Serial.print("\n");
 */    uartToM5Stack.write(rsSendBuffer, RS_BUFFER_SIZE);
}

void rsSendSetHCState(uint8_t hcState){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];
    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = HVAC_CMD_COOL_HEAT;
    rsSendBuffer[2] = 0;
    rsSendBuffer[3] = hcState;
    rsSendBuffer[4] = ETX;
    uartToM5Stack.write(rsSendBuffer, RS_BUFFER_SIZE);
}

void rsSendSetFlexitFanSpeed(uint8_t fxFanSpeed){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];
    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = HVAC_CMD_SETFANSPEED;
    rsSendBuffer[2] = 0;
    rsSendBuffer[3] = fxFanSpeed;
    rsSendBuffer[4] = ETX;
    uartToM5Stack.write(rsSendBuffer, RS_BUFFER_SIZE);
}

void rsSendSetRoofLight(uint8_t roofLight){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];
    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = HVAC_CMD_ROOFLIGHT;
    rsSendBuffer[2] = 0;
    rsSendBuffer[3] = roofLight;
    rsSendBuffer[4] = ETX;
    uartToM5Stack.write(rsSendBuffer, RS_BUFFER_SIZE);
}


void rsSendGetCurrentState(){
    uint8_t rsSendBuffer[RS_BUFFER_SIZE];
    rsSendBuffer[0] = STX;
    rsSendBuffer[1] = HVAC_CMD_GETCURRENTDATA;
    rsSendBuffer[2] = 0;
    rsSendBuffer[3] = 0;
    rsSendBuffer[4] = ETX;
    uartToM5Stack.write(rsSendBuffer, RS_BUFFER_SIZE);
}
