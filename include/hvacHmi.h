#ifndef __HVACHMI_H
	#define	__HVACHMI_H

void updateCurrentInpState();
void sendCurrentStateToPAN();
void sendCurrentState(byte recipientAddress0, byte recipientAddress1);
void processTcpDataReq(uint8_t *receivedBuffer);
#endif
