# 🚤 Eco-Hydro Cleanser (Autonomous Debris Collector)

An autonomous water debris collection boat powered by an ESP32 microcontroller. The system features dual HC-SR04 ultrasonic sensors for object detection and differential steering, high-power BTS7960 motor drivers for propulsion, an L298N driver for the waste pickup lever mechanism, and wireless program updating via **ArduinoOTA**.

---

## 🛠️ Hardware Requirements & Components

* **Microcontroller:** ESP32 Dev Module (30-Pin)
* **Propulsion Motor Drivers:** 2x BTS7960 43A High-Power H-Bridge Drivers
* **Mechanism Motor Driver:** 1x L298N Dual H-Bridge Driver
* **Sensors:** 2x HC-SR04 Ultrasonic Distance Sensors (Left & Right)
* **Power Source:** 2S/3S LiPo Battery

---

## 📌 Wiring & Pinout Guide

### 1. Propulsion Drivers (BTS7960)
| Driver | Function | ESP32 GPIO Pin |
| :--- | :--- | :--- |
| **BTS7960 #1 (Left Motor)** | RPWM (Forward) | GPIO 25 |
| | LPWM (Reverse) | GPIO 26 |
| **BTS7960 #2 (Right Motor)**| RPWM (Forward) | GPIO 27 |
| | LPWM (Reverse) | GPIO 14 |

### 2. Pickup Mechanism (L298N Driver)
| Driver Pin | Function | ESP32 GPIO Pin |
| :--- | :--- | :--- |
| **ENA / ENB** | Speed Control (PWM) | GPIO 23 / GPIO 13 |
| **IN1 / IN2** | Motor Direction Set A | GPIO 32 / GPIO 33 |
| **IN3 / IN4** | Motor Direction Set B | GPIO 18 / GPIO 19 |

### 3. Ultrasonic Sensors (HC-SR04)
| Sensor | Function | ESP32 GPIO Pin |
| :--- | :--- | :--- |
| **Left Sensor** | Trigger / Echo | GPIO 4 / GPIO 34 *(Input Only)* |
| **Right Sensor**| Trigger / Echo | GPIO 5 / GPIO 35 *(Input Only)* |

---

## 🚀 Key Features

* **Autonomous Tracking:** Automatically scans for target debris under 20 cm and adjusts propulsion.
* **Auto Collection Routine:** Halts at 5 cm, raises the pickup lever, drops debris into the bin, lowers the lever, and reverses out safely.
* **Wireless OTA Updates:** Flash code changes over Wi-Fi without needing a USB cable.
* **Serial Telemetry & Manual Override:** Send manual movement commands (`F`, `B`, `L`, `R`, `U`, `D`, `S`, `A`) over Serial at 115200 baud.
