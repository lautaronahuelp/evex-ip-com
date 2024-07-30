#ifndef REP_UDP_H
#define REP_UDP_H

#include "evexStruct.h"
#include <WiFiUdp.h>
#include <cppQueue.h>

class reporteroUDP {
  public:
    reporteroUDP(char * servidor1, char * servidor2, int puerto1, int puerto2, char * cuenta, int largo, int hb);
    void iniciar(int puertoLocal);
    void loop();
    void agregarEvento(EventoStruct evento);

  protected:
    int largoCola = 20, puerMon1, puerMon2, secuenMon, hbMon;
    unsigned long ultimoAck = 0;
    bool ack = false;
    char servMon1[51], servMon2[51], cuentaMon[5];

    cppQueue colaMon = cppQueue(sizeof(EventoStruct), largoCola);
    WiFiUDP UDP;

    void enviarMensaje(int type, EventoStruct evento, int sequence, char * account);
    void recibirACK(bool * ackg, int * sequence);
    
};

#endif