#include "eventoDC09.h"

char * eventoDC09::genDC09String(int type, EventoStruct evento, int sequence, char * account){
  static char salida[50];
  char qualifier[] = "13";
  char salidaSinStamp[44];
  
  switch(type){
    case 0:
        snprintf(salidaSinStamp, 44, "\n\"NULL\"0000R0L0#%s[]\r", account);// NULL TIENE SECUENCIA 0000
      break;
    case 1:
        //"ADM-CID"SSSSR0L0#CCCC[#CCCC|QEEE PP ZZZ] <- FORMATO CID
        snprintf(salidaSinStamp, 44, "\n\"ADM-CID\"%04dR0L0#%s[#%s|%c%d %02d %03d]\r", sequence, account, account, qualifier[evento.q], evento.eve, evento.part
          , evento.zona);
      break;
    default:
      salida[0] = 0;
      return salida;
  }
  snprintf(salida, 50, "%s%s", genCRCStamp(salidaSinStamp), salidaSinStamp);
  
  return salida;
}

bool eventoDC09::checkDC09CRC(char * message) {
  char entrada[21], salida[50], token[9];
  strcpy(salida, message);
  strcpy(entrada, strtok(&salida[8], "\r"));
  strcpy(token, strtok(salida, "\""));

  if (strcmp(token, genCRCStamp(entrada)) == 0) {
    return true;
  }
  return false;
}

char * eventoDC09::genCRCStamp(char * entradaString)
{
 static char stamp[9];
 unsigned int CRC; /* 16 BIT CRC RESULT */
 int count, ch;
 
 CRC = 0;
 count = 0;

 while (ch = *entradaString++)
 {
  CRC = calcCRC(CRC, ch); /* CALL CRC FUNCTION BELOW */
  ++count;

 } 
 
  sprintf(stamp,"%04X%04X", CRC, count);

  return stamp;
}

unsigned int eventoDC09::calcCRC(unsigned CRC, int ch)
{
 int i;
 unsigned char temp;
 temp = (unsigned char)ch; /* TREAT LOCALLY AS UNSIGNED */
 for (i = 0; i < 8; i++) /* DO 8 BITS */
 {
 temp ^= CRC & 1; /* PROCESS LSB */
 CRC >>= 1; /* SHIFT RIGHT */
 if (temp & 1)
 CRC ^= 0xA001; /* IF LSB SET,ADD FEEDBACK */
 temp >>= 1; /* GO TO NEXT BIT */
 }
 return CRC;
}