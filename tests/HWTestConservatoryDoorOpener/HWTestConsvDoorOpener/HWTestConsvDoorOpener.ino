// Test for Conservatory Door Opener
// 2023-10-22

#include <Wire.h>
#include <AS5600.h>
#include <TMC2209.h>

static const int HW_I2C_SCL = 22;
static const int HW_I2C_SDA = 21;
static const uint32_t HW_CONS_PIR = 15;
static const uint32_t HW_CONS_BUTTON = 32;
static const int HW_RX_FROM_UI = 19;
static const int HW_TX_TO_UI = 18;

TwoWire i2cOne = TwoWire(0);
AS5600 as5600;
TMC2209 stepDriver;
HardwareSerial& stepSerial = Serial2;
HardwareSerial& uiSerial = Serial1;

void setup() {
  Serial.begin(115200);

  pinMode(HW_CONS_PIR, INPUT);
  pinMode(HW_CONS_BUTTON, INPUT_PULLUP);

  i2cOne.begin(HW_I2C_SCL, HW_I2C_SCL, 100000);
  as5600.begin();
  
  stepSerial.begin(20000, SERIAL_8N1, 23, 4);
  stepDriver.setup(stepSerial, 20000, TMC2209::SERIAL_ADDRESS_0, 23, 4);
  stepDriver.setHardwareEnablePin(27);
  stepDriver.enable();

  uiSerial.begin(115200, SERIAL_N81, HW_RX_FROM_UI, HW_TX_FROM_UI);
}

void loop() {

  bool hwConsPir = digitalRead(HW_CONS_PIR);
  bool hwConsButton = digitalRead(HW_CONS_BUTTON);
  uint32_t hwAngle = as5600.getCumulativePosition();

  if (!stepDriver.isSetupAndCommunicating())
  {
    Serial.println("Stepper driver not setup and communicating!");
  }

  String rxStr;
  for (int i = 0; i < 500; i++)
  {
    if (uiSerial.available() == 0)
      break;
    rxStr += uiSerial.read();
  }

  Serial.printf("HW Test Consv Ctrl PIR %d Button %d Angle %d Rx %s\n", 
        hwConsPir, hwConsButton, hwAngle, rxStr.c_str());

  delay(250);
}
