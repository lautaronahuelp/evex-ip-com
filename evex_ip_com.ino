#include "dscKeybusInterface.h"
#include "extractorEventos.h"
#include "eventoDC09.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>


// PINES INTERFAZ KEYBUS
#define dscClockPinD D1  // GPIO 5
#define dscReadPinD D2   // GPIO 4
#define dscPC16PinD D7   // DSC Classic Series only, GPIO 13
#define dscWritePinD D8  // GPIO 15


#ifndef dscClassicSeries
extractorEventos dsc(dscClockPinD, dscReadPinD, dscWritePinD);
#endif

void setup() {
  
}

void loop() {
  // CONTROL LED

  // LOOP DSC EVENTOS Y ACCESO REMOTO

  // CONEXION ACCESO REMOTO

  // PROGRAMACION PARAMETROS

  // REPORTE EVENTOS

  // CONTACTO CON DRIVER

  // ACTUALIZACION OTA
}
