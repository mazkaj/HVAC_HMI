#ifndef __SERIALDATAM5_H
	#define	__SERIALDATAM5_H


#define RS_BUFFER_SIZE  6

#define STX 0x02
#define ETX 0x03


void initSerialDataM5Stack();
void processDataFromHMI();
void analizeReceivedData(uint8_t *receivedBuffer, uint8_t receivedBytes);
void sendHVACStateToHMI();

#endif //__SERIALDATAM5_H