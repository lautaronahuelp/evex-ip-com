#include "extractorEventos.h"

//Extrae numeros de zonas
int extractorEventos::exNumberOffset(byte panelByte, int numberOffset) {
  int number = panelData[panelByte] + numberOffset;
  return number;
}


//  Extrae codigos de acceso como int para exPanelStatus0-1B
int extractorEventos::exPanelAccessCode(byte dscCode, bool accessCodeIncrease) {

  if (accessCodeIncrease) {
    if (dscCode >= 35) dscCode += 5;
  } else {
    if (dscCode >= 40) dscCode += 3;
  }

  return (int)dscCode;
}

// Extrae bits como int para particiones y zonas
int extractorEventos::exPanelBitNumbers(byte panelByte, byte startNumber, byte startBit, byte stopBit, bool printNone) {
  int numBit;
  if (printNone && panelData[panelByte] == 0) numBit = 0;
  else {
    byte bitCount = 0;
    for (byte bit = startBit; bit <= stopBit; bit++) {
      if (bitRead(panelData[panelByte], bit)) {
        numBit = startNumber + bitCount;
      }
      bitCount++;
    }
  }
  return numBit;
}
