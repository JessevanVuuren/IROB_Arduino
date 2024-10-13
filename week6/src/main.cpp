/*
 
  This is C++ code for an assignment of the Embeddded Sensor Programming (ESP) course written in the "ESP coding style".
 
  The assignment is to emulate a sixth sense at the fingertips with the help of a sensor and an SSD1331 display.
 
  This code is designed to run on the ESP32 microcontroller and to connect to a SSD1331 display and a TL1838 VS1838B IR.
  The code listens for incoming IR signals and displays the associated data on the screen.
 
  Written by Jesse van Vuuren, Pieter van Turenhout and Wilmar van der Plas (2024)
 
*/

#include <Arduino.h>
#include <Adafruit_SSD1331.h>
#include <IRremote.hpp>

// Define pin numbers of SSD1331 for SPI (Serial Peripheral Interface) communication
#define CS_PIN 5
#define DC_PIN 4
#define DIN_PIN 23
#define CLK_PIN 18
#define RES_PIN 19

// Define IR out pin
#define IR_RECEIVE_PIN 15

// Define color codes
#define BACKGROUND_COLOR 0x0000
#define TEXT_COLOR 0xFFFF
#define WARNING_COLOR 0xF800
#define LINE_COLOR 0x7BEF

#define SERIAL_MONITOR_BAUD_RATE 115200

#define LINE_HEIGHT_PIXELS 8

#define RECENT_SIGNAL_SIZE 4

#define ICON_POSITION_X 74
#define ICON_POSITION_Y 44
#define ICON_SIZE_PIXELS 20

enum Direction {
  Horizontal,
  Vertical
};

typedef struct {
  char* protocol;
  int command;
  int address;
} Signal;

typedef struct {
  int x;
  int y;
  int length;
  Direction direction;
} Line;

typedef struct {
  int x;
  int y;
  String label;
  String data;
} DataDisplayObject;

const uint16_t UNKNOWN_SIGNAL_BITMAP[] PROGMEM = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x9000, 0x7000,
  0x0000, 0x0800, 0x7000, 0xa800, 0xd000, 0xf000, 0xf000, 0xd800, 0xc000, 0x8800, 0x4000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0xa000, 0xf800, 0x7000, 0x0000, 0x6800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xe800,
  0x7800, 0x0800, 0x0000, 0x0000, 0x0000, 0x2000, 0xc800, 0xf800, 0xf800, 0x7000, 0x0000, 0x2800, 0x0800, 0x0000, 0x0800, 0x2800,
  0x4800, 0x9800, 0xe800, 0xf800, 0xf800, 0xe800, 0x4000, 0x0000, 0x6000, 0xf800, 0xf800, 0xe800, 0xc000, 0xf800, 0x7000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x6000, 0xd800, 0xf800, 0xf800, 0x6000, 0x8000, 0xf800, 0x7000, 0x0800,
  0x0000, 0xb000, 0xf800, 0x7800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x9800, 0xf800, 0x8000,
  0x0000, 0x3800, 0x0000, 0x0000, 0x0000, 0x0000, 0xd000, 0xf800, 0x7800, 0x0000, 0x5000, 0xc800, 0x9000, 0x4000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x4800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x3000, 0xd800, 0xf800, 0xf800, 0xf800, 0x8000, 0x0000, 0x6000,
  0xf800, 0xf800, 0xc800, 0x3800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2800, 0xf800, 0xf800, 0xe000, 0x7000,
  0xa000, 0xf800, 0x8000, 0x0000, 0x5800, 0xd800, 0xf800, 0xf800, 0x2800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x8000, 0x9800, 0x0800, 0x0000, 0x0000, 0xa000, 0xf800, 0x8000, 0x0000, 0x0000, 0x8000, 0x8000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xa800, 0xf800, 0x8800, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x4800, 0xd000, 0xd800, 0xf000,
  0xf800, 0x8800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0xe800, 0xf800, 0xf800, 0xe000, 0xa800, 0xf800, 0x8800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0xe000, 0xf800, 0xf800, 0xe000, 0x0000, 0xa800, 0xf800, 0x9000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x5000, 0xf800, 0xf800, 0x5000, 0x0000, 0x0000, 0xb000, 0xf800,
  0x9000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0xb000, 0xa800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

// Initialize Adafruit_SSD1331
Adafruit_SSD1331 display(CS_PIN, DC_PIN, DIN_PIN, CLK_PIN, RES_PIN);

Signal recentSignals[RECENT_SIGNAL_SIZE];

const Line LAYOUT[] = {
  { .x = 0, .y = 9, .length = display.width(), .direction = Horizontal },
  { .x = 0, .y = 29, .length = display.width(), .direction = Horizontal },
  { .x = 72, .y = 42, .length = 23, .direction = Horizontal },
  { .x = 72, .y = 29, .length = 34, .direction = Vertical }
};

DataDisplayObject DATA_DISPLAY_OBJECTS[] = {
  { .x = 0, .y = 0, .label = "SIGNAL ", .data = "" },
  { .x = 0, .y = 12, .label = "Protocol:", .data = "" },
  { .x = 0, .y = 20, .label = "Com:", .data = "" },
  { .x = 48, .y = 20, .label = "Add:", .data = "" },
  { .x = 75, .y = 32, .label = "", .data = "" },
};

const int DISPLAY_SIZE = sizeof(DATA_DISPLAY_OBJECTS) / sizeof(DATA_DISPLAY_OBJECTS[0]);
const int LAYOUT_SIZE = sizeof(LAYOUT) / sizeof(LAYOUT[0]);

int signalInputCounter = 0;

// Returns time since ESP was turned on in HH:MM:SS format
char* getCurrentTime() {
  const int BUFFER_SIZE = 9;
  
  uint64_t milliseconds = millis();

  int seconds = milliseconds / 1000 % 60;
  int minutes = milliseconds / 60000 % 60;
  int hours = milliseconds / 3600000 % 24;

  static char buffer[BUFFER_SIZE];
  snprintf(buffer, BUFFER_SIZE, "%02d:%02d:%02d", hours, minutes, seconds);
  return buffer;
}

// Returns the protocol string cut off at the maximum of 5 characters
char* getProtocolString() {
  const int MAX_STR_LENGTH = 5;

  char* protocolString = (char*)malloc(MAX_STR_LENGTH + 1);
  const char* constProtocolString = IrReceiver.getProtocolString();

  for (int i = 0; i < MAX_STR_LENGTH; i++) {
    protocolString[i] = constProtocolString[i];
  }

  protocolString[MAX_STR_LENGTH] = '\0';
  return protocolString;
}

void drawLine(Line line) {
  if (line.direction == Horizontal) {
    display.drawLine(line.x, line.y, line.x + line.length, line.y, LINE_COLOR);
  } else if (line.direction == Vertical) {
    display.drawLine(line.x, line.y, line.x, line.y + line.length, LINE_COLOR);
  }
}

void displayDataObject(DataDisplayObject d) {
  display.setCursor(d.x, d.y);
  display.print(d.label);
  display.print(d.data);
}

void displayRecentSignals() {
  const int positionX = 0;
  const int positionY = 32;

  display.setCursor(positionX, positionY);

  for (int i = 0; i < RECENT_SIGNAL_SIZE; i++) {
    if (recentSignals[i].protocol == NULL) {
      continue;
    }

    display.print(recentSignals[i].protocol);
    display.print(" " + String(recentSignals[i].command) + " " + String(recentSignals[i].address));

    display.setCursor(positionX, display.getCursorY() + LINE_HEIGHT_PIXELS);
  }
}

void updateRecentSignals() {
  Signal mostRecentSignal = { getProtocolString(), IrReceiver.decodedIRData.command, IrReceiver.decodedIRData.address };

  free(recentSignals[RECENT_SIGNAL_SIZE - 1].protocol);

  for (int i = RECENT_SIGNAL_SIZE - 1; i >= 0; i--) {
    if (i == 0) {
      recentSignals[i] = mostRecentSignal;
    } else {
      recentSignals[i] = recentSignals[i - 1];
    }
  }
}

void playUnknownSignalAnim() {
  const int BLINK_COUNT = 3;
  const int BLINK_INTERVAL_MS = 100;

  for (size_t i = 0; i < BLINK_COUNT; i++) {
    display.drawRGBBitmap(ICON_POSITION_X, ICON_POSITION_Y, UNKNOWN_SIGNAL_BITMAP, ICON_SIZE_PIXELS, ICON_SIZE_PIXELS);
    delay(BLINK_INTERVAL_MS);
    display.writeFillRect(ICON_POSITION_X, ICON_POSITION_Y, ICON_SIZE_PIXELS, ICON_SIZE_PIXELS, BACKGROUND_COLOR);
    delay(BLINK_INTERVAL_MS);
  }
}

void displayLayout() {
  for (int i = 0; i < LAYOUT_SIZE; i++) {
    drawLine(LAYOUT[i]);
  }
}

void displayData() {
  DATA_DISPLAY_OBJECTS[0].data = getCurrentTime();
  DATA_DISPLAY_OBJECTS[1].data = getProtocolString();
  DATA_DISPLAY_OBJECTS[2].data = IrReceiver.decodedIRData.command;
  DATA_DISPLAY_OBJECTS[3].data = IrReceiver.decodedIRData.address;
  DATA_DISPLAY_OBJECTS[4].data = ++signalInputCounter;

  for (size_t i = 0; i < DISPLAY_SIZE; i++) {
    displayDataObject(DATA_DISPLAY_OBJECTS[i]);
  }

  updateRecentSignals();
  displayRecentSignals();
}

void setup() {
  Serial.begin(SERIAL_MONITOR_BAUD_RATE);

  display.begin();
  display.fillScreen(BACKGROUND_COLOR);
  display.setTextColor(TEXT_COLOR);

  IrReceiver.begin(IR_RECEIVE_PIN);

  display.print("Waiting for IR signal...");
}

void loop() {
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol == UNKNOWN) {
      playUnknownSignalAnim();
    } else {
      display.fillScreen(BACKGROUND_COLOR);
      displayLayout();
      displayData();
    }
    IrReceiver.resume();
  }
}