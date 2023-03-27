#include <Arduino.h>
#include <M5Stack.h>
#include <M5StackColours.h>

// Screen dimensions (sideways)
const int SCREEN_HEIGHT = 320;
const int SCREEN_WIDTH = 240;

// Currently read serial data
const int MAX_LINE_LENGTH = 200;
String serialLine = "";

// Status
bool isInEnabled = false;
bool isOutEnabled = false;
bool arrowRedraw = true;
bool labelRedraw = true;
const int NUM_STATUS_LINES = 3;
bool statusLineRedraw[NUM_STATUS_LINES] = {true, true, true};
String statusLines[NUM_STATUS_LINES];

// Serial port
HardwareSerial ctrlSerial(2);

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

// Send command to controller
void sendCommand(String command)
{
    ctrlSerial.printf(R"({"cmd":"%s"})" "\r\n", command.c_str());
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
void handleSerialLine()
{
    // Debug
    Serial.printf("Status line: %s\r\n", serialLine.c_str());

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

// Service serial input
void serviceSerial()
{
    // Handle serial input
    for (int i = 0; i < MAX_LINE_LENGTH; i++)
    {
        // Check chars
        if (ctrlSerial.available() == 0)
            break;

        // Check line length
        if (serialLine.length() > MAX_LINE_LENGTH)
            serialLine = "";

        // Read into line
        char c = ctrlSerial.read();
        if (c == '\n' || c == '\r')
        {
            // Handle line
            handleSerialLine();
        }
        else
        {
            // Append to line
            serialLine += c;
        }
    }
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
            M5.Lcd.fillRect(STATUS_LINES_X, STATUS_LINE_1_Y + i*STATUS_LINE_SPACING_Y, 160, 20, TFT_BLACK);
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
    ctrlSerial.begin(115200, SERIAL_8N1, 5, 2);

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
        sendCommand("openCloseToggle");
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

    // Check if redraw needed
    redraw();

    // Service serial
    serviceSerial();
}
