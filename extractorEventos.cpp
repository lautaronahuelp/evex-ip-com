#include "extractorEventos.h"

int extractorEventos::extraerComandoPanel(EventoStruct* evento, ConfigStruct* configuracion) {
  bool decoded[2] = { false, false };  //{evento, configuracion}

  evento->q = 0;
  evento->eve = 0;
  evento->part = 0;
  evento->zona = 0;
  evento->pad = 0;

  // Checks for errors on panel commands with CRC data
  switch (panelData[0]) {
    case 0x05:  // Skips panel commands without CRC data
    case 0x11:
    case 0x1B:
    case 0x1C:
    case 0x22:
    case 0x28:
    case 0x33:
    case 0x39:
    case 0x41:
    case 0x4C:
    case 0x57:
    case 0x58:
    case 0x70:
    case 0x94:
    case 0x9E:
    case 0xD5:
    case 0xE6: break;  // 0xE6 is checked separately as only some of its subcommands use CRC
    default:
      {  // Checks remaining panel commands
        if (!validCRC()) {
          return false;
        }
      }
  }

  // Processes known panel commands from the first byte of the panel message: panelData[0]
  switch (panelData[0]) {
    case 0x16: decoded[1] = exPanel_0x16(configuracion); break;  // Panel configuration, VERSION | Structure: *incomplete | Content: *incomplete
    case 0xA5:
      if (configuracion->version1 < 4) decoded[0] = exPanel_0xA5(evento);
      break;    // Date, time, system status messages - partitions 1-2 | Structure: *incomplete | Content: *incomplete
    case 0xEB:  //if(configuracion->version1 >= 4) decoded[0] = exPanel_0xEB(evento); break;// Date, time, system status messages - partitions 1-8 | Structure: *incomplete | Content: *incomplete
    default:
      break;
  }

  if (decoded[0]) return 1;
  else if (decoded[1]) return 2;

  return 0;
}

bool extractorEventos::exPanel_0x16(ConfigStruct* configuracion) {

  if (panelData[2] == 0x0E) {

    // Panel version
    configuracion->version1 = panelData[3] >> 4;
    configuracion->version2 = panelData[3] & 0x0F;

    switch (panelData[4] & 0x03) {
      case 0x01: configuracion->cableado = 0; break;  // "NC"
      case 0x02: configuracion->cableado = 1; break;  // "EOL"
      case 0x03: configuracion->cableado = 2; break;  // "DEOL"
    }

    // Code length
    if (panelData[4] & 0x08) configuracion->codigos = false;
    else configuracion->codigos = true;

    // *8 programming mode status
    if (panelData[4] & 0x10) configuracion->programando = false;
    else configuracion->programando = true;
  }

  return true;
}

bool extractorEventos::exPanel_0xA5(EventoStruct* evento) {
  int particion = 0;
 
  bool decoded = true;

  if (panelData[6] == 0 && panelData[7] == 0) {
    return false;
  }

  switch (panelData[3] >> 6) {
    case 0x01: particion = 1; break;
    case 0x02: particion = 2; break;
  }

  switch (panelData[5] & 0x03) {
    case 0x00: decoded = exPanelStatus0(6, evento); break;
    case 0x01: decoded = exPanelStatus1(6, evento); break;
    case 0x02: decoded = exPanelStatus2(6, evento); break;
    case 0x03: decoded = exPanelStatus3(6, evento); break;
    default: decoded = false; break;
  }

  evento->part = particion;
  if (decoded) {
    stream->print("(EVENTO A5 EXTRAIDO): ");
    switch (evento->q) {
      case 0:
        stream->print("1");
        break;
      case 1:
        stream->print("3");
        break;
    }
    stream->print(evento->eve);
    stream->print("0");
    stream->print(evento->part);
    stream->print("0");
    if (evento->zona < 10) stream->print("0");
    stream->println(evento->zona);
  }

  return decoded;
}

bool extractorEventos::exPanel_0xEB(EventoStruct* evento) {
  int particion = 0;
  bool decoded = true;

  if (panelData[2] == 0) particion = 0;
  else {
    particion = exPanelBitNumbers(2, 1);
  }

  switch (panelData[7]) {
    case 0x00: decoded = exPanelStatus0(8, evento); break;
    case 0x01: decoded = exPanelStatus1(8, evento); break;
    case 0x02: decoded = exPanelStatus2(8, evento); break;
    case 0x03: decoded = exPanelStatus3(8, evento); break;
    case 0x04: decoded = exPanelStatus4(8, evento); break;
    case 0x05: decoded = exPanelStatus5(8, evento); break;
    case 0x16: decoded = exPanelStatus16(8, evento); break;
    case 0x1B: decoded = exPanelStatus1B(8, evento); break;
    default: decoded = false; break;
  }

  evento->part = particion;
  if (decoded) {
    stream->print("(EVENTO EB EXTRAIDO): ");
    switch (evento->q) {
      case 0:
        stream->print("1");
        break;
      case 1:
        stream->print("3");
        break;
    }
    stream->print(evento->eve);
    stream->print("0");
    stream->print(evento->part);
    stream->print("0");
    if (evento->zona < 10) stream->print("0");
    stream->println(evento->zona);
  }

  return decoded;
}

bool extractorEventos::exPanelStatus0(byte panelByte, EventoStruct* evento) {
  bool decoded = true;
  evento->q = 0;
  switch (panelData[panelByte]) {
    // 0x09 - 0x28: Zone alarm, zones 1-32
    // 0x29 - 0x48: Zone alarm restored, zones 1-32
    case 0x49: evento->eve = 121; break;  //1121// "Duress alarm"
    case 0x4A: evento->eve = 406; break;  //1406// "Disarmed: Alarm memory"
    case 0x4B: evento->eve = 459; break;  //1459// "Recent closing alarm"
    case 0x4C: evento->eve = 330; break;  //1330// "Zone expander supervisory alarm"
    case 0x4D:
      evento->q = 1;
      evento->eve = 330;
      break;                              //3330// "Zone expander supervisory restored"
    case 0x4E: evento->eve = 110; break;  //1110// "Keypad Fire alarm"
    case 0x4F: evento->eve = 100; break;  //1100// "Keypad Aux alarm"
    case 0x50: evento->eve = 120; break;  //1120// "Keypad Panic alarm"
    case 0x51: decoded = false; break;    // "PGM2 input alarm"
    case 0x52:
      evento->q = 1;
      evento->eve = 110;
      break;  //3110// "Keypad Fire alarm restored"
    case 0x53:
      evento->q = 1;
      evento->eve = 100;
      break;  //3100// "Keypad Aux alarm restored"
    case 0x54:
      evento->q = 1;
      evento->eve = 120;
      break;  //3120// "Keypad Panic alarm restored"
    case 0x55:
      decoded = false;
      break;  // "PGM2 input alarm restored"
    // 0x56 - 0x75: Zone tamper, zones 1-32
    // 0x76 - 0x95: Zone tamper restored, zones 1-32
    case 0x98:
      evento->eve = 461;
      break;  //1461// "Keypad lockout"
    // 0x99 - 0xBD: Armed: Access codes 1-34, 40-42
    case 0xBE:
      evento->q = 1;
      evento->eve = 456;
      break;  //3456// "Armed: Partial"
    case 0xBF:
      evento->q = 1;
      evento->eve = 400;
      break;  //3400// "Armed: Special"
    // 0xC0 - 0xE4: Disarmed: Access codes 1-34, 40-42
    case 0xE5: evento->eve = 455; break;  //1455// "Auto-arm cancelled"
    case 0xE6: evento->eve = 400; break;  //1400// "Disarmed: Special"
    case 0xE7: evento->eve = 302; break;  //1302// "Panel battery trouble"
    case 0xE8: evento->eve = 301; break;  //1301// "Panel AC power trouble"
    case 0xE9: evento->eve = 321; break;  //1321// "Bell trouble"
    case 0xEA: evento->eve = 373; break;  //1373// "Fire zone trouble"
    case 0xEB: evento->eve = 312; break;  //1312// "Panel aux supply trouble"
    case 0xEC: evento->eve = 351; break;  //1351// "Telephone line trouble"
    case 0xEF:
      evento->q = 1;
      evento->eve = 302;
      break;  //3302// "Panel battery restored"
    case 0xF0:
      evento->q = 1;
      evento->eve = 301;
      break;  //3301// "Panel AC power restored"
    case 0xF1:
      evento->q = 1;
      evento->eve = 321;
      break;  //3321// "Bell restored"
    case 0xF2:
      evento->q = 1;
      evento->eve = 373;
      break;  //3373// "Fire zone trouble restored"
    case 0xF3:
      evento->q = 1;
      evento->eve = 312;
      break;  //3312// "Panel aux supply restored"
    case 0xF4:
      evento->q = 1;
      evento->eve = 351;
      break;  //3351// "Telephone line restored"
    case 0xF7:
      evento->q = 1;
      evento->eve = 354;
      evento->zona = 1;
      break;  //3354Z1// "Phone 1 FTC"
    case 0xF8:
      evento->q = 1;
      evento->eve = 354;
      evento->zona = 2;
      break;                              //3354Z2// "Phone 2 FTC"
    case 0xF9: evento->eve = 622; break;  //1622// "Event buffer threshold"//"75% full since last DLS upload"
    case 0xFA: evento->eve = 411; break;  //1411// "DLS lead-in"
    case 0xFB: evento->eve = 412; break;  //1412// "DLS lead-out"
    case 0xFE: evento->eve = 602; break;  //1602// "Periodic test transmission"
    case 0xFF: evento->eve = 601; break;  //1601// "System test"
    default: decoded = false;
  }
  if (decoded) return true;

  /*
   *  Zone alarm, zones 1-32
   *
*/
  if (panelData[panelByte] >= 0x09 && panelData[panelByte] <= 0x28) {
    //1130// "Zone alarm: "

    evento->eve = 130;
    evento->zona = exNumberOffset(panelByte, -8);

    return true;
  }

  /*
   *  Zone alarm restored, zones 1-32
   *
*/
  if (panelData[panelByte] >= 0x29 && panelData[panelByte] <= 0x48) {
    //3130// "Zone alarm restored: "
    evento->q = 1;
    evento->eve = 130;
    evento->zona = exNumberOffset(panelByte, -40);
    return true;
  }

  /*
   *  Zone tamper, zones 1-32
   *
*/
  if (panelData[panelByte] >= 0x56 && panelData[panelByte] <= 0x75) {
    //1144// "Zone tamper: "
    evento->eve = 144;
    evento->zona = exNumberOffset(panelByte, -85);
    return true;
  }

  /*
   *  Zone tamper restored, zones 1-32
   *
*/
  if (panelData[panelByte] >= 0x76 && panelData[panelByte] <= 0x95) {
    //3144// "Zone tamper restored: "
    evento->q = 1;
    evento->eve = 144;
    evento->zona = exNumberOffset(panelByte, -117);
    return true;
  }

  /*
   *  Armed by access codes 1-34, 40-42
*/
  if (panelData[panelByte] >= 0x99 && panelData[panelByte] <= 0xBD) {
    byte dscCode = panelData[panelByte] - 0x98;
    //3401// "Armed: "

    evento->q = 1;
    evento->eve = 401;
    evento->zona = exPanelAccessCode(dscCode);

    return true;
  }

  /*
   *  Disarmed by access codes 1-34, 40-42
   *
*/
  if (panelData[panelByte] >= 0xC0 && panelData[panelByte] <= 0xE4) {
    byte dscCode = panelData[panelByte] - 0xBF;
    //1401// "Disarmed: "

    evento->eve = 401;
    evento->zona = exPanelAccessCode(dscCode);

    return true;
  }

  return false;
}


bool extractorEventos::exPanelStatus1(byte panelByte, EventoStruct* evento) {
  bool decoded = true;
  evento->q = 0;
  switch (panelData[panelByte]) {
    case 0x03: evento->eve = 139; break;  //1139// "Cross zone alarm"
    case 0x04: evento->eve = 654; break;  //1654// "Delinquency alarm"
    case 0x05:
      evento->eve = 454;
      break;  //1454// "Late to close"
    // 0x24 - 0x28: Access codes 33-34, 40-42
    case 0x29: evento->eve = 411; break;  //1411// "Downloading forced answer"
    case 0x2B:
      evento->q = 1;
      evento->eve = 403;
      break;  //3403// "Armed: Auto-arm"
    // 0x2C - 0x4B: Zone battery restored, zones 1-32
    // 0x4C - 0x6B: Zone battery low, zones 1-32
    // 0x6C - 0x8B: Zone fault restored, zones 1-32
    // 0x8C - 0xAB: Zone fault, zones 1-32
    case 0xAC: evento->eve = 628; break;  //1628// "Exit installer programming"
    case 0xAD: evento->eve = 627; break;  //1627// "Enter installer programming"
    case 0xAE:
      evento->q = 1;
      evento->eve = 607;
      break;  //3607// "Walk test end"
    case 0xAF:
      evento->eve = 607;
      break;  //1607// "Walk test begin"
    // 0xB0 - 0xCF: Zones bypassed, zones 1-32
    case 0xD0:  // "Command output 4"
    case 0xD1:  // "Exit fault pre-alert"
    case 0xD2:  // "Armed: Entry delay"
    case 0xD3:  // "Downlook remote trigger"
    default: decoded = false;
  }
  if (decoded) return true;

  /*
   *  Access codes 33-34, 40-42
*/
  if (panelData[panelByte] >= 0x24 && panelData[panelByte] <= 0x28) {
    byte dscCode = panelData[panelByte] - 0x03;
    exPanelAccessCode(dscCode);
    return false;
  }

  /*
   *  Zone battery restored, zones 1-32
   */
  if (panelData[panelByte] >= 0x2C && panelData[panelByte] <= 0x4B) {
    //3384// "Zone battery restored: "
    evento->q = 1;
    evento->eve = 384;
    evento->zona = exNumberOffset(panelByte, -43);
    return true;
  }

  /*
   *  Zone low battery, zones 1-32
   */
  if (panelData[panelByte] >= 0x4C && panelData[panelByte] <= 0x6B) {
    //1384// "Zone battery low: "
    evento->eve = 384;
    evento->zona = exNumberOffset(panelByte, -75);
    return true;
  }

  /*
   *  Zone fault restored, zones 1-32
 */
  if (panelData[panelByte] >= 0x6C && panelData[panelByte] <= 0x8B) {
    //3380// "Zone fault restored: "
    evento->q = 1;
    evento->eve = 380;
    evento->zona = exNumberOffset(panelByte, -107);
    return true;
  }

  /*
   *  Zone fault, zones 1-32
   *
*/
  if (panelData[panelByte] >= 0x8C && panelData[panelByte] <= 0xAB) {
    //1380// "Zone fault: "
    evento->eve = 380;
    evento->zona = exNumberOffset(panelByte, -139);
    return true;
  }

  /*
   *  Zones bypassed, zones 1-32
   *
 */
  if (panelData[panelByte] >= 0xB0 && panelData[panelByte] <= 0xCF) {
    //1570// "Zone bypassed: "
    evento->eve = 570;
    evento->zona = exNumberOffset(panelByte, -175);
    return true;
  }

  return false;
}

bool extractorEventos::exPanelStatus2(byte panelByte, EventoStruct* evento) {
  bool decoded = true;
  evento->q = 0;
  switch (panelData[panelByte]) {

    case 0x2A: decoded = false; break;  // "Quick exit"
    case 0x63:
      evento->q = 1;
      evento->eve = 300;
      break;                              // "Keybus fault restored"
    case 0x64: evento->eve = 300; break;  // "Keybus fault"
    case 0x66:                            // "*1: Zone bypass"
    // 0x67 - 0x69: *7: Command output 1-3
    case 0x8C:                            // "Cold start"
    case 0x8D:                            // "Warm start"
    case 0x8E:                            // "Panel factory default"
    case 0x91: decoded = false; break;    // "Swinger shutdown"
    case 0x93: evento->eve = 409; break;  // "Disarmed: Keyswitch"
    case 0x96:
      evento->q = 1;
      evento->eve = 409;
      break;  //3409// "Armed: Keyswitch"
    case 0x97:
      evento->q = 1;
      evento->eve = 400;
      break;  //3400// "Armed: Keypad away"
    case 0x98:
      evento->q = 1;
      evento->eve = 408;
      break;                            //3408// "Armed: Quick-arm"
    case 0x99: decoded = false; break;  // "Activate stay/away zones"
    case 0x9A:
      evento->q = 1;
      evento->eve = 441;
      break;    //3441// "Armed: Stay"
    case 0x9B:  // "Armed: Away"
    case 0x9C:
      decoded = false;
      break;  // "Armed: No entry delay"
    // 0x9E - 0xC2: *1: Access codes 1-34, 40-42
    // 0xC3 - 0xC5: *5: Access codes 40-42
    // 0xC6 - 0xE5: Access codes 1-34, 40-42
    // 0xE6 - 0xE8: *6: Access codes 40-42
    // 0xE9 - 0xF0: Keypad restored: Slots 1-8
    // 0xF1 - 0xF8: Keypad trouble: Slots 1-8
    // 0xF9 - 0xFE: Zone expander restored: 1-6
    case 0xFF:
      evento->eve = 330;
      evento->zona = 9;
      break;  //1330 Z9// "Zone expander trouble: 1"
    default:
      decoded = false;
  }
  if (decoded) return true;
  /*
   *  *7: Command output 1-3
   */
  if (panelData[panelByte] >= 0x67 && panelData[panelByte] <= 0x69) {
    //stream->print(F("Command output: "));
    exNumberOffset(panelByte, -0x66);
    return false;
  }

  /*
   *  *1: Access codes 1-34, 40-42
   */
  if (panelData[panelByte] >= 0x9E && panelData[panelByte] <= 0xC2) {
    byte dscCode = panelData[panelByte] - 0x9D;
    //stream->print(F("*1: "));
    exPanelAccessCode(dscCode);
    return false;
  }

  /*
   *  *5: Access codes 40-42
   */
  if (panelData[panelByte] >= 0xC3 && panelData[panelByte] <= 0xC5) {
    byte dscCode = panelData[panelByte] - 0xA0;
    //stream->print(F("*5: "));
    exPanelAccessCode(dscCode);
    return false;
  }

  /*
   *  Access codes 1-32
   *
   */
  if (panelData[panelByte] >= 0xC6 && panelData[panelByte] <= 0xE5) {
    byte dscCode = panelData[panelByte] - 0xC5;
    exPanelAccessCode(dscCode);
    return false;
  }

  /*
   *  *6: Access codes 40-42
   */
  if (panelData[panelByte] >= 0xE6 && panelData[panelByte] <= 0xE8) {
    byte dscCode = panelData[panelByte] - 0xC3;
    //stream->print(F("*6: "));
    exPanelAccessCode(dscCode);
    return false;
  }

  /*
   *  Keypad restored: Slots 1-8
   *
  */
  if (panelData[panelByte] >= 0xE9 && panelData[panelByte] <= 0xF0) {
    //3330 ZONA 1 A 8// "Keypad restored: Slot "
    evento->q = 1;
    evento->eve = 330;
    evento->zona = exNumberOffset(panelByte, -232);
    return true;
  }

  /*
   *  Keypad trouble: Slots 1-8
   *
  */
  if (panelData[panelByte] >= 0xF1 && panelData[panelByte] <= 0xF8) {
    //1330 ZONA 1 A 8// "Keypad trouble: Slot "
    evento->eve = 330;
    evento->zona = exNumberOffset(panelByte, -240);
    return true;
  }

  /*
   *  Zone expander restored: 1-6
   */
  if (panelData[panelByte] >= 0xF9 && panelData[panelByte] <= 0xFE) {
    //1330 ZONAS 9 A 15// "Zone expander restored: "
    evento->q = 1;
    evento->eve = 330;
    evento->zona = exNumberOffset(panelByte, -248) + 8;
    return true;
  }

  return false;
}

bool extractorEventos::exPanelStatus3(byte panelByte, EventoStruct* evento) {
  bool decoded = true;
  evento->q = 0;
  switch (panelData[panelByte]) {
    case 0x05:
      evento->q = 1;
      evento->eve = 330;
      evento->zona = 16;
      break;  //3330 ZONA 16// "PC/RF5132: Supervisory restored"
    case 0x06:
      evento->eve = 330;
      evento->zona = 16;
      break;  //1330 ZONA 16// "PC/RF5132: Supervisory trouble"
    case 0x09:
      evento->q = 1;
      evento->eve = 330;
      evento->zona = 17;
      break;  //3330 ZONA 17// "PC5204: Supervisory restored"
    case 0x0A:
      evento->q = 0;
      evento->eve = 330;
      evento->zona = 17;
      break;  //1330 ZONA 17// PC5204: Supervisory trouble
    case 0x17:
      evento->q = 1;
      evento->eve = 330;
      evento->zona = 15;
      break;  //3330 ZONA 15// "Zone expander restored: 7"
    case 0x18:
      evento->q = 0;
      evento->eve = 330;
      evento->zona = 15;
      break;  //1330 ZONA 15// "Zone expander trouble: 7"
    // 0x25 - 0x2C: Keypad tamper restored, slots 1-8
    // 0x2D - 0x34: Keypad tamper, slots 1-8
    // 0x35 - 0x3A: Module tamper restored, slots 9-14
    // 0x3B - 0x40: Module tamper, slots 9-14
    case 0x41:
      evento->q = 1;
      evento->eve = 145;
      evento->zona = 16;
      break;  //3145 Z16// "PC/RF5132: Tamper restored"
    case 0x42:
      evento->eve = 145;
      evento->zona = 16;
      break;  //1145 Z16// "PC/RF5132: Tamper"
    case 0x43:
      evento->q = 1;
      evento->eve = 145;
      break;                              //3145 Z0// "PC5208: Tamper restored"
    case 0x44: evento->eve = 145; break;  //1145 Z0// "PC5208: Tamper"
    case 0x45:
      evento->q = 1;
      evento->eve = 145;
      evento->zona = 17;
      break;  //3145 Z17// "PC5204: Tamper restored"
    case 0x46:
      evento->eve = 145;
      evento->zona = 17;
      break;  //1145 Z17// "PC5204: Tamper"
    case 0x51:
      evento->q = 1;
      evento->eve = 145;
      evento->zona = 15;
      break;  //3145 Z15// "Zone expander tamper restored: 7"
    case 0x52:
      evento->eve = 145;
      evento->zona = 15;
      break;  //1145 Z15// "Zone expander tamper: 7"
    case 0xB3:
      evento->q = 1;
      evento->eve = 338;
      evento->zona = 17;
      break;  //3338 Z17// "PC5204: Battery restored"
    case 0xB4:
      evento->eve = 338;
      evento->zona = 17;
      break;  //1338 Z17// "PC5204: Battery trouble"
    case 0xB5:
      evento->q = 1;
      evento->eve = 337;
      evento->zona = 17;
      break;  //3337 Z17// "PC5204: Aux supply restored"
    case 0xB6:
      evento->eve = 337;
      evento->zona = 17;
      break;  //1337 Z17// "PC5204: Aux supply trouble"
    case 0xB7:
      evento->q = 1;
      evento->eve = 343;
      evento->zona = 17;
      break;  //3343 Z17// "PC5204: Output 1 restored"
    case 0xB8:
      evento->eve = 343;
      evento->zona = 17;
      break;                            //1343 Z17// "PC5204: Output 1 trouble"
    case 0xFF: decoded = false; break;  // "Extended status"
  }
  if (decoded) return true;
  /*
   *  Zone expander trouble: 2-6
   */
  if (panelData[panelByte] <= 0x04) {
    //1330 Z10 A Z14//"Zone expander trouble: "
    evento->eve = 330;
    evento->zona = exNumberOffset(panelByte, 2) + 8;
    return true;
  }

  /*
   *  Keypad tamper restored: 1-8
   */
  if (panelData[panelByte] >= 0x25 && panelData[panelByte] <= 0x2C) {
    //3145 Z1 A Z8//"Keypad tamper restored: "
    evento->q = 1;
    evento->eve = 145;
    evento->zona = exNumberOffset(panelByte, -0x24);
    return true;
  }

  /*
   *  Keypad tamper: 1-8
   */
  if (panelData[panelByte] >= 0x2D && panelData[panelByte] <= 0x34) {
    //1145 Z1 A Z8//"Keypad tamper: "
    evento->eve = 145;
    evento->zona = exNumberOffset(panelByte, -0x2C);
    return true;
  }

  /*
   *  Zone expander tamper restored: 1-6
   *
   */
  if (panelData[panelByte] >= 0x35 && panelData[panelByte] <= 0x3A) {
    //3145 Z9 A Z14//"Zone expander tamper restored: "
    evento->q = 1;
    evento->eve = 145;
    evento->zona = exNumberOffset(panelByte, -52) + 8;
    return true;
  }

  /*
   *  Zone expander tamper: 1-6
   *
   */
  if (panelData[panelByte] >= 0x3B && panelData[panelByte] <= 0x40) {
    //1145 Z9 A Z14//"Zone expander tamper: "
    evento->eve = 145;
    evento->zona = exNumberOffset(panelByte, -58) + 8;
    return true;
  }

  return decoded;
}

bool extractorEventos::exPanelStatus4(byte panelByte, EventoStruct* evento) {
  bool decoded = true;
  evento->q = 0;
  switch (panelData[panelByte]) {
    case 0x86: evento->eve = 608; break;  //1608// "Prueba periodica con problema"
    case 0x87: decoded = false; break;    //1457// "Falla al salir"//NO REPORTAR
    case 0x89: evento->eve = 406; break;  //1406// "Cancelacion de alarma"
    default:
      decoded = false;
      break;
  }
  if (decoded) return decoded;

  if (panelData[panelByte] <= 0x1F) {
    //1130// "Alarma zona: "
    evento->eve = 130;
    evento->zona = exNumberOffset(panelByte, 33);

    return true;
  }

  else if (panelData[panelByte] >= 0x20 && panelData[panelByte] <= 0x3F) {
    //3130// "Restauracion zona: "
    evento->q = 1;
    evento->eve = 130;
    evento->zona = exNumberOffset(panelByte, 1);

    return true;
  }

  else if (panelData[panelByte] >= 0x40 && panelData[panelByte] <= 0x5F) {
    //1144// "Tamper de zona: ");
    evento->eve = 144;
    evento->zona = exNumberOffset(panelByte, -31);

    return true;
  }

  else if (panelData[panelByte] >= 0x60 && panelData[panelByte] <= 0x7F) {
    //3144 //"Restauracion tamper de zona: "
    evento->q = 1;
    evento->eve = 144;
    evento->zona = exNumberOffset(panelByte, -63);

    return true;
  }

  else return false;
}

bool extractorEventos::exPanelStatus5(byte panelByte, EventoStruct* evento) {


  //*  //3041//Armed by access codes 35-95
  //*  0x00 - 0x04: Access codes 35-39
  //*  0x05 - 0x39: Access codes 43-95

  if (panelData[panelByte] <= 0x39) {
    byte dscCode = panelData[panelByte] + 0x23;
    //3401//"Armado: "
    evento->q = 1;
    evento->eve = 401;
    evento->zona = exPanelAccessCode(dscCode, false);
    return true;
  }


  //*  //1401//Disarmed by access codes 35-95
  //*  0x3A - 0x3E: Access codes 35-39
  //*  0x3F - 0x73: Access codes 43-95

  if (panelData[panelByte] >= 0x3A && panelData[panelByte] <= 0x73) {
    byte dscCode = panelData[panelByte] - 0x17;
    //1401//"Desarmado: "
    evento->q = 0;
    evento->eve = 401;
    evento->zona = exPanelAccessCode(dscCode, false);
    return true;
  }

  return false;
}

bool extractorEventos::exPanelStatus16(byte panelByte, EventoStruct* evento) {
  bool decoded = true;
  evento->q = 0;
  switch (panelData[panelByte]) {
    case 0x80: decoded = false; break;    //"Reconocimiento de falla"//NO REPORTAR
    case 0x81: evento->eve = 381; break;  //1381//"Falla supervision RF"
    case 0x82:
      evento->q = 1;
      evento->eve = 381;
      break;  //3381//"Restauracion supervision RF"
    default:
      decoded = false;
      break;
  }
  if (decoded) return decoded;

  return false;
}

bool extractorEventos::exPanelStatus1B(byte panelByte, EventoStruct* evento) {
  bool decoded = true;
  evento->q = 0;
  switch (panelData[panelByte]) {
    case 0xF1:
      evento->q = 1;
      evento->eve = 305;
      break;  //3305//"Reinicio de sistema"
    default:
      decoded = false;
      break;
  }
  if (decoded) return decoded;

  return false;
}


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

// Specifies the key value to be written by dscClockInterrupt() and selects the write partition.  This includes a 500ms
// delay after alarm keys to resolve errors when additional keys are sent immediately after alarm keys.
void extractorEventos::setWriteKey(const char receivedKey) {
  static unsigned long previousTime;
  static bool setPartition;

  // Sets the write partition if set by virtual keypad key '/'
  if (setPartition) {
    setPartition = false;
    if (receivedKey >= '1' && receivedKey <= '8') {
      writePartition = receivedKey - 48;
    }
    return;
  }

  // Sets the binary to write for virtual keypad keys
  if (!writeKeyPending && (millis() - previousTime > 500 || millis() <= 500)) {
    bool validKey = true;

    // Skips writing to disabled partitions or partitions not specified in dscKeybusInterface.h
    if (disabled[writePartition - 1] || dscPartitions < writePartition) {
      switch (receivedKey) {
        case '/': setPartition = true; validKey = false; break;
      }
      return;
    }

    // Sets binary for virtual keypad keys
    else {
      switch (receivedKey) {
        case '/': setPartition = true; validKey = false; break;
        case '0': writeKey = 0x00; break;
        case '1': writeKey = 0x05; break;
        case '2': writeKey = 0x0A; break;
        case '3': writeKey = 0x0F; break;
        case '4': writeKey = 0x11; break;
        case '5': writeKey = 0x16; break;
        case '6': writeKey = 0x1B; break;
        case '7': writeKey = 0x1C; break;
        case '8': writeKey = 0x22; break;
        case '9': writeKey = 0x27; break;
        case '*': writeKey = 0x28; if (status[writePartition - 1] < 0x9E) starKeyCheck = true; break;
        case '#': writeKey = 0x2D; break;
        case '%': writeKey = 0x46; break;                                                         //HEX 46 wireless key disarm PREBA CE2
        case '&': writeKey = 0x33; break;                                                        //HEX 33 INGRESAR & PARA PASAR DE ESCAQUE EN PROG.
        case 'f': case 'F': writeKey = 0xBB; writeAlarm = true; break;                           // Keypad fire alarm
        case 'b': case 'B': writeKey = 0x82; break;                                              // Enter event buffer
        case '>': writeKey = 0x87; break;                                                        // Event buffer right arrow
        case '<': writeKey = 0x88; break;                                                        // Event buffer left arrow
        case 'l': case 'L': writeKey = 0xA5; break;                                              // LCD keypad data request
        case 's': case 'S': writeKey = 0xAF; writeAccessCode[writePartition - 1] = true; break;  // Arm stay
        case 'w': case 'W': writeKey = 0xB1; writeAccessCode[writePartition - 1] = true; break;  // Arm away
        case 'n': case 'N': writeKey = 0xB6; writeAccessCode[writePartition - 1] = true; break;  // Arm with no entry delay (night arm)
        case 'a': case 'A': writeKey = 0xDD; writeAlarm = true; break;                           // Keypad auxiliary alarm
        case 'c': case 'C': writeKey = 0xBB; break;                                              // Door chime
        case 'r': case 'R': writeKey = 0xDA; break;                                              // Reset
        case 'p': case 'P': writeKey = 0xEE; writeAlarm = true; break;                           // Keypad panic alarm
        case 'x': case 'X': writeKey = 0xE1; break;                                              // Exit
        case '[': writeKey = 0xD5; writeAccessCode[writePartition - 1] = true; break;            // Command output 1
        case ']': writeKey = 0xDA; writeAccessCode[writePartition - 1] = true; break;            // Command output 2
        case '{': writeKey = 0x70; writeAccessCode[writePartition - 1] = true; break;            // Command output 3
        case '}': writeKey = 0xEC; writeAccessCode[writePartition - 1] = true; break;            // Command output 4
        default: {
          validKey = false;
          break;
        }
      }
    }

    // Sets the writing position in dscClockInterrupt() for the currently set partition
    switch (writePartition) {
      case 1:
      case 5: {
        writeByte = 2;
        writeBit = 9;
        break;
      }
      case 2:
      case 6: {
        writeByte = 3;
        writeBit = 17;
        break;
      }
      case 3:
      case 7: {
        writeByte = 8;
        writeBit = 57;
        break;
      }
      case 4:
      case 8: {
        writeByte = 9;
        writeBit = 65;
        break;
      }
      default: {
        writeByte = 2;
        writeBit = 9;
        break;
      }
    }

    if (writeAlarm) previousTime = millis();  // Sets a marker to time writes after keypad alarm keys
    if (validKey) {
      writeKeyPending = true;                 // Sets a flag indicating that a write is pending, cleared by dscClockInterrupt()
      writeReady = false;
    }
  }
}