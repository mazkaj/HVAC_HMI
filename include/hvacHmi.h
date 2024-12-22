#ifndef __HVACHMI_H
	#define	__HVACHMI_H

void sendCurrentStateToServer();
void sendCurrentState(byte recipientAddress0, byte recipientAddress1);
void processTcpDataReq(uint8_t *receivedBuffer);

void controlRoofLight();
void initDS18B20();
void dsReqTemperature();
bool dsGetTemperature();
void initSHT40();
void shtGetParameters();
void adjustTemperature();
void setRoofLight(uint8_t roofLightMode);

#endif
