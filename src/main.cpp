// Mandatory includes for ESP32, WiFi, Web Server, DNS, and Preferences
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>      // For saving credentials permanently
#include <ESP_Mail_Client.h>  // For sending emails via SMTP

// ------------------------------------------ WiFi & Web Server Configuration ------------------------------------------
// We will load credentials from memory, but define the AP details for configuration mode
#define AP_SSID "SmartMailBox_Setup"
#define AP_PASSWORD "********" // Password for the setup hotspot

// Create objects for the web server, DNS server, and preferences
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;

// Global variables to hold the credentials loaded from memory
String wifi_ssid = "";
String wifi_password = "";
String recipient_email = "";


// ------------------------------------------ Email Configuration ------------------------------------------
// !!! IMPORTANT: These are the SENDER'S credentials. Keep them secure. !!!
// You MUST generate an App Password for your Gmail account.
// Go to Google Account -> Security -> 2-Step Verification -> App Passwords.
#define AUTHOR_EMAIL "example@gmail.com"
#define AUTHOR_PASSWORD "**** **** **** ****" // This is your Gmail App Password

// Gmail SMTP server settings
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT esp_mail_smtp_port_465

// ------------------------------------------ IR Sensor Configuration ------------------------------------------
#define SIG_PIN 15       // Digital pin connected to the IR sensor's data output.
int flag = 0;            // Flag to prevent multiple emails from a single IR trigger
#define ON_Board_LED 2   // On-board LED for status indication

// ------------------------------------------ LED Blink Configuration ------------------------------------------
unsigned long ledPreviousMillis = 0; // Will store last time LED was updated
int ledState = HIGH;                 // ledState used to set the LED. HIGH is off.

// ------------------------------------------ Email Cooldown Configuration ------------------------------------------
const unsigned long EMAIL_COOLDOWN_MS = 30000; // 30 seconds
unsigned long lastEmailSentTime = 0;

// ------------------------------------------ Global Email Objects ------------------------------------------
SMTPSession smtp;
ESP_Mail_Session session;
SMTP_Message message;

// ------------------------------------------ Button Configuration ------------------------------------------
#define BUTTON_PIN 32 // Choose an unused GPIO pin, e.g., 32, 33, 34, 35, 36 (avoid 0, 1, 3 for boot/serial)
                      // GPIO 34, 35, 36, 39 are input-only pins, good for buttons.
const unsigned long DEBOUNCE_DELAY_MS = 50; // Debounce time
unsigned long lastButtonPressTime = 0;
int lastButtonState = HIGH; // Assume button is not pressed initially (with INPUT_PULLUP)
bool buttonPressedFlag = false; // Flag to indicate a debounced press

// ------------------------------------------ Function Prototypes ------------------------------------------
void smtpCallback(SMTP_Status status);
void connectToWiFi();
void startAPMode();
void handleRoot();
void handleSave();
void checkButton(); // New function prototype

// ------------------------------------------ Setup Function ------------------------------------------
void setup()
{
    Serial.begin(115200);
    pinMode(ON_Board_LED, OUTPUT);
    digitalWrite(ON_Board_LED, HIGH); // Turn LED off initially

    pinMode(SIG_PIN, INPUT); // Set IR sensor pin as input

    // Configure the button pin
    pinMode(BUTTON_PIN, INPUT_PULLUP); // Use internal pull-up resistor

    Serial.println();
    Serial.println("--- Smart Mailbox Initializing ---");

    // Start Preferences and load credentials
    // "mailbox-creds" is our storage namespace
    preferences.begin("mailbox-creds", false);
    wifi_ssid = preferences.getString("ssid", "");
    wifi_password = preferences.getString("password", "");
    recipient_email = preferences.getString("recipient", "");

    // Check if we have saved credentials. If not, start Configuration Mode.
    if (wifi_ssid == "" || recipient_email == "") {
        Serial.println("No saved credentials found. Starting Configuration Mode.");
        digitalWrite(ON_Board_LED, LOW); // Turn LED ON to indicate config mode
        startAPMode(); // This function will loop until credentials are saved
    } else {
        Serial.println("Credentials found. Attempting to connect to WiFi...");
        connectToWiFi();

        // If WiFi connected successfully, proceed with normal operation
        if (WiFi.status() == WL_CONNECTED) {
            // --- Configure SMTP ---
            smtp.debug(1);
            smtp.callback(smtpCallback);

            // --- Configure Email Session ---
            session.server.host_name = SMTP_HOST;
            session.server.port = SMTP_PORT;
            session.login.email = AUTHOR_EMAIL;
            session.login.password = AUTHOR_PASSWORD;

            // Set Time properties for secure connection (certificate validation)
            session.time.ntp_server = F("pool.ntp.org,time.nist.gov");
            session.time.gmt_offset = 2; // Germany is UTC+2 (CEST) in summer
            session.time.day_light_offset = 0;

            // --- Configure Email Message ---
            message.sender.name = "Smart Mail Box";
            message.sender.email = AUTHOR_EMAIL;
            message.subject = "Hi, You have received a mail";
            // Use the recipient email loaded from preferences
            message.addRecipient("Mailbox Owner", recipient_email.c_str());

            String textMsg = "You have received a mail in your mail box!";
            message.text.content = textMsg.c_str();
            message.text.charSet = "UTF-8";
            message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;

            lastEmailSentTime = -EMAIL_COOLDOWN_MS; // Allows immediate email after boot if IR triggers

            Serial.println("\nIR Sensor and Email setup complete. Waiting for IR signal...");
        } else {
            // Handle case where saved credentials are bad
            Serial.println("Failed to connect with saved credentials.");
            Serial.println("Clearing credentials and restarting in Configuration Mode in 10s...");
            delay(10000);
            preferences.clear(); // Clear the bad credentials
            ESP.restart();       // Restart the ESP32
        }
    }
}

// ------------------------------------------ Main Loop Function ------------------------------------------
void loop()
{
    // Check button state first in both modes
    checkButton(); // Call the new button handling function

    // --- CONFIGURATION MODE ---
    // If we are in AP mode, handle web server requests and keep the LED solid ON.
    if (WiFi.getMode() == WIFI_AP) {
        dnsServer.processNextRequest();
        server.handleClient();
        digitalWrite(ON_Board_LED, LOW); // Keep LED solid ON for config mode
        return; // Skip the rest of the loop
    }

    // --- NORMAL OPERATION MODE ---

    // 1. Handle WiFi connection status
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected. Reconnecting...");
        connectToWiFi(); // This function now blinks the LED while trying to connect
    }

    // 2. Handle core logic (IR sensor and email) - This takes precedence over idle blinking
    // Also ensures we are connected to WiFi before attempting to send email
    if (WiFi.status() == WL_CONNECTED && digitalRead(SIG_PIN) == LOW && flag == 0 && (millis() - lastEmailSentTime > EMAIL_COOLDOWN_MS)) {
        Serial.println("IR Signal detected!");
        digitalWrite(ON_Board_LED, LOW); // Turn LED solid ON during the sending process

        if (!smtp.connect(&session)) {
            Serial.printf("Error connecting to SMTP server: %s\n", smtp.errorReason().c_str());
        } else {
             if (!MailClient.sendMail(&smtp, &message, true)) { // The 'true' parameter is for non-blocking send
                Serial.println("Error sending Email, " + smtp.errorReason());
            } else {
                Serial.println("Email sending initiated.");
                lastEmailSentTime = millis();
            }
        }

        // Wait a bit for email client to process, then turn LED off
        // Note: For a blocking send, this LED would stay on longer.
        // With non-blocking (true in MailClient.sendMail), the callback handles success/failure.
        // We'll turn it off here quickly as the send is just initiated.
        digitalWrite(ON_Board_LED, HIGH); // Turn LED OFF when done with the attempt
        flag = 1; // Set flag to prevent re-triggering
    }
    else if (flag == 1 && digitalRead(SIG_PIN) == HIGH) {
        Serial.println("IR Signal cleared. Resetting flag.");
        flag = 0; // Reset flag for the next event
    }
    // 3. Handle idle behavior (continuous blinking) if connected and not sending email
    else if (WiFi.status() == WL_CONNECTED && flag == 0) {
        unsigned long currentMillis = millis();
        // Slow blink every 1.5 seconds when connected and idle
        if (currentMillis - ledPreviousMillis >= 1500) {
            ledPreviousMillis = currentMillis;
            ledState = !ledState; // Toggle state
            digitalWrite(ON_Board_LED, ledState);
        }
    }

    delay(10); // Small delay to keep the loop from running too fast
}


// ------------------------------------------ WiFi Connection Function (Normal Mode) ------------------------------------------
void connectToWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(wifi_ssid);

    WiFi.mode(WIFI_STA); // Ensure STA mode
    // Use the credentials loaded from Preferences
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

    int retries = 0;
    // Blink the LED while trying to connect to WiFi
    while (WiFi.status() != WL_CONNECTED && retries < 40) { // 40 * 500ms = 20 seconds timeout
        // Check for button press even during WiFi connection attempt
        checkButton();
        if (WiFi.status() == WL_CONNECTED) break; // Exit if button press caused restart

        digitalWrite(ON_Board_LED, LOW);  // LED ON
        delay(250);
        digitalWrite(ON_Board_LED, HIGH); // LED OFF
        delay(250);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        digitalWrite(ON_Board_LED, HIGH); // Turn LED OFF to let the main loop's blinker take over
    } else {
        Serial.println("\nFailed to connect to WiFi.");
        digitalWrite(ON_Board_LED, LOW); // Solid ON to indicate a persistent connection failure
    }
}

// ------------------------------------------ Configuration Mode Functions ------------------------------------------

// This function sets up the ESP32 as an Access Point and starts the web server
void startAPMode() {
    // Set ESP32 in Access Point mode
    WiFi.mode(WIFI_AP); // Explicitly set mode
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP Created with SSID: ");
    Serial.println(AP_SSID);
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Start DNS server for captive portal functionality
    // This makes it so any web address typed into the browser redirects to our page
    dnsServer.start(53, "*", IP);

    // Define web server routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleRoot); // Redirect all other requests to the root page

    server.begin(); // Start the server
    Serial.println("Web server started. Connect to the AP and open 192.168.4.1");
    // The main loop() will now handle client requests until restart.
}

// Serves the HTML for the configuration page
void handleRoot() {
    // Use a raw literal string to easily write HTML code
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Smart Mailbox Setup</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: -apple-system, system-ui, BlinkMacSystemFont, "Segoe UI", "Roboto", "Helvetica Neue", Arial, sans-serif; background-color: #f0f2f5; margin: 0; padding: 20px; color: #333; }
  .container { max-width: 500px; margin: 30px auto; background: #fff; padding: 25px 30px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); }
  h2 { text-align: center; color: #1c1e21; margin-bottom: 25px; }
  label { display: block; margin-bottom: 8px; font-weight: 600; color: #606770; }
  input[type="text"], input[type="password"], input[type="email"] { width: 100%; padding: 12px; margin-bottom: 20px; border: 1px solid #dddfe2; border-radius: 6px; box-sizing: border-box; font-size: 16px; }
  input:focus { border-color: #4CAF50; outline: none; }
  input[type="submit"] { background-color: #4CAF50; color: white; padding: 14px 20px; border: none; border-radius: 6px; cursor: pointer; width: 100%; font-size: 18px; font-weight: bold; }
  input[type="submit"]:hover { background-color: #45a049; }
</style>
</head>
<body>
<div class="container">
  <h2>Smart Mailbox Configuration</h2>
  <form action="/save" method="post">
    <label for="ssid">WiFi Network Name (SSID):</label>
    <input type="text" id="ssid" name="ssid" placeholder="YourHomeWiFi" required>
    <label for="password">WiFi Password:</label>
    <input type="password" id="password" name="password">
    <label for="recipient">Recipient Email Address:</label>
    <input type="email" id="recipient" name="recipient" placeholder="your.email@example.com" required>
    <input type="submit" value="Save and Connect">
  </form>
</div>
</body>
</html>
)rawliteral";
    server.send(200, "text/html", html);
}

// Handles saving the credentials and restarting the ESP32
void handleSave() {
    Serial.println("Save request received. Storing new credentials.");

    // Retrieve the form data from the request arguments
    String new_ssid = server.arg("ssid");
    String new_password = server.arg("password");
    String new_recipient = server.arg("recipient");

    // Save the data to Preferences for permanent storage
    preferences.putString("ssid", new_ssid);
    preferences.putString("password", new_password);
    preferences.putString("recipient", new_recipient);

    Serial.println("Credentials saved:");
    Serial.print("SSID: "); Serial.println(new_ssid);
    Serial.print("Recipient: "); Serial.println(new_recipient);

    // Send a success response page to the client
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Saved!</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: -apple-system, system-ui, sans-serif; background-color: #f0f2f5; margin: 40px; text-align: center; }
  .message { background: #fff; padding: 40px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); display: inline-block; }
  h2 { color: #4CAF50; }
  p { font-size: 18px; color: #333; }
</style>
</head>
<body>
<div class="message">
  <h2>Success!</h2>
  <p>Credentials saved. The device will now restart and connect to your WiFi.</p>
</div>
</body>
</html>
)rawliteral";
    server.send(200, "text/html", html);

    // Give the client time to receive the response, then restart the ESP32
    delay(1500);
    ESP.restart();
}

// ------------------------------------------ Button Handling Function ------------------------------------------
void checkButton() {
    int currentButtonState = digitalRead(BUTTON_PIN); // Read the current state

    // If the button state has changed (from HIGH to LOW for a press with INPUT_PULLUP)
    if (currentButtonState != lastButtonState) {
        lastButtonPressTime = millis(); // Reset the debounce timer
    }

    // After debounce delay, if the button state is still LOW (pressed) and it wasn't already registered
    if ((millis() - lastButtonPressTime) > DEBOUNCE_DELAY_MS) {
        if (currentButtonState == LOW && !buttonPressedFlag) {
            Serial.println("Button Pressed! Entering Configuration Mode...");
            buttonPressedFlag = true; // Set flag to prevent repeated actions

            // Disconnect from current WiFi (if connected)
            WiFi.disconnect(true); // true parameter clears saved WiFi credentials
            delay(100); // Give it a moment to disconnect

            // Clear stored preferences for WiFi and recipient to force re-setup
            preferences.clear(); // Clears all stored preferences in the "mailbox-creds" namespace
            preferences.end();   // Close preferences

            Serial.println("WiFi disconnected and preferences cleared. Restarting...");
            digitalWrite(ON_Board_LED, LOW); // Indicate action with solid LED
            delay(2000); // Give user visual feedback
            ESP.restart(); // Restart the ESP32, which will then start in AP mode
        }
        // If button is released (HIGH) and it was previously pressed
        else if (currentButtonState == HIGH && buttonPressedFlag) {
            buttonPressedFlag = false; // Reset the flag
        }
    }

    lastButtonState = currentButtonState; // Save the current state for the next loop iteration
}

// ------------------------------------------ SMTP Callback Function (Unchanged) ------------------------------------------
void smtpCallback(SMTP_Status status)
{
    Serial.print("Email Status: ");
    Serial.println(status.info());

    if (status.success())
    {
        Serial.println("----------------");
        ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
        ESP_MAIL_PRINTF("Message sent failed: %d\n", status.failedCount());
        Serial.println("----------------\n");
    }
}