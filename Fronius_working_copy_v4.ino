// 20x4 LCD
// Version 4

/*
Changes to add:
DONE  1. Debouncing rotary encoder
DONE  2. Save selectd values to EEPROM/Flash
      3. Change layout to 4 circuts
      4. Fix variable naming
      5. Add timer for data update
      6. Add night shutdown
DONE  7. Fix calculation of when to energize the relays
DONE  8. Add 0 to power on code to prevent turning on before getting data from
Fronius DONE  9. Add retries for getting data from Fronius
      10. Add FreeRTOS (multithread)
*/

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>  //SDA = B7[A4], SCL = B6[A5] STM32/[Arduino]
#include <Preferences.h>
#include <SolarCalculator.h>
#include <WiFi.h>

#include "NTP.h"
#include "WiFiUdp.h"

WiFiUDP wifiUdp;
NTP ntp(wifiUdp);

// Display setup
LiquidCrystal_I2C lcd(0x27, 20, 4);

int menuCounter = 1;  // counts the clicks of the rotary encoder between menu
                      // items (0-3 in this case)

int menu1_Value = 0;  // value within menu 1
int menu2_Value = 0;  // value within menu 2
int menu3_Value = 0;  // value within menu 3
int menu4_Value = 0;  // value within menu 4

int menu1_Value_old = 0;
int menu2_Value_old = 0;
int menu3_Value_old = 0;
int menu4_Value_old = 0;

bool menu1_selected = false;  // enable/disable to change the value of menu item
bool menu2_selected = false;
bool menu3_selected = false;
bool menu4_selected = false;
// Note: if a menu is selected ">" becomes "X".

// Defining pins
// Arduino interrupt pins: 2, 3.
const int RotaryCLK = 17;  // CLK pin on the rotary encoder
const int RotaryDT = 16;   // DT pin on the rotary encoder
const int PushButton = 4;  // Button to enter/exit menu

// Statuses for the rotary encoder
int CLKNow;
int CLKPrevious;

int DTNow;
int DTPrevious;

bool refreshLCD = true;         // refreshes values
bool refreshSelection = false;  // refreshes selection (> / X)

// Fronius setup
const char *HOST = "192.168.1.15";
const char *SSID = "FiberLink69";
const char *PASSWORD = "Internet69";
const long interval =
    20000;  // interval at which to get data from Fronius (milliseconds)
unsigned long previousMillis =
    0;  // will store last time Fonius data was updated
unsigned int generatedPower = 0;  // Fronius current power
int froniusRetries = 0;  // Count unsuccessful data retrieval from Fronius

unsigned long lastDebounceTime = 0;  // used to debounce rotary encoder
unsigned long debounceDelay = 50;    // the debounce time

// Relay setup
const int Relay1 = 27;  // Relay 1 circut
const int Relay2 = 26;  // Relay 1 circut
const int Relay3 = 25;  // Relay 1 circut
const int Relay4 = 33;  // Relay 1 circut

// Time for day calculacion
int year = 2022;
int month = 1;
int day = 1;

// Location
double latitude = 49.99;
double longitude = 19.55;
int utc_offset = 0;

double transit, sunrise, sunset;

bool isDay = true;  // check if it's day

Preferences preferences;  // Initiate an instance of the Preferences library

// Dispaly functions
void rotate() {
    //-----MENU1--------------------------------------------------------------
    if (menu1_selected == true) {
        CLKNow = digitalRead(RotaryCLK);  // Read the state of the CLK pin
        // If last and current state of CLK are different, then a pulse occurred
        if (CLKNow != CLKPrevious) {
            lastDebounceTime = millis();  // If the state changed due to noise
                                          // or pressinf, reset the timer
        }
        if ((millis() - lastDebounceTime) > debounceDelay) {
            if (CLKNow != CLKPrevious && CLKNow == 1) {
                // If the DT state is different than the CLK state then
                // the encoder is rotating in A direction, so we increase
                if (digitalRead(RotaryDT) != CLKNow) {
                    if (menu1_Value < 100)  // we do not go above 100
                    {
                        menu1_Value += 10;
                    } else {
                        menu1_Value = 0;
                    }
                } else {
                    if (menu1_Value < 1)  // we do not go below 0
                    {
                        menu1_Value = 100;
                    } else {
                        // Encoder is rotating B direction so decrease
                        menu1_Value -= 10;
                    }
                }
            }
        }
        CLKPrevious = CLKNow;  // Store last CLK state
    }
    //---MENU2---------------------------------------------------------------
    else if (menu2_selected == true) {
        CLKNow = digitalRead(RotaryCLK);  // Read the state of the CLK pin
        // If last and current state of CLK are different, then a pulse occurred
        if (CLKNow != CLKPrevious) {
            lastDebounceTime = millis();  // If the state changed due to noise
                                          // or pressinf, reset the timer
        }
        if ((millis() - lastDebounceTime) > debounceDelay) {
            if (CLKNow != CLKPrevious && CLKNow == 1) {
                // If the DT state is different than the CLK state then
                // the encoder is rotating in A direction, so we increase
                if (digitalRead(RotaryDT) != CLKNow) {
                    if (menu2_Value < 11000)  // we do not go above 100
                    {
                        menu2_Value += 100;
                    } else {
                        menu2_Value = 0;
                    }
                } else {
                    if (menu2_Value < 1)  // we do not go below 0
                    {
                        menu2_Value = 11000;
                    } else {
                        // Encoder is rotating in B direction, so decrease
                        menu2_Value -= 100;
                    }
                }
            }
        }
        CLKPrevious = CLKNow;  // Store last CLK state
    }
    //---MENU3---------------------------------------------------------------
    else if (menu3_selected == true) {
        CLKNow = digitalRead(RotaryCLK);  // Read the state of the CLK pin
        // If last and current state of CLK are different, then a pulse occurred
        if (CLKNow != CLKPrevious) {
            lastDebounceTime = millis();  // If the state changed due to noise
                                          // or pressinf, reset the timer
        }
        if ((millis() - lastDebounceTime) > debounceDelay) {
            if (CLKNow != CLKPrevious && CLKNow == 1) {
                // If the DT state is different than the CLK state then
                // the encoder is rotating in A direction, so we increase
                if (digitalRead(RotaryDT) != CLKNow) {
                    if (menu3_Value < 11000)  // we do not go above 100
                    {
                        menu3_Value += 100;
                    } else {
                        menu3_Value = 0;
                    }
                } else {
                    if (menu3_Value < 1)  // we do not go below 0
                    {
                        menu3_Value = 11000;
                    } else {
                        // Encoder is rotating B direction so decrease
                        menu3_Value -= 100;
                    }
                }
            }
        }
        CLKPrevious = CLKNow;  // Store last CLK state
    }
    //---MENU4----------------------------------------------------------------
    else if (menu4_selected == true) {
        CLKNow = digitalRead(RotaryCLK);  // Read the state of the CLK pin
        // If last and current state of CLK are different, then a pulse occurred
        if (CLKNow != CLKPrevious) {
            lastDebounceTime = millis();  // If the state changed due to noise
                                          // or pressinf, reset the timer
        }
        if ((millis() - lastDebounceTime) > debounceDelay) {
            if (CLKNow != CLKPrevious && CLKNow == 1) {
                // If the DT state is different than the CLK state then
                // the encoder is rotating in A direction, so we increase
                if (digitalRead(RotaryDT) != CLKNow) {
                    if (menu4_Value < 11000)  // we do not go above 100
                    {
                        menu4_Value += 100;
                    } else {
                        menu4_Value = 0;
                    }
                } else {
                    if (menu4_Value < 1)  // we do not go below 0
                    {
                        menu4_Value = 11000;
                    } else {
                        // Encoder is rotating in B direction, so decrease
                        menu4_Value -= 100;
                    }
                }
            }
        }
        CLKPrevious = CLKNow;  // Store last CLK state
    } else                     // MENU
            // COUNTER----------------------------------------------------------------------------
    {
        CLKNow = digitalRead(RotaryCLK);  // Read the state of the CLK pin
        // If last and current state of CLK are different, then a pulse occurred
        if (CLKNow != CLKPrevious) {
            lastDebounceTime = millis();  // If the state changed due to noise
                                          // or pressinf, reset the timer
        }
        if ((millis() - lastDebounceTime) > debounceDelay) {
            if (CLKNow != CLKPrevious && CLKNow == 1) {
                // If the DT state is different than the CLK state then
                // the encoder is rotating in A direction, so we increase
                if (digitalRead(RotaryDT) != CLKNow) {
                    if (menuCounter < 3)  // we do not go above 3
                    {
                        menuCounter++;
                    } else {
                        menuCounter = 1;
                    }
                } else {
                    if (menuCounter < 2)  // we do not go below 0
                    {
                        menuCounter = 3;
                    } else {
                        // Encoder is rotating CW so decrease
                        menuCounter--;
                    }
                }
            }
        }
        CLKPrevious = CLKNow;  // Store last CLK state
    }

    // Refresh LCD after changing the counter's value
    refreshLCD = true;
}

void pushButton() {
    if ((millis() - lastDebounceTime) > debounceDelay) {
        switch (menuCounter) {
            case 0:
                menu1_selected =
                    !menu1_selected;          // we change the status of the
                lastDebounceTime = millis();  // variable to the opposite
                break;

            case 1:
                menu2_selected = !menu2_selected;
                lastDebounceTime = millis();
                break;

            case 2:
                menu3_selected = !menu3_selected;
                lastDebounceTime = millis();
                break;

            case 3:
                menu4_selected = !menu4_selected;
                lastDebounceTime = millis();
                break;
        }

        refreshLCD = true;  // Refresh LCD after changing the value of the menu
        refreshSelection = true;  // refresh the selection ("X")
    }
}

void printLCD() {
    // These are the values which are not changing the operation

    lcd.setCursor(1, 0);  // 1st line, 2nd block
    lcd.print("Prod");    // text
    //----------------------
    lcd.setCursor(1, 1);  // 2nd line, 2nd block
    lcd.print("Obw 1");   // text
    //----------------------
    lcd.setCursor(1, 2);  // 3rd line, 2nd block
    lcd.print("Obw 2");   // text
    //----------------------
    lcd.setCursor(1, 3);  // 4th line, 2nd block
    lcd.print("Obw 3");   // text
    //----------------------
    lcd.setCursor(19, 0);  // 1st line, 14th block
    lcd.print("W");        // counts - text
}

void updateLCD() {
    // lcd.setCursor(17,0); //1st line, 18th block
    // lcd.print(menuCounter); //counter (0 to 3)

    lcd.setCursor(9, 0);        // 1st line, 10th block
    lcd.print("        ");      // erase the content by printing space over it
    lcd.setCursor(9, 0);        // 1st line, 10th block
    lcd.print(generatedPower);  // print the value of menu1_Value variable

    lcd.setCursor(9, 1);
    lcd.print("     ");
    lcd.setCursor(9, 1);
    lcd.print(menu2_Value);  //

    lcd.setCursor(9, 2);
    lcd.print("     ");
    lcd.setCursor(9, 2);
    lcd.print(menu3_Value);  //

    lcd.setCursor(9, 3);
    lcd.print("     ");
    lcd.setCursor(9, 3);
    lcd.print(menu4_Value);  //
}

void updateCursorPosition() {
    // Clear display's ">" parts
    lcd.setCursor(0, 0);  // 1st line, 1st block
    lcd.print(" ");       // erase by printing a space
    lcd.setCursor(0, 1);
    lcd.print(" ");
    lcd.setCursor(0, 2);
    lcd.print(" ");
    lcd.setCursor(0, 3);
    lcd.print(" ");

    // Place cursor to the new position
    switch (menuCounter)  // this checks the value of the counter (0, 1, 2 or 3)
    {
        case 0:
            lcd.setCursor(0, 0);  // 1st line, 1st block
            lcd.print(">");
            break;
        //-------------------------------
        case 1:
            lcd.setCursor(0, 1);  // 2nd line, 1st block
            lcd.print(">");
            break;
        //-------------------------------
        case 2:
            lcd.setCursor(0, 2);  // 3rd line, 1st block
            lcd.print(">");
            break;
        //-------------------------------
        case 3:
            lcd.setCursor(0, 3);  // 4th line, 1st block
            lcd.print(">");
            break;
    }
}

void updateSelection() {
    // When a menu is selected ">" becomes "X"

    if (menu1_selected == true) {
        lcd.setCursor(0, 0);  // 1st line, 1st block
        lcd.print("X");
    }
    //-------------------
    if (menu2_selected == true) {
        lcd.setCursor(0, 1);  // 2nd line, 1st block
        lcd.print("X");
    }
    //-------------------
    if (menu3_selected == true) {
        lcd.setCursor(0, 2);  // 3rd line, 1st block
        lcd.print("X");
    }
    //-------------------
    if (menu4_selected == true) {
        lcd.setCursor(0, 3);  // 4th line, 1st block
        lcd.print("X");
    }
}

// Fronius communication functions
//void connectToWiFi(const char *ssid, const char *pwd);

void connectToWiFi(const char *ssid, const char *pwd) {
    WiFi.begin(ssid, pwd);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("Wifi connected");
    // lcd.print("Wifi connected");
}

// Save current values to flash, but only if they changed
void saveValues() {
    preferences.begin("values", false);
    if (menu1_Value != menu1_Value_old || menu2_Value != menu2_Value_old ||
        menu3_Value != menu3_Value_old || menu4_Value != menu4_Value_old) {
        preferences.putInt("menu1", menu1_Value);
        preferences.putInt("menu2", menu2_Value);
        preferences.putInt("menu3", menu3_Value);
        preferences.putInt("menu4", menu4_Value);

        menu1_Value_old = menu1_Value;
        menu2_Value_old = menu2_Value;
        menu3_Value_old = menu3_Value;
        menu4_Value_old = menu4_Value;
    }
    preferences.end();
}

void getFroniusData() {
    if ((WiFi.status() == WL_CONNECTED)) {
        HTTPClient http;
        String url = "http://" + String(HOST) +
                     "/solar_api/v1/GetInverterRealtimeData.cgi?Scope=System";
        http.begin(url);
        // start connection and send HTTP header
        int httpCode = http.GET();
        // httpCode will be negative on error
        if (httpCode > 0) {
            // HTTP header has been send and Server response header has been
            // handled file found at server
            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                // Allocate the JSON document
                // Use arduinojson.org/v6/assistant to compute the capacity.
                // const size_t capacity = JSON_OBJECT_SIZE(3) +
                // JSON_ARRAY_SIZE(2) + 60;
                DynamicJsonDocument doc(900);
                // Parse JSON object
                DeserializationError error = deserializeJson(doc, payload);
                if (error) {
                    Serial.print(F("deserializeJson() failed: "));
                    Serial.println(error.c_str());
                    return;
                }
                // Extract current power generated from the Fronius inverter
                generatedPower = doc["Body"]["Data"]["PAC"]["Values"]["1"];
                // String displayedPower = String(generatedPower) + " W";
                // Serial.print(displayedPower);
                froniusRetries = 5;
                refreshLCD = true;
            }
        } else {
            Serial.print("HTTP Error");
            // lcd.print("HTTP Error");
            Serial.printf("[HTTP] GET... failed, error: %s\n",
                          http.errorToString(httpCode).c_str());
            lcd.setCursor(
                9,
                0);  // Defining positon to write from first row, first column .
            lcd.print("Bl fal. ");
        }
        http.end();
    } else {
        Serial.println("Wifi Error");
        lcd.setCursor(
            9, 0);  // Defining positon to write from first row, first column .
        lcd.print("Bl sieci");
        // lcd.println("Wifi Error");
        connectToWiFi(SSID, PASSWORD);
    }
    Serial.println();
    lcd.setCursor(0, 0);
    // print the number of seconds since reset:
    // lcd.print(millis() / 1000);
}

void checkIfDay() {
    ntp.update();

    calcSunriseSunset(year, month, day, latitude, longitude, transit, sunrise,
                      sunset);

    char str[6];
    int sunrise_h = String(hoursToString(sunrise + utc_offset, str))
                        .substring(0, 2)
                        .toInt();
    int sunrise_m =
        String(hoursToString(sunrise + utc_offset, str)).substring(3).toInt();

    int sunset_h =
        String(hoursToString(sunset + utc_offset, str)).substring(0, 2).toInt();
    int sunset_m =
        String(hoursToString(sunset + utc_offset, str)).substring(3).toInt();

    if ((ntp.hours() * 100 + ntp.minutes()) > (sunrise_h * 100 + sunrise_m) &&
        (ntp.hours() * 100 + ntp.minutes()) < (sunset_h * 100 + sunset_m)) {
        isDay = true;
        Serial.println("it's day");
    } else {
        isDay = false;
        Serial.println("it's night");
    }
}

// Rounded HH:mm format
char *hoursToString(double h, char *str) {
    int m = int(round(h * 60));
    int hr = (m / 60) % 24;
    int mn = m % 60;

    str[0] = (hr / 10) % 10 + '0';
    str[1] = (hr % 10) + '0';
    str[2] = ':';
    str[3] = (mn / 10) % 10 + '0';
    str[4] = (mn % 10) + '0';
    str[5] = '\0';
    return str;
}

void setup() {
    Serial.begin(115200);

    // Get stored values
    preferences.begin("values", false);
    menu1_Value = preferences.getInt("menu1", menu1_Value);
    menu2_Value = preferences.getInt("menu2", menu2_Value);
    menu3_Value = preferences.getInt("menu3", menu3_Value);
    menu4_Value = preferences.getInt("menu4", menu4_Value);
    preferences.end();

    // Save old values to skip saving to memory after restart
    menu1_Value_old = menu1_Value;
    menu2_Value_old = menu2_Value;
    menu3_Value_old = menu3_Value;
    menu4_Value_old = menu4_Value;

    pinMode(Relay1, OUTPUT);
    pinMode(Relay2, OUTPUT);
    pinMode(Relay3, OUTPUT);
    pinMode(Relay4, OUTPUT);

    pinMode(RotaryCLK, INPUT_PULLUP);   // RotaryCLK
    pinMode(RotaryDT, INPUT_PULLUP);    // RotaryDT
    pinMode(PushButton, INPUT_PULLUP);  // Button

    //------------------------------------------------------
    lcd.init();  // initialize the lcd
    lcd.backlight();
    //------------------------------------------------------
    lcd.setCursor(
        0, 0);  // Defining positon to write from first row, first column .
    lcd.print("Fronius sterownik");
    lcd.setCursor(0, 1);  // Second row, first column
    lcd.print("Wersja 4.0");
    lcd.setCursor(0, 2);  // Second row, first column
    lcd.print(" ");
    delay(5000);  // wait 2 sec

    lcd.clear();  // clear the whole LCD

    printLCD();  // print the stationary parts on the screen
    //------------------------------------------------------
    // Store states of the rotary encoder
    CLKPrevious = digitalRead(RotaryCLK);
    DTPrevious = digitalRead(RotaryDT);

    ntp.begin();

    attachInterrupt(digitalPinToInterrupt(RotaryCLK), rotate,
                    CHANGE);  // CLK pin is an interrupt pin
    attachInterrupt(digitalPinToInterrupt(PushButton), pushButton,
                    FALLING);  // PushButton pin is an interrupt pin
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        // save the last time you got Fronius data
        previousMillis = currentMillis;

        checkIfDay();

        if (isDay == true) {
            for (froniusRetries = 0; froniusRetries < 5; froniusRetries++) {
                getFroniusData();  // Get data from Fronius
            }
            saveValues();  // Save current values to memory if they changed
        } else {
            generatedPower = 0;
            saveValues();
        }
    }

    if (refreshLCD == true)  // If we are allowed to update the LCD ...
    {
        updateLCD();  // ... we update the LCD ...

        //... also, if one of the menus are already selected...
        if (menu1_selected == true || menu2_selected == true ||
            menu3_selected == true || menu4_selected == true) {
            // do nothing
        } else {
            updateCursorPosition();  // update the position
        }

        refreshLCD = false;  // reset the variable - wait for a new trigger
    }

    if (refreshSelection == true)  // if the selection is changed
    {
        updateSelection();  // update the selection on the LCD
        refreshSelection =
            false;  // reset the variable - wait for a new trigger
    }
    // Relay switch acion

    // Circut 1
    if (menu2_Value == 11000) {
        digitalWrite(Relay1, LOW);
    } else {
        if (menu2_Value == 0) {
            digitalWrite(Relay1, HIGH);
        } else {
            if (menu2_Value <= generatedPower) {
                digitalWrite(Relay1, LOW);
            } else {
                digitalWrite(Relay1, HIGH);
            }
        }
    }

    // Circut 2
    if (menu3_Value == 11000) {
        digitalWrite(Relay2, LOW);
    } else {
        if (menu3_Value == 0) {
            digitalWrite(Relay2, HIGH);
        } else {
            if (menu3_Value <= generatedPower) {
                digitalWrite(Relay2, LOW);
            } else {
                digitalWrite(Relay2, HIGH);
            }
        }
    }

    // Circut 3
    if (menu4_Value == 11000) {
        digitalWrite(Relay3, LOW);
    } else {
        if (menu4_Value == 0) {
            digitalWrite(Relay3, HIGH);
        } else {
            if (menu4_Value <= generatedPower) {
                digitalWrite(Relay3, LOW);
            } else {
                digitalWrite(Relay3, HIGH);
            }
        }
    }

    digitalWrite(Relay4, HIGH);
}