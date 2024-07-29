#include "dscKeybusInterface.h"
#include "extractorEventos.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <cppQueue.h>

// Configures the Keybus interface with the specified pins - dscWritePin is optional, leaving it out disables the
// virtual keypad.
#define dscClockPinD D1  // GPIO 5
#define dscReadPinD D2   // GPIO 4
#define dscPC16PinD D7   // DSC Classic Series only, GPIO 13
#define dscWritePinD D8  // GPIO 15

// Initialize components
#ifndef dscClassicSeries
extractorEventos dsc(dscClockPinD, dscReadPinD, dscWritePinD);
#endif

void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}
