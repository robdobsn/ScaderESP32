#include "Arduino.h"
#include "M5Stack.h"
#include "M5StackColours.h"
#include "MiniHDLC.h"

// GPIO Pins
const int KITCHEN_PIR_PIN = 26;
const int CTRL_SERIAL_RX_PIN = 5;
const int CTRL_SERIAL_TX_PIN = 2;

// Screen dimensions (sideways)
const int SCREEN_HEIGHT = 320;
const int SCREEN_WIDTH = 240;

// MiniHDLC
void frameRxCB(const uint8_t *framebuffer, unsigned framelength);
MiniHDLC miniHDLC(NULL, frameRxCB);

// Status
bool isInEnabled = false;
bool isOutEnabled = false;
bool arrowRedraw = true;
bool labelRedraw = true;
const int NUM_STATUS_LINES = 3;
bool statusLineRedraw[NUM_STATUS_LINES] = {true, true, true};
String statusLines[NUM_STATUS_LINES];
bool kitPIRLastDispLevel = false;
bool kitPIRLastLevel = false;

// Serial port
HardwareSerial ctrlSerial(2);
const int MAX_CHARS_TO_PROCESS = 500;

// Status update
uint32_t lastStatusUpdateTimeMs = 0;
const int STATUS_UPDATE_RATE_MS = 1000;

// Debug
uint32_t lastDebugTimeMs = 0;
static const uint32_t DEBUG_INTERVAL_MS = 1000;

// Button legends
String buttonLegendText[] = 
{
    "In",
    "Out",
    "Open"
};

// Message names
#define IN_ENABLED_STR "inEnabled"
#define OUT_ENABLED_STR "outEnabled"
#define STATUS_STR1 "statusStr1"
#define STATUS_STR2 "statusStr2"
#define STATUS_STR3 "statusStr3"
#define OPEN_CLOSE_LABEL_STR "openCloseLabel"
const char *statusMsgNames[] =
{
    IN_ENABLED_STR,
    OUT_ENABLED_STR,
    STATUS_STR1,
    STATUS_STR2,
    STATUS_STR3,
    OPEN_CLOSE_LABEL_STR
};

void writeJSONString(const String &str)
{
    uint32_t encodedLen = miniHDLC.calcEncodedLen((const uint8_t*) str.c_str(), str.length());
    std::vector<uint8_t> encoded(encodedLen);
    miniHDLC.encodeFrame(encoded.data(), encodedLen, (const uint8_t*) str.c_str(), str.length());
    ctrlSerial.write(encoded.data(), encodedLen);
    
    // Debug
    Serial.printf("Sent JSON: %s\r\n", str.c_str());
}

// Send command to controller
void sendCommand(String command)
{
    // Form HDLC frame
    String cmdStr = R"({"cmd":")" + command + R"("})";
    writeJSONString(cmdStr);
}

// Send status update
void sendStatusUpdate()
{
    // Status msg
    String statusMsg = R"({"cmd":"status","isInEnabled":)" + String(isInEnabled) + 
                    R"(,"isOutEnabled":)" + String(isOutEnabled) + R"(})";
    writeJSONString(statusMsg);
}

// Handle status message
void handleStatusMessage(const String &name, const String &val)
{
    // Handle status values
    if (name.equalsIgnoreCase(IN_ENABLED_STR))
    {
        bool oldIsInEnabled = isInEnabled;
        isInEnabled = val.equalsIgnoreCase("true") || val.equalsIgnoreCase("1");
        arrowRedraw |= oldIsInEnabled != isInEnabled;
    }
    else if (name.equalsIgnoreCase(OUT_ENABLED_STR))
    {
        bool oldIsOutEnabled = isOutEnabled;
        isOutEnabled = val.equalsIgnoreCase("true") || val.equalsIgnoreCase("1");
        arrowRedraw |= oldIsOutEnabled != isOutEnabled;
    }
    else if (name.equalsIgnoreCase(STATUS_STR1))
    {
        // Handle status 1
        if (statusLines[0] != val)
        {
            statusLines[0] = val;
            statusLineRedraw[0] = true;
        }
    }
    else if (name.equalsIgnoreCase(STATUS_STR2))
    {
        // Handle status 2
        if (statusLines[1] != val)
        {
            statusLines[1] = val;
            statusLineRedraw[1] = true;
        }
    }
    else if (name.equalsIgnoreCase(STATUS_STR3))
    {
        // Handle status 3
        if (statusLines[2] != val)
        {
            statusLines[2] = val;
            statusLineRedraw[2] = true;
        }
    }
    else if (name.equalsIgnoreCase(OPEN_CLOSE_LABEL_STR))
    {
        // Handle open/close label
        if (buttonLegendText[2] != val)
        {
            buttonLegendText[2] = val;
            labelRedraw = true;
        }
    }
}

// Handle single line of serial info
void handleSerialLine(String& serialLine)
{
    // Debug
    // Serial.printf("Status line: %s\r\n", serialLine.c_str());

    // Handle line JSON
    for (int i = 0; i < sizeof(statusMsgNames) / sizeof(statusMsgNames[0]); i++)
    {
        String statusName = statusMsgNames[i];
        String jsonStatus = "\"" + statusName + "\":";
        int statusStart = serialLine.indexOf(jsonStatus);
        if (statusStart > 0)
        {
            // Get status
            int statusEnd1 = serialLine.indexOf(',', statusStart + jsonStatus.length());
            int statusEnd2 = serialLine.indexOf('}', statusStart + jsonStatus.length());
            if ((statusEnd1 > 0) || (statusEnd2 > 0))
            {
                int statusEnd = statusEnd1 > 0 ? statusEnd1 : statusEnd2;
                String statusValue = serialLine.substring(statusStart + jsonStatus.length(), statusEnd);
                statusValue.replace("\"", "");

                // Debug
                Serial.printf("Status: %s = %s\r\n", statusName.c_str(), statusValue.c_str());
                
                // Handle status
                handleStatusMessage(statusName, statusValue);
            }
        }
    }

    // Clear line
    serialLine = "";
}

void frameRxCB(const uint8_t *framebuffer, unsigned framelength)
{
    // Decode
    String frameStr(framebuffer, framelength);
    Serial.printf("frameRxCB %s", frameStr.c_str());
    // Process
    handleSerialLine(frameStr);
    // Serial.printf("frameRxCB %d\r\n", framelength);
}

// Show button legends
void showButtonLegends()
{
    // Button positions
    const int legendsY[] = { 70, 160, 250 };
    const int legendSpacingY = 18;
    const int legendX = 220;

    // Clear
    M5.Lcd.fillRect(legendX, 0, 20, 320, TFT_BLACK);

    // Iterate buttons
    for (int i = 0; i < sizeof(legendsY) / sizeof(legendsY[0]); i++)
    {
        int textLen = buttonLegendText[i].length();
        for (int j = 0; j < textLen; j++)
        {
            // Draw legend
            M5.Lcd.setCursor(legendX, legendsY[i] - textLen*legendSpacingY/2 + j*legendSpacingY);
            M5.Lcd.printf("%c", buttonLegendText[i].charAt(j));
        }
    }
}

// Redraw screen
void redraw()
{
    // Show button legends
    if (labelRedraw)
    {
        showButtonLegends();
        labelRedraw = false;
    }

    // Show status lines
    const int STATUS_LINES_X = 40;
    const int STATUS_LINE_1_Y = 240;
    const int STATUS_LINE_SPACING_Y = 18;
    for (int i = 0; i < NUM_STATUS_LINES; i++)
    {
        if (statusLineRedraw[i])
        {
            // Clear line
            M5.Lcd.fillRect(STATUS_LINES_X, STATUS_LINE_1_Y + i*STATUS_LINE_SPACING_Y, 170, 20, TFT_BLACK);
            M5.Lcd.setCursor(STATUS_LINES_X, STATUS_LINE_1_Y + i*STATUS_LINE_SPACING_Y);
            M5.Lcd.println(statusLines[i]);
            statusLineRedraw[i] = false;
        }
    }

    // Arrow settings
    const int DISABLED_COLOUR = TFT_DARKSLATEGRAY;
    const int ARROW_1_LEFT_X = 50;
    const int ARROW_2_LEFT_X = 40;
    const int ARROW_BODY_LEN = 100;
    const int ARROW_HEAD_LEN = 50;
    const int ARROW_BODY_WIDTH = 50;
    const int ARROW_HEAD_STICKOUT = 20;
    const int ARROW_1_Y = 35;
    const int ARROW_2_Y = 135;

    // Check redraw
    if (arrowRedraw)
    {
        // Done
        arrowRedraw = false;

        // In enabled
        M5.Lcd.fillRect(ARROW_1_LEFT_X, ARROW_1_Y, 
                        ARROW_BODY_LEN, ARROW_BODY_WIDTH, 
                        isInEnabled ? GREEN : DISABLED_COLOUR);
        M5.Lcd.fillTriangle(ARROW_1_LEFT_X+ARROW_BODY_LEN, ARROW_1_Y-ARROW_HEAD_STICKOUT,
                        ARROW_1_LEFT_X+ARROW_BODY_LEN+ARROW_HEAD_LEN, int(ARROW_1_Y + ARROW_BODY_WIDTH / 2),
                        ARROW_1_LEFT_X+ARROW_BODY_LEN, ARROW_1_Y+ARROW_BODY_WIDTH+ARROW_HEAD_STICKOUT,
                        isInEnabled ? GREEN : DISABLED_COLOUR);

        // Out enabled
        M5.Lcd.fillRect(ARROW_2_LEFT_X+ARROW_HEAD_LEN, ARROW_2_Y, 
                        ARROW_BODY_LEN, ARROW_BODY_WIDTH, 
                        isOutEnabled ? GREEN : DISABLED_COLOUR);
        M5.Lcd.fillTriangle(ARROW_2_LEFT_X+ARROW_HEAD_LEN, ARROW_2_Y-ARROW_HEAD_STICKOUT,
                        ARROW_2_LEFT_X, int(ARROW_2_Y + ARROW_BODY_WIDTH / 2),
                        ARROW_2_LEFT_X+ARROW_HEAD_LEN, ARROW_2_Y+ARROW_BODY_WIDTH+ARROW_HEAD_STICKOUT,
                        isOutEnabled ? GREEN : DISABLED_COLOUR);
    }

    // Indicator of PIR input
    bool kitPIRDispLevel = digitalRead(KITCHEN_PIR_PIN);
    if (kitPIRDispLevel != kitPIRLastDispLevel)
    {
        // Draw
        M5.Lcd.fillRect(220, 310, 10, 10, kitPIRDispLevel ? GREEN : DISABLED_COLOUR);

        // Done
        kitPIRLastDispLevel = kitPIRDispLevel;
    }
}

// Setup
void setup()
{
    // put your setup code here, to run once:
    M5.begin();
    M5.Power.begin();
    // M5.Lcd.fillScreen(BLACK);
    // M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setRotation(2);

    // Button legends
    showButtonLegends();

    // Setup ctrl serial
    ctrlSerial.begin(115200, SERIAL_8N1, CTRL_SERIAL_RX_PIN, CTRL_SERIAL_TX_PIN);

    // Setup kitchen PIR input
    pinMode(KITCHEN_PIR_PIN, INPUT);
    kitPIRLastLevel = digitalRead(KITCHEN_PIR_PIN);
    kitPIRLastDispLevel = !kitPIRLastLevel;

    // Setup diags serial
    Serial.begin(115200);
}

// Loop
void loop()
{
    // Update M5stack
    M5.update();

    // Check buttons
    if (M5.BtnA.wasReleased() || M5.BtnA.pressedFor(1000, 200))
    {
        // sendCommand("openCloseToggle");
    }
    else if (M5.BtnB.wasReleased() || M5.BtnB.pressedFor(1000, 200))
    {
        if (isOutEnabled)
        {
            sendCommand("outDisable");
            // isOutEnabled = false;
        }
        else
        {
            sendCommand("outEnable");
            // isOutEnabled = true;
        }
    }
    else if (M5.BtnC.wasReleased() || M5.BtnC.pressedFor(1000, 200))
    {
        if (isInEnabled)
        {
            sendCommand("inDisable");
            // isInEnabled = false;
        }
        else
        {
            sendCommand("inEnable");
            // isInEnabled = true;
        }
    }

    // Check if PIR changed
    bool kitPIRLevel = digitalRead(KITCHEN_PIR_PIN);
    if (kitPIRLevel != kitPIRLastLevel)
    {
        // Send command
        sendCommand(kitPIRLevel ? "kitchenPIRActive" : "kitchenPIRInactive");

        // Done
        kitPIRLastLevel = kitPIRLevel;
    }

    // Check if redraw needed
    redraw();

    // Service serial
    for (int i = 0; i < MAX_CHARS_TO_PROCESS; i++)
    {
        // Check chars
        if (ctrlSerial.available() == 0)
        {
            break;
        }

        // Read into line
        uint8_t c = ctrlSerial.read();
        miniHDLC.handleChar(c);
    }

    // Status update
    uint32_t nowMs = millis();
    if (nowMs - lastStatusUpdateTimeMs > STATUS_UPDATE_RATE_MS)
    {
        // Send status update
        sendStatusUpdate();

        // Done
        lastStatusUpdateTimeMs = nowMs;
    }

    // Debug
    if (nowMs - lastDebugTimeMs > DEBUG_INTERVAL_MS)
    {
        // Debug
        Serial.printf("isInEnabled=%d isOutEnabled=%d\r\n", isInEnabled, isOutEnabled);

        // Done
        lastDebugTimeMs = nowMs;
    }
}
