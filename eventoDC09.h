#ifndef EVENTO_DC09_H
#define EVENTO_DC09_H

#include "evexStruct.h"
#include <cstdio>
#include <cstring>
#include <stdlib.h>

class eventoDC09 {
  public:
    static char * genDC09String(int type, EventoStruct evento, int sequence, char * account);
    static bool checkDC09CRC(char * message);
    static char * genCRCStamp(char * entradaString);
    static bool parseDC09(char * mensaje, int * secuencia, char * token);

  protected:
    static unsigned int calcCRC(unsigned CRC, int ch);// CALC DC09 CRC
};

extern eventoDC09 EventoDC09;

#endif