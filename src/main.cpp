#include <Adafruit_MAX31865.h>
#include <Adafruit_INA260.h>
#include <Adafruit_INA228.h>
#include <LittleFS.h>
#include <LiquidCrystal.h>
#include <Arduino.h>

//MAX31865 (1)
#define MAX31865_MOSI_1 11
#define MAX31865_MISO_1 12
#define MAX31865_SCLK_1 13
#define MAX31865_CS_1   10
//MAX31865 (2)
#define MAX31865_MOSI_2  MAX31865_MOSI_1//9
#define MAX31865_MISO_2  MAX31865_MISO_1//10
#define MAX31865_SCLK_2  MAX31865_SCLK_1//8
#define MAX31865_CS_2   3


#define INA260_I2CADDR 0x44 // A1 is soldered.


// The value of the Rref resistor. Use 430.0 for PT100 and 4300.0 for PT1000
#define RREF      4300.0
// The 'nominal' 0-degrees-C resistance of the sensor
// 100.0 for PT100, 1000.0 for PT1000
#define RNOMINAL  1000 // Ajustar para resistencia em 0ºC

// I2C pins
#define I2C_SDA 4
#define I2C_SCL 5

// LCD pins
#define LCD_RS 41
#define LCD_EN 42
#define LCD_D4 37
#define LCD_D5 38
#define LCD_D6 39
#define LCD_D7 40

#define SELECT_KEY   01
#define LEFT_KEY     02
#define DOWN_KEY     03
#define UP_KEY       04
#define RIGHT_KEY    05

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);


// MAX31865 settings
Adafruit_MAX31865 max31865_1 = Adafruit_MAX31865(MAX31865_CS_1, MAX31865_MOSI_1, MAX31865_MISO_1, MAX31865_SCLK_1);

Adafruit_MAX31865 max31865_2 = Adafruit_MAX31865(MAX31865_CS_2, MAX31865_MOSI_2, MAX31865_MISO_2, MAX31865_SCLK_2); // Temp. Sensor 2

// INA260 and INA228
Adafruit_INA260 ina260;
Adafruit_INA228 ina228;

// Interval to log data (5 minutes in milliseconds)
const unsigned long logInterval =  500;
const unsigned long logIntervalLCD =  150;
unsigned long lastLogTime = 0;
unsigned long lastLogTimeLCD = 0;
int menuPos = 0;
bool isMeasuring = true;
int selectedFile = 1; // File number to manage (1 to 10)
int buttonThresholds[5];

String menuLines[6] =
{
  "Start Measurement",
  "Dump Log to Serial",
  "Erase Memory",
  "File Management",
  "File Browser",
  "Erase File"
};


void calibrateButtons() {
    lcd.clear();
    lcd.print("Calibrating...");
    delay(1000);

    // Step 1: Record baseline (no button pressed)
    lcd.clear();
    lcd.print("Release All");
    lcd.setCursor(0, 1);
    lcd.print("Buttons");
    delay(1000); // Allow time for stabilization

    long sum = 0;
    for (int i = 0; i < 50; i++) { // Sample multiple times for accuracy
        sum += analogRead(A0);
        delay(10);
    }
    buttonThresholds[0] = sum / 50; // Calculate baseline

    lcd.clear();
    lcd.print("Baseline: ");
    lcd.setCursor(0, 1);
    lcd.print(buttonThresholds[0]);
    delay(2000);

    // Step 2: Calibrate each button
    const char* buttonNames[5] = {"SELECT", "LEFT", "DOWN", "UP", "RIGHT"};
    for (int i = 1; i <= 4; i++) {
        lcd.clear();
        lcd.print("Press ");
        lcd.print(buttonNames[i - 1]);
        lcd.setCursor(0, 1);
        lcd.print("Hold steady...");

        // Wait for button press (value drops below baseline - some margin)
        unsigned long timeout = millis();
        while (analogRead(A0) >= buttonThresholds[0] - 300) {
            if (millis() - timeout > 10000) { // Timeout after 5 seconds
                lcd.clear();
                lcd.print("Timeout!");
                lcd.setCursor(0, 1);
                lcd.print(buttonNames[i - 1]);
                delay(2000);
                return; // Exit calibration to avoid indefinite loop
            }
            delay(10);
        }

        // Collect stable readings
        sum = 0;
        for (int j = 0; j < 50; j++) {
            sum += analogRead(A0);
            delay(10);
        }
        buttonThresholds[i] = sum / 50; // Store the threshold for the button

        lcd.clear();
        lcd.print(buttonNames[i - 1]);
        lcd.setCursor(0, 1);
        lcd.print("Saved: ");
        lcd.print(buttonThresholds[i]);
        delay(1500);

        // Wait for the user to release the button
        lcd.clear();
        lcd.print("Release ");
        lcd.print(buttonNames[i - 1]);
        timeout = millis();
        while (analogRead(A0) < buttonThresholds[0] - 300) {
            
            if (millis() - timeout > 10000) { // Timeout after 5 seconds
                lcd.clear();
                lcd.print("Timeout!");
                lcd.setCursor(0, 1);
                lcd.print("Release ");
                lcd.print(buttonNames[i - 1]);
                delay(2000);
                return; // Exit calibration to avoid indefinite loop
            }
            delay(10);
        }
        delay(500);
    }

    lcd.clear();
    lcd.print("Calibration Done");
    delay(2000);

    // Print thresholds for debugging
    Serial.println("Button thresholds:");
    for (int i = 0; i <= 5; i++) {
       //Serial.print("Button ");
       //Serial.print(i == 0 ? "NONE" : buttonNames[i - 1]);
       //Serial.print(": ");
        //Serial.println(buttonThresholds[i]);
    }
  return;
}





void setup() {
   // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.setCursor(0,0);
  lcd.print("Initializing");
  delay(1000);

  calibrateButtons();
  

  pinMode(LCD_RS, OUTPUT);
  pinMode(LCD_EN, OUTPUT);
  pinMode(LCD_D4, OUTPUT);
  pinMode(LCD_D5, OUTPUT);
  pinMode(LCD_D6, OUTPUT);
  pinMode(LCD_D7, OUTPUT);

  


  Serial.begin(115200);

  // Initialize SPI devices
  if (!max31865_1.begin(MAX31865_2WIRE)) {
    Serial.println("Failed to initialize MAX31865 (Sensor 1)");
    while (1);
  }
  if (!max31865_2.begin(MAX31865_2WIRE)) {
    Serial.println("Failed to initialize MAX31865 (Sensor 2)");
    while (1);
  }


  // Initialize I2C and INA260
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!ina260.begin(INA260_I2CADDR)) {
    Serial.println("Failed to initialize INA260");
    while (1);
  }
  if (!ina228.begin(INA228_I2CADDR_DEFAULT)) {
    Serial.println("Failed to initialize INA228");
    while (1);
  }
  ina228.setShunt(0.015,10.0);
  ina228.setMode(INA228_MODE_CONTINUOUS);
  float averagingcount = ina228.getAveragingCount();
  Serial.printf("INA228, Averaging: %f\n",averagingcount);

  // Initialize LittleFS
  if (!LittleFS.begin()) {
    Serial.println("Mounting LittleFS failed! Attempting to format...");

    // Format LittleFS
    if (LittleFS.format()) {
      Serial.println("LittleFS formatted successfully. Rebooting...");
      ESP.restart(); // Reboot to apply the format
    } else {
      Serial.println("Failed to format LittleFS. Check your partition table.");
      while (1);
    }
  } else {
    Serial.println("LittleFS mounted successfully.");
  }
  
}

void logData() {
  
  // Read data from MAX31865 sensors
  float temp1 = max31865_1.temperature(RNOMINAL, RREF); // PT100, 430 Ohm reference
  float temp2 = max31865_2.temperature(RNOMINAL, RREF);
  
  // Read data from INA260
  float currentINA260 = ina260.readCurrent();     // Current in mA
  float voltageINA260 = ina260.readBusVoltage();  // Voltage in m
  
  // Read data from INA228
  float currentINA228 = ina228.readCurrent();     // Current in mA
  float voltageINA228 = ina228.readBusVoltage(); // Voltage in mV
  

  Serial.println("Logging Data...");
  Serial.printf(">Temp1: %.2f °C\n", temp1);
  Serial.printf(">Temp2: %.2f °C\n", temp2);
  Serial.printf(">INA260 - Current: %.2f mA\n", currentINA260);
  Serial.printf(">INA260 - Voltage: %.2f mV\n", voltageINA260);
  Serial.printf(">INA228 - Current: %.2f mA\n", currentINA228);
  Serial.printf(">INA228 - Voltage: %.2f mV\n", voltageINA228);

  float shuntvoltage = 0;
  float busvoltage = 0;
  float current_mA = 0;
  float loadvoltage = 0;


  
  // Save data to flash
  String filename = "/log" + String(selectedFile) + ".txt";
  File logFile = LittleFS.open(filename, FILE_APPEND);
  if (!logFile) {
    Serial.println("Failed to open log file.");
    return;
  }

  logFile.printf(">Time: %lu, >Temp1: %.2f, >Temp2: %.2f, >INA260_Current: %.2f, >INA260_Voltage: %.2f, >INA228_Current: %.2f, >INA228_Voltage: %.2f\n",
                 millis() / 1000, temp1, temp2, currentINA260, voltageINA260, currentINA228, voltageINA228);

  logFile.close();
  Serial.println("Data logged to flash");

   // Displaying on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T1:");
    lcd.print(temp1, 1); // One decimal
    lcd.print("C T2:");
    lcd.print(temp2, 1);
    delay(2000); // Hold for 2 seconds

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("INA260 mA:");
    lcd.print(currentINA260, 1);
    lcd.setCursor(0, 1);
    lcd.print("mV:");
    lcd.print(voltageINA260, 1);
    delay(2000); // Hold for 2 seconds

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("INA228 mA:");
    lcd.print(currentINA228, 1);
    lcd.setCursor(0, 1);
    lcd.print("mV:");
    lcd.print(voltageINA228, 1);
    delay(2000); // Hold for 2 seconds
}

void dumpLogToSerial() {
   String filename = "/log" + String(selectedFile) + ".txt";
    File logFile = LittleFS.open(filename, FILE_READ);
    if (!logFile) {
        Serial.println("No log file found.");
        lcd.clear();
        lcd.printf("No log %d file.",selectedFile );

        delay(1000); // Optional feedback delay
        return;
    }
    // Buffer to store each line
    char buffer[256];

    while (logFile.available()) {
        // Read each line into the buffer
        size_t len = logFile.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
        buffer[len] = '\0'; // Null-terminate the string

        // Variables to hold extracted values
        float temp1, temp2, currentINA260, voltageINA260, currentINA228, voltageINA228;

        // Parse the line using sscanf
        if (sscanf(buffer,
                   ">Time: %*f, >Temp1: %f, >Temp2: %f, >INA260_Current: %f, >INA260_Voltage: %f, >INA228_Current: %f, >INA228_Voltage: %f",
                   &temp1, &temp2, &currentINA260, &voltageINA260, &currentINA228, &voltageINA228) == 6) {
            // Print values in the desired format
            Serial.printf(">Temp1: %.2f °C\n", temp1);
            Serial.printf(">Temp2: %.2f °C\n", temp2);
            Serial.printf(">INA260 - Current: %.2f mA\n", currentINA260);
            Serial.printf(">INA260 - Voltage: %.2f mV\n", voltageINA260);
            Serial.printf(">INA228 - Current: %.2f mA\n", currentINA228);
            Serial.printf(">INA228 - Voltage: %.2f mV\n", voltageINA228);
            Serial.println("----------------------------");
        } else {
            Serial.println("Failed to parse line:");
            Serial.println(buffer);
        }
    }

    logFile.close();
    Serial.println("Log dump complete.");
    delay(1000); // Optional feedback delay
}


int getKeyID()
{
  static unsigned long lastReadTime = 0;
  if (millis() - lastReadTime < 5) return 0;  // Limit polling frequency
  lastReadTime = millis();
  // Calculate the average value of the buffer
  long sum = 0;
  for (int i = 0; i < 20; i++) {
      sum += analogRead(A0);
      delay(1);
  }
  long aRead = sum/20;
  // Debouncing: Repeat the read and confirm
  long confirmedRead = analogRead(A0);
  if (abs(aRead - confirmedRead) > 300) return 0;  // Noise threshold of 50
  
  //int aRead = analogRead(A0);
  if( aRead > buttonThresholds[0] - 500 ) return 0;                //no key is pressed 7888
  if( aRead > buttonThresholds[1] - 500  ) return SELECT_KEY;      //select key 5988
  if( aRead > buttonThresholds[2] - 500  ) return LEFT_KEY;        //left key 4420
  if( aRead > buttonThresholds[3] - 500  ) return DOWN_KEY;        //down key 3100
  if( aRead > buttonThresholds[4] - 500   ) return UP_KEY;       //right key 1958
  if( aRead < buttonThresholds[5]   ) return RIGHT_KEY;         //up key 1494
  

return 0;
}
void manageFiles() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Select File #:");

    while (true) {
        lcd.setCursor(0, 1);
        lcd.print("File: ");
        lcd.print(selectedFile);
        lcd.print("         "); // Clear any leftover text

        // Navigate through file numbers
        if (getKeyID() == UP_KEY) {
            selectedFile++;
            if (selectedFile > 10) selectedFile = 10;
            delay(300); // Debounce
        }
        if (getKeyID() == DOWN_KEY) {
            selectedFile--;
            if (selectedFile < 1) selectedFile = 1;
            delay(300); // Debounce
        }

        // Confirm selection
        if (getKeyID() == SELECT_KEY) {
            lcd.clear();
            lcd.setCursor(0, 1);
            lcd.print("File ");
            lcd.print(selectedFile);
            lcd.print(" selected");
            delay(2000);
            break;
        }

        // Exit file selection
        if (getKeyID() == LEFT_KEY) {
            lcd.clear();
            lcd.setCursor(0, 1);
            lcd.print("Exiting...");
            delay(1000);
            break;
        }
    }
}

void eraseAllMemory() {
    for (int i = 1; i <= 10; i++) {
        String filename = "/log" + String(i) + ".txt";
        if (LittleFS.exists(filename)) {
            if (LittleFS.remove(filename)) {
                Serial.println(filename + " erased successfully.");
                lcd.clear();
                lcd.printf("%s erased successfully.",filename);
                delay(1000); // Optional feedback delay
            } else {
                Serial.println("Failed to erase " + filename);
                delay(1000); // Optional feedback delay
            }
        } else {
            Serial.println(filename + " does not exist.");
            delay(1000); // Optional feedback delay
        }
    }
}

void eraseFile() {
   
        String filename = "/log" + String(selectedFile) + ".txt";
        if (LittleFS.exists(filename)) {
            if (LittleFS.remove(filename)) {
                Serial.println(filename + " erased successfully.");
                delay(1000); // Optional feedback delay
            } else {
                Serial.println("Failed to erase " + filename);
                delay(1000); // Optional feedback delay
            }
        } else {
            Serial.println(filename + " does not exist.");
            delay(1000); // Optional feedback delay
        }
    
}
void browseFiles() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Loading files...");

    // Get all files from LittleFS
    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        lcd.clear();
        lcd.print("Failed to open FS!");
        delay(2000);
        return;
    }

    // Store file names in a list
    String fileNames[20]; // Supports up to 20 files
    int fileCount = 0;

    File file = root.openNextFile();
    while (file && fileCount < 20) {
        fileNames[fileCount++] = file.name();
        file = root.openNextFile();
    }

    if (fileCount == 0) {
        lcd.clear();
        lcd.print("No files found!");
        delay(2000);
        return;
    }

    // File browsing menu
    int fileIndex = 0;
    while (true) {
        // Display current file
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("File: ");
        lcd.print(fileIndex + 1);
        lcd.print("/");
        lcd.print(fileCount);

        lcd.setCursor(0, 1);
        lcd.print(fileNames[fileIndex]);

        // Navigation logic
        if (getKeyID() == UP_KEY) {
            fileIndex--;
            if (fileIndex < 0) fileIndex = fileCount - 1; // Wrap-around
            delay(300); // Debounce
        }
        if (getKeyID() == DOWN_KEY) {
            fileIndex++;
            if (fileIndex >= fileCount) fileIndex = 0; // Wrap-around
            delay(300); // Debounce
        }
        if (getKeyID() == SELECT_KEY) {
            lcd.clear();
            lcd.print("Selected: ");
            lcd.setCursor(0, 1);
            lcd.print(fileNames[fileIndex]);
            delay(2000);
            break;
        }
        if (getKeyID() == LEFT_KEY) {
            lcd.clear();
            lcd.print("Exiting...");
            delay(1000);
            break;
        }
    }
}



void loop() {   
  delay(100);
  
  if (millis() - lastLogTimeLCD >= logIntervalLCD) {
    lastLogTimeLCD = millis();
   
    if (getKeyID() == UP_KEY){
      menuPos--;
      lcd.clear();
    } 
    if (getKeyID() == DOWN_KEY) {
      menuPos++;
      lcd.clear();
    }
    if (menuPos < 0) menuPos = 0;
    if (menuPos > 5) menuPos = 5;
    lcd.setCursor(0, 0);
    lcd.print(">"); 
    lcd.print(menuLines[menuPos]);
    lcd.setCursor(0, 1);
    if (menuPos < 5) {
        lcd.print(" "); // Clear potential leftover text
        lcd.print(menuLines[menuPos + 1]);
    } else {
        lcd.print("                "); // Clear second line if no menu item below
    }
 
  }

  if (getKeyID() == SELECT_KEY) {
    switch (menuPos) {
        case 0: // Start Measurement
            lcd.clear();
            lcd.setCursor(0, 1);
            lcd.print("Measuring...");
            isMeasuring = true;
            delay(1000); // Optional feedback delay

            while(isMeasuring)
            {
              logData(); 
              delay(200);
              if (getKeyID() == LEFT_KEY)
              {
                isMeasuring = false;
                lcd.clear();
                lcd.setCursor(0, 1);
                lcd.print("Stopping Meas...");
                delay(2000);
               } 
            }
           
            break;
        case 1: // Dump Log to Serial
          lcd.clear();
          lcd.setCursor(0, 1);
          lcd.print("Dumping...");
          dumpLogToSerial();
          delay(500); // Optional feedback delay
        break;

        case 2: // Return
            lcd.clear();
            lcd.setCursor(0, 1);
            lcd.print("Erasing Memory...");
            delay(1000); // Optional feedback delay
            eraseAllMemory();
            break;
        case 3: // File Management
           manageFiles();
        break;
        case 4: // File Browser
            browseFiles();
       break;
        case 5: // File Eraser
             lcd.clear();
            lcd.setCursor(0, 1);
            lcd.printf("Erasing file %d",selectedFile);
            delay(1000); // Optional feedback delay
            eraseFile();
       break;
    }
}

   
  
}
