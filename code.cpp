#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Keypad
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};

byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// WiFi
const char *ssid = "YOUR WIFI NAME";
const char *password = "YOUR WIFI PASSWORD";
const char *postEndpoint = "YOUR GOOGLE APP SHEET ENDPOINT";
const char *getEndpoint = postEndpoint;

// Variables
int numStudents = 0;
int currentStudent = 0;
int ratingSum = 0;
bool collectingFeedback = false;
float lastAverage = 0;
int pendingRating = -1;

const int ledPin = 2;

void setup()
{
    Serial.begin(115200);
    Wire.begin(21, 22);
    lcd.begin(16, 2);
    lcd.backlight();
    pinMode(ledPin, OUTPUT);

    lcd.print("Connecting WiFi");
    WiFi.begin(ssid, password);

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20)
    {
        delay(500);
        lcd.print(".");
        retries++;
    }

    lcd.clear();
    if (WiFi.status() == WL_CONNECTED)
    {
        lcd.print("WiFi Connected");
        digitalWrite(ledPin, HIGH);
    }
    else
    {
        lcd.print("WiFi Failed");
    }
    delay(1500);
    displayMenu();
}

void loop()
{
    char key = keypad.getKey();

    if (key && !collectingFeedback)
    {
        switch (key)
        {
        case '1':
            enterFeedback();
            break;
        case '2':
            showAverage();
            break;
        case '3':
            fetchAverage();
            break;
        }
    }

    if (collectingFeedback && key)
    {
        if (pendingRating == -1 && key >= '1' && key <= '5')
        {
            pendingRating = key - '0';
            confirmRating(pendingRating);
        }
        else if (pendingRating != -1)
        {
            if (key == 'A')
            {
                ratingSum += pendingRating;
                currentStudent++;
                lcd.clear();
                lcd.print("Saved: ");
                lcd.print(pendingRating);
                pendingRating = -1;
                delay(1000);

                if (currentStudent < numStudents)
                {
                    lcd.clear();
                    lcd.print("Next student...");
                    delay(1000);
                    promptStudent();
                }
                else
                {
                    collectingFeedback = false;
                    lastAverage = (float)ratingSum / numStudents;
                    sendAverageToSheet(); // Send average after all feedback is collected
                    showAverage();
                }
            }
            else if (key == 'B')
            {
                pendingRating = -1;
                promptStudent(); // Let them rate again
            }
        }
    }
}

void displayMenu()
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("1.Feedback 2.Avg ");
    lcd.setCursor(0, 1);
    lcd.print("3.Fetch");
}

void enterFeedback()
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter students:");
    lcd.setCursor(0, 1);
    lcd.print("B:OK C:Back");
    String num = "";

    while (true)
    {
        char k = keypad.getKey();
        if (k >= '0' && k <= '9')
        {
            num += k;
            lcd.setCursor(0, 1);
            lcd.print("Count: ");
            lcd.print(num);
            lcd.print("    "); // Clear leftovers
        }
        else if (k == 'C')
        {
            displayMenu();
            return;
        }
        else if (k == 'B')
        {
            break;
        }
    }

    numStudents = num.toInt();
    if (numStudents > 0)
    {
        currentStudent = 0;
        ratingSum = 0;
        collectingFeedback = true;
        lcd.clear();
        lcd.print("Start feedback");
        delay(1000);
        clearKeypadBuffer();
        promptStudent();
    }
    else
    {
        lcd.clear();
        lcd.print("Invalid number!");
        delay(1500);
        displayMenu();
    }
}

void promptStudent()
{
    lcd.clear();
    lcd.print("Rate (1 to 5):");
    lcd.setCursor(0, 1);
    lcd.print("Student ");
    lcd.print(currentStudent + 1);
}

void confirmRating(int rating)
{
    lcd.clear();
    lcd.print("You chose: ");
    lcd.print(rating);
    lcd.setCursor(0, 1);
    lcd.print("A:OK B:Cancel");
}

void clearKeypadBuffer()
{
    while (keypad.getKey())
        delay(10);
}

void showAverage()
{
    lcd.clear();
    lcd.print("Avg: ");
    lcd.print(lastAverage, 2);
    delay(2000);
    displayMenu();
}

void sendAverageToSheet()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;
        http.begin(postEndpoint);
        http.addHeader("Content-Type", "application/json");

        // Include both the average and the number of students
        String jsonData = "{\"average\":" + String(lastAverage, 2) + ",\"num_students\":" + String(numStudents) + "}";
        int response = http.POST(jsonData);

        lcd.clear();
        lcd.print(response > 0 ? "Sent to Sheet!" : "Send Failed!");
        http.end();
    }
    else
    {
        lcd.clear();
        lcd.print("WiFi Error");
    }
    delay(2000);
}

void fetchAverage()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;
        http.begin(getEndpoint);
        int responseCode = http.GET();

        lcd.clear();
        if (responseCode > 0)
        {
            String payload = http.getString();
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, payload);
            float fetchedAvg = doc["average"];
            lcd.print("Fetched Avg: ");
            lcd.print(fetchedAvg, 2);
        }
        else
        {
            lcd.print("Fetch Failed!");
        }
        http.end();
    }
    else
    {
        lcd.clear();
        lcd.print("WiFi Error");
    }
    delay(2000);
    displayMenu();
}