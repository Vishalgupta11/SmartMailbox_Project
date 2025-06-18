# SmartMailbox_Project

## Table of Contents
* [Overview](#overview)
* [Features](#features)
* [Hardware Requirements](#hardware-requirements)
* [Wiring Diagram](#wiring-diagram)
* [Software Setup](#software-setup)
* [Usage](#usage)
* [Troubleshooting](#troubleshooting)

## Overview

This project implements a "Smart Mailbox" system using an ESP32 microcontroller, an IR (Infrared) sensor, and email notifications. When mail is detected in the mailbox by the IR sensor, the ESP32 connects to Wi-Fi and sends an email notification to a predefined recipient. The device also features a configuration portal (hotspot) for easy Wi-Fi and recipient email setup, and a button to re-enter configuration mode.

## Features

* **IR Sensor Mail Detection:** Detects when mail is placed in the mailbox.
* **Email Notifications:** Sends an email alert to a specified recipient via SMTP (Gmail App Password required for sender).
* **Wi-Fi Provisioning:** Built-in web-based configuration portal (hotspot) to easily set up Wi-Fi credentials and recipient email address.
* **Persistent Configuration:** Saves Wi-Fi and recipient email in ESP32's non-volatile storage (NVS) using `Preferences`.
* **Configuration Mode Button:** A dedicated button to force the device back into configuration (hotspot) mode, clearing saved credentials.
* **LED Status Indicator:** On-board LED provides visual feedback on Wi-Fi connection status, configuration mode, and email sending activity.
* **Email Cooldown:** Prevents spamming by enforcing a 30-second delay between email notifications.

## Hardware Requirements

* **ESP32 Development Board:** Any standard ESP32 board (e.g., ESP32 DevKitC, NodeMCU-32S).
* **IR Infrared Obstacle Avoidance Sensor:** A module with a digital output (e.g., KY-032, or a similar sensor with active low output).
* **Push Button:** A momentary push button.
* (Optional, for external LED) 220 Ohm - 1k Ohm Resistor and a standard LED.

## Wiring Diagram

Connect the components to your ESP32 as follows:

* **IR Sensor (SIG_PIN):**
    * `VCC` -> ESP32 3.3V
    * `GND` -> ESP32 GND
    * `OUT` (or `SIG`) -> **GPIO 15** (defined as `SIG_PIN`)

* **Push Button (BUTTON_PIN):**
    * One side of the button -> **GPIO 32** (defined as `BUTTON_PIN`)
    * Other side of the button -> ESP32 **GND**
    *(No external resistor needed for the button, as `INPUT_PULLUP` is used in code)*

* **On-board LED (ON_Board_LED):**
    * No external wiring needed. This code uses the built-in LED typically connected to **GPIO 2** on most ESP32 development boards. Its state is usually active-low (HIGH = OFF, LOW = ON).

## Software Setup

### Prerequisites

* **PlatformIO IDE Extension:** Installed in Visual Studio Code.
* **Git:** Installed on your system for version control.

### Project Setup

1.  **Clone the Repository:**
    ```bash
    git clone [https://github.com/Vishalgupta11/SmartMailbox_Project.git](https://github.com/Vishalgupta11/SmartMailbox_Project.git)
    cd SmartMailbox_Project
    ```

2.  **Open in VS Code with PlatformIO:**
    * Open Visual Studio Code.
    * Go to `File > Open Folder...` and select the `SmartMailbox_Project` folder you just cloned.
    * PlatformIO should detect the project.

3.  **PlatformIO Configuration (`platformio.ini`):**
    The `platformio.ini` file defines your project's environment and dependencies. It should already be configured from the repository.
    ```ini
    ; platformio.ini content
    [env:esp32dev]
    platform = espressif32
    board = esp32dev
    framework = arduino
    monitor_speed = 115200
    upload_speed = 921600
    lib_deps =
        preferences
        mobizt/ESP Mail Client@^2.0.0
    ```

4.  **Install Libraries:**
    PlatformIO will automatically install the specified libraries (`Preferences` and `ESP Mail Client`) when you build the project for the first time. If not, click the "Check" (build) icon in the PlatformIO toolbar.

5.  **Sender Email Configuration:**
    **IMPORTANT:** You must configure your sender email credentials in `src/main.cpp`.
    * Open `src/main.cpp`.
    * Locate the following lines and update them:
        ```cpp
        #define AUTHOR_EMAIL "your.gmail.address@gmail.com" // Replace with your Gmail address
        #define AUTHOR_PASSWORD "your_gmail_app_password" // Replace with your Gmail App Password
        ```
    * **How to get a Gmail App Password:**
        1.  Go to your Google Account.
        2.  Navigate to `Security`.
        3.  Under "Signing in to Google", ensure `2-Step Verification` is ON.
        4.  Below 2-Step Verification, find `App passwords`.
        5.  Generate a new App password for your "Mail" app and "Other device". Copy the generated 16-character password (e.g., `abcd efgh ijkl mnop`). This is what you'll use for `AUTHOR_PASSWORD`. **Do not use your regular Gmail password.**

## Usage

### 1. Initial Setup (Configuration Mode)

When the ESP32 powers on for the first time, or if credentials are not found/cleared:

1.  The on-board LED will light up **solid ON**.
2.  The ESP32 will create a Wi-Fi Access Point (hotspot) named `SmartMailBox_Setup`.
3.  Connect to this Wi-Fi network from your phone or computer. The password is `12345678`.
4.  A captive portal should automatically open in your browser, or you can manually navigate to `http://192.168.4.1`.
5.  On the setup page:
    * Enter your home Wi-Fi SSID (network name).
    * Enter your home Wi-Fi Password.
    * Enter the Recipient Email Address (where you want mail notifications to be sent).
6.  Click "Save and Connect".
7.  The ESP32 will restart and attempt to connect to your home Wi-Fi.

### 2. Normal Operation

Once connected to your home Wi-Fi:

* The on-board LED will **slowly blink** (1.5-second interval) to indicate it's connected and waiting for a signal.
* When the IR sensor detects an object (e.g., mail in the box):
    * The LED will turn **solid ON** briefly.
    * An email notification will be sent to the configured recipient email address.
    * After sending, the LED will return to slow blinking.
* There is a **30-second cooldown** after sending an email to prevent rapid-fire notifications from multiple detections.

### 3. Re-entering Configuration Mode

If you need to change Wi-Fi credentials or the recipient email:

* Press and hold the **push button** connected to GPIO 32.
* The ESP32 will disconnect from Wi-Fi, clear its saved credentials, and restart into Configuration Mode. Follow the "Initial Setup" steps again.

## Troubleshooting

* **LED Solid ON but no hotspot:** Check if the device previously connected to Wi-Fi with invalid credentials. Wait for 10 seconds (as per code's restart logic) or manually press the setup button.
* **Cannot connect to `SmartMailBox_Setup` hotspot:** Double-check the hotspot password (`12345678`).
* **No captive portal / `192.168.4.1` not working:** Try forgetting the `SmartMailBox_Setup` network on your device and reconnecting. Ensure no VPN or proxy is active on your connected device.
* **Emails not sending:**
    * Verify your `AUTHOR_EMAIL` and `AUTHOR_PASSWORD` (App Password!) in `src/main.cpp` are correct.
    * Ensure 2-Step Verification is enabled on your Gmail account.
    * Check your internet connection on the ESP32.
    * Check the serial monitor for any SMTP error messages.
    * The subject line `Hi Vishal, you have received a mail` and sender name `Smart Mail Box` are hardcoded. You might want to change them.
* **IR Sensor not triggering:**
    * Ensure the IR sensor is wired correctly (VCC, GND, and SIG_PIN to GPIO 15).
    * Check the sensor's sensitivity (it often has a potentiometer to adjust detection range).
    * Verify `SIG_PIN` is correctly set as `INPUT`. The code expects an `active LOW` signal on detection.

---

Feel free to contribute, open issues, or suggest improvements!
