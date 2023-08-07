#ifndef __SERIALDATAM5_H
	#define	__SERIALDATAM5_H


#define RS_BUFFER_SIZE  6
#define RS_HVACBUFFER_SIZE  22

#define STX 0x02
#define ETX 0x03


void initSerialDataM5Stack();
uint8_t processDataFromHVAC();
uint8_t analizeReceivedData(uint8_t *receivedBuffer, uint8_t receivedBytes);
void rsSendSetDACVoltage(uint16_t setVoltage);
void rsSendSetHCState(uint8_t hcState);
void rsSendGetCurrentState();

#endif //__SERIALDATAM5_H