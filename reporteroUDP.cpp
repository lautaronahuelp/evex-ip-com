#include "reporteroUDP.h"

reporteroUDP::reporteroUDP(char* servidor1, char* servidor2, int puerto1, int puerto2, char* cuenta, int hb, int largo, int int1, int int2) {
  strcpy(&servMon[0][0], servidor1);
  strcpy(&servMon[1][0], servidor2);
  strcpy(cuentaMon, cuenta);
  puerMon[0] = puerto1;
  puerMon[1] = puerto2;
  hbMon = hb;
  largoCola = largo;
  maxIntentos[0] = int1;
  maxIntentos[1] = int2;
}

void reporteroUDP::agregarEvento(EventoStruct evento) {
  colaMon.push(&evento);
}

void reporteroUDP::iniciar(int puertoLocal) {
  UDP.begin(puertoLocal);
  UDP.flush();
}

void reporteroUDP::loop() {
  static bool aumSecEnFalla = true;
  int intervalo = 1000, numServ = 0;
  EventoStruct peekEvento;
  bool fallaCom = hayFallaCom();
  if (numIntentos[numServ] == maxIntentos[numServ]) {//rota el numero de servidor antes de la falla, falta que rote en la falla
    switch (numServ) {
      case 0:
        numServ = 1;
        break;
      case 1:
        numServ = 0;
        break;
    }
  }

  if (recibirAck()) {
    if (!fallaCom) colaMon.drop();
    tiemUltAck = millis();
    aumSecEnFalla = true;
    numIntentos[0] = 0;  //TEMA ULTIMO SERVIDOR Q RESPONDIO
    numIntentos[1] = 0;
  }

  if (!fallaCom) {
    if (millis() - tiemUltIntento > intervalo) {
      if (!(colaMon.isEmpty())) {
        colaMon.peek(&peekEvento);
        enviarEvento(1, peekEvento);
        tiemUltIntento = millis();
        numIntentos[numServ]++;
      } else if (millis() - tiemUltAck > hbMon) {
        enviarEvento(0, peekEvento);
        tiemUltIntento = millis();
        numIntentos[numServ]++;
      }
    }
  }

  if (fallaCom) {
    if (aumSecEnFalla) {
      aumentarSecuencia();
      aumSecEnFalla = false;
    }
    if (millis() - tiemUltIntento > intervaloEnFalla) {
      enviarEvento(0, peekEvento);
      tiemUltIntento = millis();
    }
  }


}

void reporteroUDP::enviarEvento(int tipo, EventoStruct evento) {
  int iniPaquete = 0, finPaquete = 0;
  //falta implementar rotacion de servidores
  iniPaquete = UDP.beginPacket(servMon[0], puerMon[0]);
  UDP.write(EventoDC09.genDC09String(tipo, evento, secuenMon, cuentaMon));
  finPaquete = UDP.endPacket();
}

bool reporteroUDP::recibirAck() {
  char paqueteEntrante[50];
  int sizePaquete = UDP.parsePacket();
  bool ack = false;
  char salida[4];
  char seqRec[5];
  char tokens[2][4] = { "ACK", "ack" };

  if (sizePaquete) {
    int len = UDP.read(paqueteEntrante, 50);
    if (len > 0) {
      paqueteEntrante[len] = 0;
    }

    if (EventoDC09.checkDC09CRC(&paqueteEntrante[1])) {
      strncpy(salida, &paqueteEntrante[10], 3);
      salida[3] = 0;

      if (strcmp(salida, tokens[0]) == 0) {
        ack = true;
      } else if (strcmp(salida, tokens[1]) == 0) {
        ack = true;
      }

      if (ack) {
        strncpy(seqRec, &paqueteEntrante[14], 4);

        if (atoi(seqRec) == secuenMon) {
          //si la secuencia es ok y el string de secuencia es invalido-> lo toma valido
          (secuenMon)++;
        } else {
          ack = false;
        }
      }
      return ack;
    }
  }
  return false;
}

bool reporteroUDP::hayFallaCom() {
  return (numIntentos[0] == maxIntentos[0]) && (numIntentos[1] == maxIntentos[1]);
}

bool reporteroUDP::reiniciarCom() {
  if (hayFallaCom()) {
    numIntentos[0] = 0;
    numIntentos[1] = 0;
    return true;
  } else {
    return false;
  }
}

int reporteroUDP::aumentarSecuencia() {
  secuenMon++;
  if (secuenMon > 9999) secuenMon = 0;
  return secuenMon;
}