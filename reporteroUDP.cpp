#include "reporteroUDP.h"

void reporteroUDP::agregarEvento(EventoStruct evento) {
  colaMon.push(&evento);
}

void reporteroUDP::iniciar(int puertoLocal) {
  UDP.begin(puertoLocal);
  UDP.flush();
}

void reporteroUDP::loop() {
  EventoStruct peekEvento;
  receiveACK(&ack, &secuenMon);

  if (ack) {
    colaMon.drop();
    ack = false;
    ultimoAck = millis();
  }

  if (!(colaMon.isEmpty())) {
    colaMon.peek(&peekEvento);
    sendMessage(1, peekEvento, secuenMon, cuentaMon);
  } else if (millis() - ultimoAck > hbMon) {
    sendMessage(0, peekEvento, secuenMon, cuentaMon);
  }
}