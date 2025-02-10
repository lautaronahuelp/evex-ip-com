#ifndef REP_UDP_H
#define REP_UDP_H

#include <Arduino.h>
#include "evexStruct.h"
#include "eventoDC09.h"
#include <WiFiUdp.h>
#include <cppQueue.h>

class reporteroUDP {
  public:
    reporteroUDP(char * servidor1, char * servidor2, int puerto1, int puerto2, char * cuenta, int hb = 5, int largo = 20, int int1 = 4, int int2 = 4);
    void iniciar(int puertoLocal);
    void loop();
    void agregarEvento(EventoStruct evento);
    bool reiniciarCom();
    bool hayFallaCom();

  protected:
    int largoCola = 20, puerMon[2], secuenMon, hbMon, numIntentos[2] = { 0, 0 }, maxIntentos[2] = { 4, 4 };
    int timeOutReportes = 1000;
    int timeOutEnFalla = 60000; //hb cada 1 minuto y medio para testear red
    int numServ = 0;
    bool hbEnProceso = true, comHabilitada =  true; //se esta intentando reportar un HB
    unsigned long tiemUltAck = 0, tiemUltIntento = 0;
    char servMon[2][51], cuentaMon[5];

    cppQueue colaMon = cppQueue(sizeof(EventoStruct), largoCola);
    WiFiUDP UDP;

    void enviarEvento(int tipo, EventoStruct evento);
    bool recibirAck();
    void reintentarCom();

    int aumentarSecuencia();
    bool secuenciaEsCorrecta(int secuencia);
    bool intentosExcedidos();
    void intercambioServidores();
    
};

#endif