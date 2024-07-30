#ifndef EVEX_H
#define EVEX_H

#include "evexStruct.h"
#include "dscKeybus.h"

class extractorEventos : public dscKeybusInterface {
  public:
    extractorEventos(byte setClockPin, byte setReadPin, byte setWritePin = 255) : dscKeybusInterface(setClockPin, setReadPin, setWritePin){};
    int extraerComandoPanel(EventoStruct * evento, ConfigStruct * config);

  protected:
    bool exPanel_0xA5(EventoStruct * evento);
    bool exPanel_0xEB(EventoStruct * evento);
    bool exPanel_0x16(ConfigStruct * configuracion);
    bool exPanelStatus0(byte panelByte, EventoStruct * evento);
    bool exPanelStatus1(byte panelByte, EventoStruct * evento);
    bool exPanelStatus2(byte panelByte, EventoStruct * evento);
    bool exPanelStatus3(byte panelByte, EventoStruct * evento);
    bool exPanelStatus4(byte panelByte, EventoStruct * evento);
    bool exPanelStatus5(byte panelByte, EventoStruct * evento);
    bool exPanelStatus16(byte panelByte, EventoStruct * evento);
    bool exPanelStatus1B(byte panelByte, EventoStruct * evento);
    void exNumberSpace(byte number);
    int exPanelBitNumbers(byte panelByte, byte startNumber, byte startBit = 0, byte stopBit = 7, bool printNone = true);
    int exNumberOffset(byte panelByte, int numberOffset);
    int exPanelAccessCode(byte dscCode, bool accessCodeIncrease = true);
    void exPartition();
    void exPanelTime(byte panelByte);

    void setWriteKey(const char receivedKey);
};

#endif