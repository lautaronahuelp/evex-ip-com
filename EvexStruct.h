#ifndef EVST_H
#define EVST_H
typedef struct eventoSt EventoStruct;
typedef struct configSt ConfigStruct;

struct eventoSt{
  unsigned long int q: 1;// CALIFICADOR 0: E/1 || 1: R/3
  unsigned long int eve: 10;
  unsigned long int part: 4;
  unsigned long int zona: 7;
  unsigned long int pad: 10;
};

struct configSt{
  unsigned long int version1: 3;// VERSION DIGITO 1
  unsigned long int version2: 3;// VERSION DIGITO 2
  unsigned long int cableado: 2;// 0:NC 1:EOL 2:DEOL
  bool codigos;// LARGO CODIGOS true:4 digitos false:6 digitos
  bool programando;
};

#endif