/*
    DSC Keybus Interface

    https://github.com/taligentx/dscKeybusInterface

    This library is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dscKeybus.h"


#if defined(ESP32)
portMUX_TYPE dscKeybusInterface::timer1Mux = portMUX_INITIALIZER_UNLOCKED;
hw_timer_t * dscKeybusInterface::timer1 = NULL;
#endif  // ESP32


dscKeybusInterface::dscKeybusInterface(byte setClockPin, byte setReadPin, byte setWritePin) {
  dscClockPin = setClockPin;
  dscReadPin = setReadPin;
  dscWritePin = setWritePin;
  if (dscWritePin != 255) virtualKeypad = true;
  writeReady = false;
  processRedundantData = true;
  displayTrailingBits = false;
  processModuleData = false;
  writePartition = 1;
  pauseStatus = false;
}


void dscKeybusInterface::begin(Stream &_stream) {
  pinMode(dscClockPin, INPUT);
  pinMode(dscReadPin, INPUT);
  if (virtualKeypad) pinMode(dscWritePin, OUTPUT);
  stream = &_stream;

  // Platform-specific timers trigger a read of the data line 250us after the Keybus clock changes

  // Arduino/AVR Timer1 calls ISR(TIMER1_OVF_vect) from dscClockInterrupt() and is disabled in the ISR for a one-shot timer
  #if defined(__AVR__)
  TCCR1A = 0;
  TCCR1B = 0;
  TIMSK1 |= (1 << TOIE1);

  // esp8266 timer1 calls dscDataInterrupt() from dscClockInterrupt() as a one-shot timer
  #elif defined(ESP8266)
  timer1_isr_init();
  timer1_attachInterrupt(dscDataInterrupt);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);

  // esp32 timer1 calls dscDataInterrupt() from dscClockInterrupt()
  #elif defined(ESP32)
  timer1 = timerBegin(1, 80, true);
  timerStop(timer1);
  timerAttachInterrupt(timer1, &dscDataInterrupt, true);
  timerAlarmWrite(timer1, 250, true);
  timerAlarmEnable(timer1);
  #endif

  // Generates an interrupt when the Keybus clock rises or falls - requires a hardware interrupt pin on Arduino/AVR
  attachInterrupt(digitalPinToInterrupt(dscClockPin), dscClockInterrupt, CHANGE);
}


void dscKeybusInterface::stop() {

  // Disables Arduino/AVR Timer1 interrupts
  #if defined(__AVR__)
  TIMSK1 = 0;

  // Disables esp8266 timer1
  #elif defined(ESP8266)
  timer1_disable();
  timer1_detachInterrupt();

  // Disables esp32 timer1
  #elif defined(ESP32)
  timerAlarmDisable(timer1);
  timerEnd(timer1);
  #endif

  // Disables the Keybus clock pin interrupt
  detachInterrupt(digitalPinToInterrupt(dscClockPin));

  // Resets the panel capture data and counters
  panelBufferLength = 0;
  for (byte i = 0; i < dscReadSize; i++) isrPanelData[i] = 0;
  isrPanelBitTotal = 0;
  isrPanelBitCount = 0;
  isrPanelByteCount = 0;

  // Resets the keypad and module capture data
  for (byte i = 0; i < dscReadSize; i++) isrModuleData[i] = 0;
}


bool dscKeybusInterface::loop() {

  #if defined(ESP8266) || defined(ESP32)
  yield();
  #endif

  // Checks if Keybus data is detected and sets a status flag if data is not detected for 3s
  #if defined(ESP32)
  portENTER_CRITICAL(&timer1Mux);
  #else
  noInterrupts();
  #endif

  if (millis() - keybusTime > 3000) keybusConnected = false;  // keybusTime is set in dscDataInterrupt() when the clock resets
  else keybusConnected = true;

  #if defined(ESP32)
  portEXIT_CRITICAL(&timer1Mux);
  #else
  interrupts();
  #endif

  if (previousKeybus != keybusConnected) {
    previousKeybus = keybusConnected;
    keybusChanged = true;
    if (!pauseStatus) statusChanged = true;
    if (!keybusConnected) return true;
  }

  // Writes keys when multiple keys are sent as a char array
  if (writeKeysPending) writeKeys(writeKeysArray);

  // Skips processing if the panel data buffer is empty
  if (panelBufferLength == 0) return false;

  // Copies data from the buffer to panelData[]
  static byte panelBufferIndex = 1;
  byte dataIndex = panelBufferIndex - 1;
  for (byte i = 0; i < dscReadSize; i++) panelData[i] = panelBuffer[dataIndex][i];
  panelBitCount = panelBufferBitCount[dataIndex];
  panelByteCount = panelBufferByteCount[dataIndex];
  panelBufferIndex++;

  // Resets counters when the buffer is cleared
  #if defined(ESP32)
  portENTER_CRITICAL(&timer1Mux);
  #else
  noInterrupts();
  #endif

  if (panelBufferIndex > panelBufferLength) {
    panelBufferIndex = 1;
    panelBufferLength = 0;
  }

  #if defined(ESP32)
  portEXIT_CRITICAL(&timer1Mux);
  #else
  interrupts();
  #endif

  // Waits at startup for the 0x05 status command or a command with valid CRC data to eliminate spurious data.
  static bool startupCycle = true;
  if (startupCycle) {
    if (panelData[0] == 0) return false;
    else if (panelData[0] == 0x05 || panelData[0] == 0x1B) {
      if (panelByteCount == 6) keybusVersion1 = true;
      startupCycle = false;
      writeReady = true;
    }
    else if (!validCRC()) return false;
  }

  // Sets writeReady status
  if (!writeKeyPending && !writeKeysPending) writeReady = true;
  else writeReady = false;

  // Skips redundant data sent constantly while in installer programming
  static byte previousCmd0A[dscReadSize];
  static byte previousCmd0F[dscReadSize];
  static byte previousCmdE6_20[dscReadSize];
  static byte previousCmdE6_21[dscReadSize];
  switch (panelData[0]) {
    case 0x0A:  // Partition 1 status in programming
      if (redundantPanelData(previousCmd0A, panelData)) return false;
      break;

    case 0x0F:  // Partition 2 status in programming
      if (redundantPanelData(previousCmd0F, panelData)) return false;
      break;

    case 0xE6:
      if (panelData[2] == 0x20 && redundantPanelData(previousCmdE6_20, panelData)) return false;  // Partition 1 status in programming, zone lights 33-64
      if (panelData[2] == 0x21 && redundantPanelData(previousCmdE6_21, panelData)) return false;  // Partition 2 status in programming
      break;
  }
  if (dscPartitions > 4) {
    static byte previousCmdE6_03[dscReadSize];
    if (panelData[0] == 0xE6 && panelData[2] == 0x03 && redundantPanelData(previousCmdE6_03, panelData, 8)) return false;  // Status in alarm/programming, partitions 5-8
  }

  // Processes valid panel data
  switch (panelData[0]) {
    case 0x05:                                                     // Panel status: partitions 1-4
    case 0x1B: processPanelStatus(); break;                        // Panel status: partitions 5-8
    case 0x16: processPanel_0x16(); break;                         // Panel configuration
    case 0x27: processPanel_0x27(); break;                         // Panel status and zones 1-8 status
    case 0x2D: processPanel_0x2D(); break;                         // Panel status and zones 9-16 status
    case 0x34: processPanel_0x34(); break;                         // Panel status and zones 17-24 status
    case 0x3E: processPanel_0x3E(); break;                         // Panel status and zones 25-32 status
    case 0x87: processPanel_0x87(); break;                         // PGM outputs
    case 0xA5: processPanel_0xA5(); break;                         // Date, time, system status messages - partitions 1-2
    case 0xE6: if (dscPartitions > 2) processPanel_0xE6(); break;  // Extended status command split into multiple subcommands to handle up to 8 partitions/64 zones
    case 0xEB: if (dscPartitions > 2) processPanel_0xEB(); break;  // Date, time, system status messages - partitions 1-8
  }

  return true;
}


bool dscKeybusInterface::handleModule() {
  if (!moduleDataCaptured) return false;
  moduleDataCaptured = false;

  if (moduleBitCount < 8) return false;

  // Determines if a keybus message is a response to a panel command
  switch (moduleCmd) {
    case 0x11:
    case 0x28:
    case 0xD5: queryResponse = true; break;
    default: queryResponse = false; break;
  }

  return true;
}

// Sets up writes for a single key
void dscKeybusInterface::write(const char receivedKey) {

  // Blocks if a previous write is in progress
  while(writeKeyPending || writeKeysPending) {
    loop();
    #if defined(ESP8266)
    yield();
    #endif
  }

  setWriteKey(receivedKey);
}


// Sets up writes for multiple keys sent as a char array
void dscKeybusInterface::write(const char *receivedKeys, bool blockingWrite) {

  // Blocks if a previous write is in progress
  while(writeKeyPending || writeKeysPending) {
    loop();
    #if defined(ESP8266)
    yield();
    #endif
  }

  writeKeysArray = receivedKeys;

  if (writeKeysArray[0] != '\0') {
    writeKeysPending = true;
    writeReady = false;
  }

  // Optionally blocks until the write is complete, necessary if the received keys char array is ephemeral
  if (blockingWrite) {
    while (writeKeysPending) {
      writeKeys(writeKeysArray);
      loop();
      #if defined(ESP8266)
      yield();
      #endif
    }
  }
  else writeKeys(writeKeysArray);
}


// Writes multiple keys from a char array
void dscKeybusInterface::writeKeys(const char *writeKeysArray) {
  static byte writeCounter = 0;
  if (!writeKeyPending && writeKeysPending && writeCounter < strlen(writeKeysArray)) {
    if (writeKeysArray[writeCounter] != '\0') {
      setWriteKey(writeKeysArray[writeCounter]);
      writeCounter++;
      if (writeKeysArray[writeCounter] == '\0') {
        writeKeysPending = false;
        writeCounter = 0;
      }
    }
  }
}


// Specifies the key value to be written by dscClockInterrupt() and selects the write partition.  This includes a 500ms
// delay after alarm keys to resolve errors when additional keys are sent immediately after alarm keys.
void dscKeybusInterface::setWriteKey(const char receivedKey) {
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


#if defined(__AVR__)
bool dscKeybusInterface::redundantPanelData(byte previousCmd[], volatile byte currentCmd[], byte checkedBytes) {
#elif defined(ESP8266)
bool ICACHE_RAM_ATTR dscKeybusInterface::redundantPanelData(byte previousCmd[], volatile byte currentCmd[], byte checkedBytes) {
#elif defined(ESP32)
bool IRAM_ATTR dscKeybusInterface::redundantPanelData(byte previousCmd[], volatile byte currentCmd[], byte checkedBytes) {
#endif

  bool redundantData = true;
  for (byte i = 0; i < checkedBytes; i++) {
    if (previousCmd[i] != currentCmd[i]) {
      redundantData = false;
      break;
    }
  }
  if (redundantData) return true;
  else {
    for (byte i = 0; i < dscReadSize; i++) previousCmd[i] = currentCmd[i];
    return false;
  }
}


bool dscKeybusInterface::validCRC() {
  byte byteCount = (panelBitCount - 1) / 8;
  int dataSum = 0;
  for (byte panelByte = 0; panelByte < byteCount; panelByte++) {
    if (panelByte != 1) dataSum += panelData[panelByte];
  }
  if (dataSum % 256 == panelData[byteCount]) return true;
  else return false;
}


// Called as an interrupt when the DSC clock changes to write data for virtual keypad and setup timers to read
// data after an interval.
#if defined(__AVR__)
void dscKeybusInterface::dscClockInterrupt() {
#elif defined(ESP8266)
void ICACHE_RAM_ATTR dscKeybusInterface::dscClockInterrupt() {
#elif defined(ESP32)
void IRAM_ATTR dscKeybusInterface::dscClockInterrupt() {
#endif

  // Data sent from the panel and keypads/modules has latency after a clock change (observed up to 160us for
  // keypad data).  The following sets up a timer for each platform that will call dscDataInterrupt() in
  // 250us to read the data line.

  // AVR Timer1 calls dscDataInterrupt() via ISR(TIMER1_OVF_vect) when the Timer1 counter overflows
  #if defined(__AVR__)
  TCNT1=61535;            // Timer1 counter start value, overflows at 65535 in 250us
  TCCR1B |= (1 << CS10);  // Sets the prescaler to 1

  // esp8266 timer1 calls dscDataInterrupt() in 250us
  #elif defined(ESP8266)
  timer1_write(1250);

  // esp32 timer1 calls dscDataInterrupt() in 250us
  #elif defined(ESP32)
  timerStart(timer1);
  portENTER_CRITICAL(&timer1Mux);
  #endif

  static unsigned long previousClockHighTime;
  static bool skipData = false;

  // Panel sends data while the clock is high
  if (digitalRead(dscClockPin) == HIGH) {
    if (virtualKeypad) digitalWrite(dscWritePin, LOW);  // Restores the data line after a virtual keypad write
    previousClockHighTime = micros();
  }

  // Keypads and modules send data while the clock is low
  else {
    clockHighTime = micros() - previousClockHighTime;  // Tracks the clock high time to find the reset between commands

    // Saves data and resets counters after the clock cycle is complete (high for at least 1ms)
    if (clockHighTime > 1000) {
      keybusTime = millis();

      // Skips incomplete and redundant data from status commands - these are sent constantly on the keybus at a high
      // rate, so they are always skipped.  Checking is required in the ISR to prevent flooding the buffer.
      if (isrPanelBitTotal < 8) skipData = true;
      else switch (isrPanelData[0]) {
        static byte previousCmd05[dscReadSize];
        static byte previousCmd1B[dscReadSize];

        case 0x05:  // Status: partitions 1-4
          if (redundantPanelData(previousCmd05, isrPanelData, isrPanelByteCount)) skipData = true;
          break;

        case 0x1B:  // Status: partitions 5-8
          if (redundantPanelData(previousCmd1B, isrPanelData, isrPanelByteCount)) skipData = true;
          break;
      }

      // Stores new panel data in the panel buffer
      currentCmd = isrPanelData[0];
      if (panelBufferLength == dscBufferSize) bufferOverflow = true;
      else if (!skipData && panelBufferLength < dscBufferSize) {
        for (byte i = 0; i < dscReadSize; i++) panelBuffer[panelBufferLength][i] = isrPanelData[i];
        panelBufferBitCount[panelBufferLength] = isrPanelBitTotal;
        panelBufferByteCount[panelBufferLength] = isrPanelByteCount;
        panelBufferLength++;
      }

      if (processModuleData) {

        // Stores new keypad and module data - this data is not buffered
        if (moduleDataDetected) {
          moduleCmd = isrPanelData[0];
          moduleSubCmd = isrPanelData[2];
          moduleDataDetected = false;
          moduleDataCaptured = true;  // Sets a flag for handleModule()
          for (byte i = 0; i < dscReadSize; i++) moduleData[i] = isrModuleData[i];
          moduleBitCount = isrPanelBitTotal;
          moduleByteCount = isrPanelByteCount;
        }

        // Resets the keypad and module capture data
        for (byte i = 0; i < dscReadSize; i++) isrModuleData[i] = 0;
      }

      // Resets the panel capture data and counters
      for (byte i = 0; i < dscReadSize; i++) isrPanelData[i] = 0;
      isrPanelBitTotal = 0;
      isrPanelBitCount = 0;
      isrPanelByteCount = 0;
      skipData = false;
    }

    // Virtual keypad
    if (virtualKeypad) {

      static bool writeStart = false;
      static bool writeRepeat = false;
      static bool writeCmd = false;

      if (writePartition <= 4 && statusCmd == 0x05) writeCmd = true;
      else if (writePartition >= 5 && statusCmd == 0x1B) writeCmd = true;
      else writeCmd = false;

      // Writes a F/A/P alarm key and repeats the key on the next immediate command from the panel (0x1C verification)
      if ((writeAlarm && writeKeyPending) || writeRepeat) {

        // Writes the first bit by shifting the alarm key data right 7 bits and checking bit 0
        if (isrPanelBitTotal == 0) {
          if (!((writeKey >> 7) & 0x01)) {
            digitalWrite(dscWritePin, HIGH);
          }
          writeStart = true;  // Resolves a timing issue where some writes do not begin at the correct bit
        }

        // Writes the remaining alarm key data
        else if (writeStart && isrPanelBitTotal <= 7) {
          if (!((writeKey >> (7 - isrPanelBitTotal)) & 0x01)) digitalWrite(dscWritePin, HIGH);

          // Resets counters when the write is complete
          if (isrPanelBitTotal == 7) {
            writeKeyPending = false;
            writeStart = false;
            writeAlarm = false;

            // Sets up a repeated write for alarm keys
            if (!writeRepeat) writeRepeat = true;
            else writeRepeat = false;
          }
        }
      }

      // Writes a regular key unless waiting for a response to the '*' key or the panel is sending a query command
      else if (writeKeyPending && !starKeyWait[writePartition - 1] && isrPanelByteCount == writeByte && writeCmd) {

        // Writes the first bit by shifting the key data right 7 bits and checking bit 0
        if (isrPanelBitTotal == writeBit) {
          if (!((writeKey >> 7) & 0x01)) digitalWrite(dscWritePin, HIGH);
          writeStart = true;  // Resolves a timing issue where some writes do not begin at the correct bit
        }

        // Writes the remaining key data
        else if (writeStart && isrPanelBitTotal > writeBit && isrPanelBitTotal <= writeBit + 7) {
          if (!((writeKey >> (7 - isrPanelBitCount)) & 0x01)) digitalWrite(dscWritePin, HIGH);

          // Resets counters when the write is complete
          if (isrPanelBitTotal == writeBit + 7) {
            if (starKeyCheck) starKeyWait[writePartition - 1] = true;  // Handles waiting until the panel is ready after pressing '*'
            else writeKeyPending = false;
            writeStart = false;
          }
        }
      }
    }
  }
  #if defined(ESP32)
  portEXIT_CRITICAL(&timer1Mux);
  #endif
}


// Interrupt function called by AVR Timer1, esp8266 timer1, and esp32 timer1 after 250us to read the data line
#if defined(__AVR__)
void dscKeybusInterface::dscDataInterrupt() {
#elif defined(ESP8266)
void ICACHE_RAM_ATTR dscKeybusInterface::dscDataInterrupt() {
#elif defined(ESP32)
void IRAM_ATTR dscKeybusInterface::dscDataInterrupt() {
  timerStop(timer1);
  portENTER_CRITICAL(&timer1Mux);
#endif

  // Panel sends data while the clock is high
  if (digitalRead(dscClockPin) == HIGH) {

    // Reads panel data and sets data counters
    if (isrPanelByteCount < dscReadSize) {  // Limits Keybus data bytes to dscReadSize
      if (isrPanelBitCount < 8) {
        // Data is captured in each byte by shifting left by 1 bit and writing to bit 0
        isrPanelData[isrPanelByteCount] <<= 1;
        if (digitalRead(dscReadPin) == HIGH) {
          isrPanelData[isrPanelByteCount] |= 1;
        }
      }

      // Tests for a status command, used in dscClockInterrupt() to ensure keys are only written during a status command
      if (isrPanelBitTotal == 7) {
        switch (isrPanelData[0]) {
          case 0x05:
          case 0x0A: statusCmd = 0x05; break;
          case 0x1B: statusCmd = 0x1B; break;
          default: statusCmd = 0; break;
        }
      }

      // Stores the stop bit by itself in byte 1 - this aligns the Keybus bytes with panelData[] bytes
      if (isrPanelBitTotal == 8) {
        isrPanelBitCount = 0;
        isrPanelByteCount++;
      }

      // Increments the bit counter if the byte is incomplete
      else if (isrPanelBitCount < 7) {
        isrPanelBitCount++;
      }

      // Byte is complete, set the counters for the next byte
      else {
        isrPanelBitCount = 0;
        isrPanelByteCount++;
      }

      isrPanelBitTotal++;
    }
  }

  // Keypads and modules send data while the clock is low
  else {

    // Keypad and module data is not buffered and skipped if the panel data buffer is filling
    if (processModuleData && isrPanelByteCount < dscReadSize && panelBufferLength <= 1) {

      // Data is captured in each byte by shifting left by 1 bit and writing to bit 0
      if (isrPanelBitCount < 8) {
        isrModuleData[isrPanelByteCount] <<= 1;
        if (digitalRead(dscReadPin) == HIGH) {
          isrModuleData[isrPanelByteCount] |= 1;
        }
        else {
          moduleDataDetected = true;  // Keypads and modules send data by pulling the data line low
        }
      }

      // Stores the stop bit by itself in byte 1 - this aligns the Keybus bytes with moduleData[] bytes
      if (isrPanelBitTotal == 8) {
        isrModuleData[1] = 1;  // Sets the stop bit manually to 1 in byte 1
      }
    }
  }
  #if defined(ESP32)
  portEXIT_CRITICAL(&timer1Mux);
  #endif
}