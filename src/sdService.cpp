#include "SD.h"
#include <main.h>
#include <sdService.h>

extern nodeConfig_t _nodeConfig;

File _confFile;

void sdInitialize(){
    SD.begin(4);
}

void readConfiguration(){
    _confFile = SD.open("/registers.txt", FILE_READ);
    if (_confFile){
        char readLineBuff[60];
        char addressStr[3];
        String readLineStr;
        readLineStr.reserve(60);
        String addrString;
        String valueString;
        uint8_t sepDescription = readLineStr.indexOf(';');
        uint8_t sepStartValue = readLineStr.indexOf(';', sepDescription + 1);
        uint8_t sepEndValue = readLineStr.indexOf(';', sepStartValue + 1);
        uint8_t address;
        uint32_t paramValue;
        while(_confFile.available()){
            readLineStr = _confFile.readStringUntil('\n');
            sepDescription = readLineStr.indexOf(';');
            sepStartValue = readLineStr.indexOf(';', sepDescription + 1);
            sepEndValue = readLineStr.indexOf(';', sepStartValue + 1);
            addrString = readLineStr.substring(1, sepDescription);
            valueString = readLineStr.substring(sepStartValue + 1, sepEndValue);
            address = addrString.toInt();
            Serial.print(addrString);
            switch (address)
            {
            case 0:
                _nodeConfig.configuration = valueString.toInt();
                Serial.printf(" config = %d \r\n", _nodeConfig.configuration);
                break;
            case 1:
                _nodeConfig.tresholdOn = valueString.toInt();
                Serial.printf(" treshON = %d \r\n", _nodeConfig.tresholdOn);
                break;
            case 3:
                _nodeConfig.tresholdOff = valueString.toInt();
                Serial.printf(" treshOFF = %d \r\n", _nodeConfig.tresholdOff);
                break;
            case 5:
                _nodeConfig.hourOn = valueString.toInt();
                Serial.printf(" treshON = %d \r\n", _nodeConfig.hourOn);
                break;
            case 6:
                _nodeConfig.hourOff = valueString.toInt();
                Serial.printf(" treshOFF = %d \r\n", _nodeConfig.hourOff);
                break;

            default:
                break;
            }
            Serial.println();
        }
        _confFile.close();
        showNodeName();
    }
    else{
        Serial.printf("SD ERROR!!! - Can`t read registers.txt file ... ");
        Serial.printf("Using default configration \n");
        setFlagConfig(CONFBIT_ROOFLIGHT);
        _nodeConfig.tresholdOn = 5;
        _nodeConfig.tresholdOff = 10;
        _nodeConfig.hourOn = 132;
        _nodeConfig.hourOff = 36;
    }
}

uint8_t storeConfigurationToSD(uint8_t *registers){
    _confFile = SD.open("/registers.txt", FILE_WRITE, true);
    if (_confFile){
        writeConfig(_confFile, registers[eTcpPacketPosStartPayLoad + 2]);
        _confFile.close();
        return registers[eTcpPacketPosStartPayLoad + 1];
    }
    else
        return 0;
}

void writeConfig(fs::File _confFile, uint8_t confValue){
    _confFile.print("{000;Configuration;");
    _confFile.print(confValue);
    _confFile.print(";");
    for (uint8_t iBit = 0; iBit < 8; iBit++){
        if ((confValue & (1 << iBit)))
            _confFile.print("[X] ");
        else
            _confFile.print("[ ] ");
    }
    _confFile.println("}");
}

uint8_t putShortToSendBuff(uint8_t *sendBuff, uint16_t shortToPut, uint8_t posInBuff){
    sendBuff[posInBuff++] = (shortToPut & 0xFF);
    sendBuff[posInBuff++] = ((shortToPut >> 8) & 0xFF);
	return posInBuff;
}

