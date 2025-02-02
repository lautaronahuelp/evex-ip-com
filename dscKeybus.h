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

#ifndef dscKeybus_h
#define dscKeybus_h

#include <Arduino.h>

#if defined(__AVR__)
const byte dscPartitions = 4;   // Maximum number of partitions - requires 19 bytes of memory per partition
const byte dscZones = 4;        // Maximum number of zone groups, 8 zones per group - requires 6 bytes of memory per zone group
const byte dscBufferSize = 10;  // Number of commands to buffer if the sketch is busy - requires dscReadSize + 2 bytes of memory per command
const byte dscReadSize = 16;    // Maximum bytes of a Keybus command
#elif defined(ESP8266)
const byte dscPartitions = 8;
const byte dscZones = 8;
const byte dscBufferSize = 50;
const byte dscReadSize = 16;
#elif defined(ESP32)
const byte dscPartitions = 8;
const byte dscZones = 8;
const DRAM_ATTR byte dscBufferSize = 50;
const DRAM_ATTR byte dscReadSize = 16;
#endif

// Exit delay target states
#define DSC_EXIT_STAY 1
#define DSC_EXIT_AWAY 2
#define DSC_EXIT_NO_ENTRY_DELAY 3


class dscKeybusInterface {

  public:

    // Initializes writes as disabled by default
    dscKeybusInterface(byte setClockPin, byte setReadPin, byte setWritePin = 255);

    // Interface control
    void begin(Stream &_stream = Serial);             // Initializes the stream output to Serial by default
    bool loop();                                      // Returns true if valid panel data is available
    void stop();                                      // Disables the clock hardware interrupt and data timer interrupt
    void resetStatus();                               // Resets the state of all status components as changed for sketches to get the current status

    // Writes a single key - nonblocking unless a previous write is in progress
    void write(const char receivedKey);

    // Writes multiple keys from a char array
    //
    // By default, this is nonblocking unless there is a previous write in progress - in this case, the sketch must keep the char
    // array defined at least until the write is complete.
    //
    // If the char array is ephemeral, check if the write is complete by checking writeReady or set blockingWrite to true to
    // block until the write is complete.
    void write(const char * receivedKeys, bool blockingWrite = false);

    // Write control
    static byte writePartition;                       // Set to a partition number for virtual keypad
    bool writeReady;                                  // True if the library is ready to write a key

    // Prints output to the stream interface set in begin()
    void printPanelBinary(bool printSpaces = true);   // Includes spaces between bytes by default
    void printPanelCommand();                         // Prints the panel command as hex
    void printPanelMessage();                         // Prints the decoded panel message
    void printModuleBinary(bool printSpaces = true);  // Includes spaces between bytes by default
    void printModuleMessage();                        // Prints the decoded keypad or module message

    // These can be configured in the sketch setup() before begin()
    bool hideKeypadDigits;          // Controls if keypad digits are hidden for publicly posted logs (default: false)
    static bool processModuleData;  // Controls if keypad and module data is processed and displayed (default: false)
    bool displayTrailingBits;       // Controls if bits read as the clock is reset are displayed, appears to be spurious data (default: false)

    // Panel time
    bool timestampChanged;          // True after the panel sends a timestamped message
    byte hour, minute, day, month;
    int year;

    // Sets panel time, the year can be sent as either 2 or 4 digits, returns true if the panel is ready to set the time
    bool setTime(unsigned int year, byte month, byte day, byte hour, byte minute, const char* accessCode, byte timePartition = 1);

    // Status tracking
    bool statusChanged;                   // True after any status change
    bool pauseStatus;                     // Prevent status from showing as changed, set in sketch to control when to update status
    bool keybusConnected, keybusChanged;  // True if data is detected on the Keybus
    byte accessCode[dscPartitions];
    bool accessCodeChanged[dscPartitions];
    bool accessCodePrompt;                // True if the panel is requesting an access code
    bool decimalInput;                    // True if the panel is requesting 3 digit input (for 0x6E readout)
    bool trouble, troubleChanged;
    bool powerTrouble, powerChanged;
    bool batteryTrouble, batteryChanged;
    bool keypadFireAlarm, keypadAuxAlarm, keypadPanicAlarm;
    bool ready[dscPartitions], readyChanged[dscPartitions];
    bool disabled[dscPartitions], disabledChanged[dscPartitions];
    bool armed[dscPartitions], armedAway[dscPartitions], armedStay[dscPartitions];
    bool noEntryDelay[dscPartitions], armedChanged[dscPartitions];
    bool alarm[dscPartitions], alarmChanged[dscPartitions];
    bool exitDelay[dscPartitions], exitDelayChanged[dscPartitions];
    byte exitState[dscPartitions], exitStateChanged[dscPartitions];
    bool entryDelay[dscPartitions], entryDelayChanged[dscPartitions];
    bool fire[dscPartitions], fireChanged[dscPartitions];
    bool openZonesStatusChanged;
    byte openZones[dscZones], openZonesChanged[dscZones];    // Zone status is stored in an array using 1 bit per zone, up to 64 zones
    bool alarmZonesStatusChanged;
    byte alarmZones[dscZones], alarmZonesChanged[dscZones];  // Zone alarm status is stored in an array using 1 bit per zone, up to 64 zones
    bool pgmOutputsStatusChanged;
    byte pgmOutputs[2], pgmOutputsChanged[2];
    byte panelVersion;

    /* panelData[] and moduleData[] store panel and keypad/module data in an array: command [0], stop bit by itself [1],
     * followed by the remaining data.  These can be accessed directly in the sketch to get data that is not already
     * tracked in the library.  See dscKeybusPrintData.cpp for the currently known DSC commands and data.
     *
     * panelData[] example:
     *   Byte 0     Byte 2   Byte 3   Byte 4   Byte 5
     *   00000101 0 10000001 00000001 10010001 11000111 [0x05] Partition 1: Ready Backlight - Partition ready | Partition 2: disabled
     *            ^ Byte 1 (stop bit)
     */
    static byte panelData[dscReadSize];
    static volatile byte moduleData[dscReadSize];

    // status[] and lights[] store the current status message and LED state for each partition.  These can be accessed
    // directly in the sketch to get data that is not already tracked in the library.  See printPanelMessages() and
    // printPanelLights() in dscKeybusPrintData.cpp to see how this data translates to the status message and LED status.
    byte status[dscPartitions];
    byte lights[dscPartitions];

    // Process keypad and module data, returns true if data is available
    bool handleModule();

    // True if dscBufferSize needs to be increased
    static volatile bool bufferOverflow;

    // Timer interrupt function to capture data - declared as public for use by AVR Timer1
    static void dscDataInterrupt();

    // Deprecated
    bool processRedundantData;  // Controls if repeated periodic commands are processed and displayed (default: false)

  protected:

    void processPanelStatus();
    void processPanelStatus0(byte partition, byte panelByte);
    void processPanelStatus1(byte partition, byte panelByte);
    void processPanelStatus2(byte partition, byte panelByte);
    void processPanelStatus4(byte partition, byte panelByte);
    void processPanelStatus5(byte partition, byte panelByte);
    void processPanel_0x16();
    void processPanel_0x27();
    void processPanel_0x2D();
    void processPanel_0x34();
    void processPanel_0x3E();
    void processPanel_0x87();
    void processPanel_0xA5();
    void processPanel_0xE6();
    void processPanel_0xE6_0x09();
    void processPanel_0xE6_0x0B();
    void processPanel_0xE6_0x0D();
    void processPanel_0xE6_0x0F();
    void processPanel_0xE6_0x1A();
    void processPanel_0xEB();
    void processReadyStatus(byte partitionIndex, bool status);
    void processAlarmStatus(byte partitionIndex, bool status);
    void processExitDelayStatus(byte partitionIndex, bool status);
    void processEntryDelayStatus(byte partitionIndex, bool status);
    void processNoEntryDelayStatus(byte partitionIndex, bool status);
    void processZoneStatus(byte zonesByte, byte panelByte);
    void processTime(byte panelByte);
    void processAlarmZones(byte panelByte, byte startByte, byte zoneCountOffset, byte writeValue);
    void processAlarmZonesStatus(byte zonesByte, byte zoneCount, byte writeValue);
    void processArmed(byte partitionIndex, bool armedStatus);
    void processPanelAccessCode(byte partitionIndex, byte dscCode, bool accessCodeIncrease = true);

    void printPanelPartitionStatus(byte startPartition, byte startByte, byte endByte);
    void printPanelStatus0(byte panelByte);
    void printPanelStatus1(byte panelByte);
    void printPanelStatus2(byte panelByte);
    void printPanelStatus3(byte panelByte);
    void printPanelStatus4(byte panelByte);
    void printPanelStatus5(byte panelByte);
    void printPanelStatus14(byte panelByte);
    void printPanelStatus16(byte panelByte);
    void printPanelStatus17(byte panelByte);
    void printPanelStatus18(byte panelByte);
    void printPanelStatus1B(byte panelByte);

    void printPanelMessages(byte panelByte);
    void printPanelLights(byte panelByte, bool printMessage = true);
    void printPanelTime(byte panelByte);
    void printPanelBeeps(byte panelByte);
    void printPanelTone(byte panelByte);
    void printPanelBuzzer(byte panelByte);
    bool printPanelZones(byte inputByte, byte startZone);
    void printPanelAccessCode(byte dscCode, bool accessCodeIncrease = true);
    void printPanelBitNumbers(byte panelByte, byte startNumber, byte startBit = 0, byte stopBit = 7, bool printNone = true);
    void printNumberSpace(byte number);
    void printNumberOffset(byte panelByte, int numberOffset);
    void printUnknownData();
    void printPartition();
    void printStatusLights();
    void printStatusLightsFlashing();
    void printZoneLights(bool lowerRange = true);
    void printPanel_0x05();
    void printPanel_0x0A_0F();
    void printPanel_0x11();
    void printPanel_0x16();
    void printPanel_0x1B();
    void printPanel_0x1C();
    void printPanel_0x22_28_33_39();
    void printPanel_0x27();
    void printPanel_0x2D();
    void printPanel_0x34();
    void printPanel_0x3E();
    void printPanel_0x41();
    void printPanel_0x4C();
    void printPanel_0x57();
    void printPanel_0x58();
    void printPanel_0x5D_63();
    void printPanel_0x64();
    void printPanel_0x69();
    void printPanel_0x6E();
    void printPanel_0x70();
    void printPanel_0x75();
    void printPanel_0x7A();
    void printPanel_0x7F();
    void printPanel_0x82();
    void printPanel_0x87();
    void printPanel_0x8D();
    void printPanel_0x94();
    void printPanel_0x9E();
    void printPanel_0xA5();
    void printPanel_0xAA();
    void printPanel_0xB1();
    void printPanel_0xBB();
    void printPanel_0xC3();
    void printPanel_0xCE();
    void printPanel_0xD5();
    void printPanel_0xE6();
    void printPanel_0xE6_0x01_06_20_21();
    void printPanel_0xE6_0x08_0A_0C_0E();
    void printPanel_0xE6_0x09();
    void printPanel_0xE6_0x0B();
    void printPanel_0xE6_0x0D();
    void printPanel_0xE6_0x0F();
    void printPanel_0xE6_0x17();
    void printPanel_0xE6_0x18();
    void printPanel_0xE6_0x19();
    void printPanel_0xE6_0x1A();
    void printPanel_0xE6_0x1D();
    void printPanel_0xE6_0x1F();
    void printPanel_0xE6_0x2B();
    void printPanel_0xE6_0x2C();
    void printPanel_0xE6_0x41();
    void printPanel_0xEB();
    void printPanel_0xEC();

    void printModule_0xBB();
    void printModule_0xDD();
    void printModule_0xEE();
    void printModule_Status();
    void printModule_0x11();
    void printModule_0x41();
    void printModule_0x4C();
    void printModule_0x57();
    void printModule_0x58();
    void printModule_0x70();
    void printModule_0x94();
    void printModule_0xD5();
    bool printModule_Keys();
    void printModule_KeyCodes(byte keyByte);
    void printModule_Expander();
    bool printModuleSlots(byte startCount, byte startByte, byte endByte, byte startMask, byte endMask, byte bitShift, byte matchValue, bool reverse = false);
    void printModuleProgramming(byte panelByte2, byte panelByte3);

    bool validCRC();
    void writeKeys(const char * writeKeysArray);
    void setWriteKey(const char receivedKey);
    static void dscClockInterrupt();
    static bool redundantPanelData(byte previousCmd[], volatile byte currentCmd[], byte checkedBytes = dscReadSize);

    #if defined(ESP32)
    static hw_timer_t * timer1;
    static portMUX_TYPE timer1Mux;
    #endif

    Stream* stream;
    const char* writeKeysArray;
    bool writeKeysPending;
    bool writeAccessCode[dscPartitions];
    bool queryResponse;
    bool previousTrouble;
    bool previousKeybus;
    bool previousPower;
    bool previousDisabled[dscPartitions];
    byte previousAccessCode[dscPartitions];
    byte previousLights[dscPartitions], previousStatus[dscPartitions];
    bool previousReady[dscPartitions];
    bool previousExitDelay[dscPartitions], previousEntryDelay[dscPartitions];
    byte previousExitState[dscPartitions];
    bool previousArmed[dscPartitions], previousArmedStay[dscPartitions], previousNoEntryDelay[dscPartitions];
    bool previousAlarm[dscPartitions];
    bool previousFire[dscPartitions];
    byte previousOpenZones[dscZones], previousAlarmZones[dscZones];
    byte previousPgmOutputs[2];
    bool keybusVersion1;

    static byte dscClockPin;
    static byte dscReadPin;
    static byte dscWritePin;
    static byte writeByte, writeBit;
    static bool virtualKeypad;
    static char writeKey;
    static byte panelBitCount, panelByteCount;
    static volatile bool writeKeyPending;
    static volatile bool writeAlarm, starKeyCheck, starKeyWait[dscPartitions];
    static volatile bool moduleDataDetected, moduleDataCaptured;
    static volatile unsigned long clockHighTime, keybusTime;
    static volatile byte panelBufferLength;
    static volatile byte panelBuffer[dscBufferSize][dscReadSize];
    static volatile byte panelBufferBitCount[dscBufferSize], panelBufferByteCount[dscBufferSize];
    static volatile byte moduleBitCount, moduleByteCount;
    static volatile byte currentCmd, statusCmd, moduleCmd, moduleSubCmd;
    static volatile byte isrPanelData[dscReadSize], isrPanelBitTotal, isrPanelBitCount, isrPanelByteCount;
    static volatile byte isrModuleData[dscReadSize];
};

#endif // dscKeybus_h