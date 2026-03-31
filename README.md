# Feeder Control System - Flask Web Interface

A comprehensive web-based control system for managing 5 feeders and 25 dispensers with relay control, built with Flask and Bootstrap.

## 📋 Table of Contents
- [Overview](#overview)
- [Features](#features)
- [System Architecture](#system-architecture)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage Guide](#usage-guide)
- [API Documentation](#api-documentation)
- [Troubleshooting](#troubleshooting)
- [Development](#development)
- [License](#license)

## 🎯 Overview

This project provides a web-based interface for controlling a feeding system consisting of:
- **5 Feeders** (controlled by servos via PCA9685 #1)
- **25 Dispensers** (controlled by servos via PCA9685 #1 and #2)
- **2-Channel Relay Module** for additional equipment control

The system is designed to run on a Flask server that communicates with an ESP32 microcontroller running the actual hardware control logic.

## ✨ Features

### Control Panel
- Real-time status monitoring of all 25 dispensers
- Individual dispenser feed state configuration (Feed/Don't Feed)
- Grouped display by feeder (5 dispensers per feeder)
- Start/Stop/Pause feeding operations
- Visual feedback of system state (IDLE, RUNNING, PAUSED)

### Relay Control
- Individual control of 2 relay channels
- Visual status indicators
- Toggle buttons for each relay
- "Both ON/OFF" buttons for simultaneous control
- Real-time status updates

### Settings Management
- Adjustable feeder parameters:
  - Return time (100-10000 ms)
  - Movement angle (0-180°)
- Dispenser configuration:
  - Open time (1000-30000 ms)
  - Open angle (0-180°)
  - Closed angle (0-180°)
- Timing settings:
  - Dispenser to feeder delay (100-5000 ms)
  - Cycle interval (100-5000 ms)

## 🏗 System Architecture
