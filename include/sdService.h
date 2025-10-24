
#define NUMBER_OF_REGISTER  15

// {000;Configuration;0;[ ] [ ] [ ] [ ] [ ] [ ] [ ] [ ] }
// {001;Treshold ON;200; 0x00 0xc8 }
// {003;Treshold OFF;2000; 0x07 0xd0 }
// {005;Nazwa;Lampy front         }
#include "main.h"

void sdInitialize();
void readConfiguration();
uint8_t storeConfigurationToSD(uint8_t *registers);
void writeConfig(fs::File _confFile, uint8_t confValue);
uint8_t putShortToSendBuff(uint8_t *sendBuff, uint16_t shortToPut, uint8_t posInBuff);