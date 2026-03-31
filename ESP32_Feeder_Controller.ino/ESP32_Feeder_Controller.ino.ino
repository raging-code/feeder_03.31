void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}
/**
 * ESP32 Feeder Controller - Hardware Control Code
 * This code runs on the ESP32 and directly controls:
 * - 5 Feeders (servos via PCA9685 #1)
 * - 25 Dispensers (servos via PCA9685 #1 and #2)
 * - 2-Channel Relay Module
 * 
 * This version is compatible with the Flask web interface
 * All API endpoints remain the same as the original
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <ArduinoJson.h>

// ============================================
// PIN DEFINITIONS
// ============================================
#define RELAY1_PIN 12  // GPIO12 for Relay 1 (Channel 1)
#define RELAY2_PIN 13  // GPIO13 for Relay 2 (Channel 2)

// ============================================
// RELAY STATES
// ============================================
bool relay1State = false;  // false = OFF, true = ON
bool relay2State = false;

// ============================================
// PCA9685 I2C ADDRESSES
// ============================================
#define PCA9685_ADDR1 0x40  // Default address (A0-A2 not connected)
#define PCA9685_ADDR2 0x41  // Address with A0 shorted (A0 = 1)

// ============================================
// PCA9685 SERVO DRIVER OBJECTS
// ============================================
Adafruit_PWMServoDriver pwm1 = Adafruit_PWMServoDriver(PCA9685_ADDR1);
Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(PCA9685_ADDR2);

// ============================================
// SERVO CALIBRATION VALUES
// ============================================
#define SERVO_MIN_PULSE_WIDTH 500   // Minimum pulse width in microseconds
#define SERVO_MAX_PULSE_WIDTH 2500  // Maximum pulse width in microseconds
#define SERVO_FREQUENCY 50          // Standard servo frequency (50Hz)

// ============================================
// PCA9685 CHANNEL ALLOCATION - PCA9685 #1 (Address 0x40)
// ============================================
// Channels 0-4: Feeders 1-5
#define SERVO_FEEDER1_CHANNEL 0
#define SERVO_FEEDER2_CHANNEL 1
#define SERVO_FEEDER3_CHANNEL 2
#define SERVO_FEEDER4_CHANNEL 3
#define SERVO_FEEDER5_CHANNEL 4

// Channels 5-15: Dispensers 1-11
#define SERVO_DISPENSE1_CHANNEL 5
#define SERVO_DISPENSE2_CHANNEL 6
#define SERVO_DISPENSE3_CHANNEL 7
#define SERVO_DISPENSE4_CHANNEL 8
#define SERVO_DISPENSE5_CHANNEL 9
#define SERVO_DISPENSE6_CHANNEL 10
#define SERVO_DISPENSE7_CHANNEL 11
#define SERVO_DISPENSE8_CHANNEL 12
#define SERVO_DISPENSE9_CHANNEL 13
#define SERVO_DISPENSE10_CHANNEL 14
#define SERVO_DISPENSE11_CHANNEL 15

// ============================================
// PCA9685 CHANNEL ALLOCATION - PCA9685 #2 (Address 0x41)
// ============================================
#define SERVO_DISPENSE12_CHANNEL 0
#define SERVO_DISPENSE13_CHANNEL 14  // Channel 14 for Dispenser 13
#define SERVO_DISPENSE14_CHANNEL 2
#define SERVO_DISPENSE15_CHANNEL 15  // Channel 15 for Dispenser 15
#define SERVO_DISPENSE16_CHANNEL 4
#define SERVO_DISPENSE17_CHANNEL 5
#define SERVO_DISPENSE18_CHANNEL 6
#define SERVO_DISPENSE19_CHANNEL 7
#define SERVO_DISPENSE20_CHANNEL 8
#define SERVO_DISPENSE21_CHANNEL 9
#define SERVO_DISPENSE22_CHANNEL 10
#define SERVO_DISPENSE23_CHANNEL 11
#define SERVO_DISPENSE24_CHANNEL 12
#define SERVO_DISPENSE25_CHANNEL 13

// ============================================
// DEFAULT SETTINGS
// ============================================
int feederInterval = 1000;          // 1 second for feeder to return
int feederAngle = 50;               // Feeder movement angle
int dispenserOpenTime = 2000;       // 2 seconds for dispenser to stay open
int dispenserAngle = 0;             // Dispenser opens to 0 degrees
int dispenserClosedAngle = 70;      // Dispenser closed position (70 degrees)
int intervalBetweenFeederAndDispenser = 1000; // Delay between dispenser open and feeder move
int intervalBetweenDispensers = 1000;         // Delay between cycles

// ============================================
// DISPENSER STATES (25 dispensers)
// ============================================
bool dispenserStates[25] = {true, true, true, true, true, true, true, true, true, true,
                            true, true, true, true, true, true, true, true, true, true,
                            true, true, true, true, true};

// ============================================
// FEEDER STATE MACHINE
// Each feeder has its own state machine for sequential processing
// ============================================
struct FeederProcess {
    enum FeederState {
        FEEDER_IDLE,
        OPEN_DISPENSER,
        WAIT_DISPENSER_OPEN,
        MOVE_FEEDER,
        WAIT_FEEDER,
        RETURN_FEEDER,
        WAIT_FEEDER_RETURN,
        CLOSE_DISPENSER,
        WAIT_DISPENSER_CLOSE,
        NEXT_DISPENSER,
        FEEDER_COMPLETED
    };
    
    FeederState state;
    int currentDispenser;      // 0-4 relative to feeder
    unsigned long timer;
    bool active;
    bool dispenserNeedsFeed[5];
    bool completed;
};

// Create feeder processes for all 5 feeders
FeederProcess feeders[5];

// ============================================
// SYSTEM STATE
// ============================================
enum SystemState { IDLE, RUNNING, PAUSED };
SystemState currentState = IDLE;

// ============================================
// WiFi CREDENTIALS - UPDATE THESE
// ============================================
const char* ssid = "YOUR_WIFI_SSID";      // CHANGE THIS
const char* password = "YOUR_WIFI_PASSWORD";  // CHANGE THIS

// ============================================
// WEB SERVER
// ============================================
WebServer server(80);

// ============================================
// HELPER FUNCTIONS
// ============================================

// Map angle to pulse width for PCA9685
int angleToPulse(int angle) {
    int pulseWidth = map(angle, 0, 180, SERVO_MIN_PULSE_WIDTH, SERVO_MAX_PULSE_WIDTH);
    float pulseMicroseconds = pulseWidth;
    float periodMicroseconds = 1000000.0 / SERVO_FREQUENCY;
    float dutyCycle = pulseMicroseconds / periodMicroseconds;
    int pwmValue = dutyCycle * 4096;
    return constrain(pwmValue, 0, 4095);
}

// ============================================
// RELAY CONTROL FUNCTIONS
// ============================================
void setRelay1(bool state) {
    digitalWrite(RELAY1_PIN, state ? HIGH : LOW);
    relay1State = state;
    Serial.print("Relay 1 ");
    Serial.println(state ? "ON" : "OFF");
}

void setRelay2(bool state) {
    digitalWrite(RELAY2_PIN, state ? HIGH : LOW);
    relay2State = state;
    Serial.print("Relay 2 ");
    Serial.println(state ? "ON" : "OFF");
}

void setBothRelays(bool state) {
    setRelay1(state);
    setRelay2(state);
}

void toggleRelay1() {
    setRelay1(!relay1State);
}

void toggleRelay2() {
    setRelay2(!relay2State);
}

// ============================================
// SERVO CONTROL FUNCTIONS
// ============================================

// Stop a servo completely (remove signal)
void stopServo(uint8_t pcaNumber, uint8_t channel) {
    if(pcaNumber == 1) {
        pwm1.setPWM(channel, 0, 0);
    } else {
        pwm2.setPWM(channel, 0, 0);
    }
}

// Set servo angle
void setServoAngle(uint8_t pcaNumber, uint8_t channel, int angle) {
    int pulse = angleToPulse(angle);
    if(pcaNumber == 1) {
        pwm1.setPWM(channel, 0, pulse);
    } else {
        pwm2.setPWM(channel, 0, pulse);
    }
}

// Set dispenser angle by number (1-25)
void setDispenserAngle(int dispenserNum, int angle) {
    switch(dispenserNum) {
        case 1: setServoAngle(1, SERVO_DISPENSE1_CHANNEL, angle); break;
        case 2: setServoAngle(1, SERVO_DISPENSE2_CHANNEL, angle); break;
        case 3: setServoAngle(1, SERVO_DISPENSE3_CHANNEL, angle); break;
        case 4: setServoAngle(1, SERVO_DISPENSE4_CHANNEL, angle); break;
        case 5: setServoAngle(1, SERVO_DISPENSE5_CHANNEL, angle); break;
        case 6: setServoAngle(1, SERVO_DISPENSE6_CHANNEL, angle); break;
        case 7: setServoAngle(1, SERVO_DISPENSE7_CHANNEL, angle); break;
        case 8: setServoAngle(1, SERVO_DISPENSE8_CHANNEL, angle); break;
        case 9: setServoAngle(1, SERVO_DISPENSE9_CHANNEL, angle); break;
        case 10: setServoAngle(1, SERVO_DISPENSE10_CHANNEL, angle); break;
        case 11: setServoAngle(1, SERVO_DISPENSE11_CHANNEL, angle); break;
        case 12: setServoAngle(2, SERVO_DISPENSE12_CHANNEL, angle); break;
        case 13: setServoAngle(2, SERVO_DISPENSE13_CHANNEL, angle); break;
        case 14: setServoAngle(2, SERVO_DISPENSE14_CHANNEL, angle); break;
        case 15: setServoAngle(2, SERVO_DISPENSE15_CHANNEL, angle); break;
        case 16: setServoAngle(2, SERVO_DISPENSE16_CHANNEL, angle); break;
        case 17: setServoAngle(2, SERVO_DISPENSE17_CHANNEL, angle); break;
        case 18: setServoAngle(2, SERVO_DISPENSE18_CHANNEL, angle); break;
        case 19: setServoAngle(2, SERVO_DISPENSE19_CHANNEL, angle); break;
        case 20: setServoAngle(2, SERVO_DISPENSE20_CHANNEL, angle); break;
        case 21: setServoAngle(2, SERVO_DISPENSE21_CHANNEL, angle); break;
        case 22: setServoAngle(2, SERVO_DISPENSE22_CHANNEL, angle); break;
        case 23: setServoAngle(2, SERVO_DISPENSE23_CHANNEL, angle); break;
        case 24: setServoAngle(2, SERVO_DISPENSE24_CHANNEL, angle); break;
        case 25: setServoAngle(2, SERVO_DISPENSE25_CHANNEL, angle); break;
        default: break;
    }
}

// Stop dispenser servo
void stopDispenser(int dispenserNum) {
    switch(dispenserNum) {
        case 1: stopServo(1, SERVO_DISPENSE1_CHANNEL); break;
        case 2: stopServo(1, SERVO_DISPENSE2_CHANNEL); break;
        case 3: stopServo(1, SERVO_DISPENSE3_CHANNEL); break;
        case 4: stopServo(1, SERVO_DISPENSE4_CHANNEL); break;
        case 5: stopServo(1, SERVO_DISPENSE5_CHANNEL); break;
        case 6: stopServo(1, SERVO_DISPENSE6_CHANNEL); break;
        case 7: stopServo(1, SERVO_DISPENSE7_CHANNEL); break;
        case 8: stopServo(1, SERVO_DISPENSE8_CHANNEL); break;
        case 9: stopServo(1, SERVO_DISPENSE9_CHANNEL); break;
        case 10: stopServo(1, SERVO_DISPENSE10_CHANNEL); break;
        case 11: stopServo(1, SERVO_DISPENSE11_CHANNEL); break;
        case 12: stopServo(2, SERVO_DISPENSE12_CHANNEL); break;
        case 13: stopServo(2, SERVO_DISPENSE13_CHANNEL); break;
        case 14: stopServo(2, SERVO_DISPENSE14_CHANNEL); break;
        case 15: stopServo(2, SERVO_DISPENSE15_CHANNEL); break;
        case 16: stopServo(2, SERVO_DISPENSE16_CHANNEL); break;
        case 17: stopServo(2, SERVO_DISPENSE17_CHANNEL); break;
        case 18: stopServo(2, SERVO_DISPENSE18_CHANNEL); break;
        case 19: stopServo(2, SERVO_DISPENSE19_CHANNEL); break;
        case 20: stopServo(2, SERVO_DISPENSE20_CHANNEL); break;
        case 21: stopServo(2, SERVO_DISPENSE21_CHANNEL); break;
        case 22: stopServo(2, SERVO_DISPENSE22_CHANNEL); break;
        case 23: stopServo(2, SERVO_DISPENSE23_CHANNEL); break;
        case 24: stopServo(2, SERVO_DISPENSE24_CHANNEL); break;
        case 25: stopServo(2, SERVO_DISPENSE25_CHANNEL); break;
        default: break;
    }
}

// Set feeder angle by number (1-5)
void setFeederAngle(int feederNum, int angle) {
    switch(feederNum) {
        case 1: setServoAngle(1, SERVO_FEEDER1_CHANNEL, angle); break;
        case 2: setServoAngle(1, SERVO_FEEDER2_CHANNEL, angle); break;
        case 3: setServoAngle(1, SERVO_FEEDER3_CHANNEL, angle); break;
        case 4: setServoAngle(1, SERVO_FEEDER4_CHANNEL, angle); break;
        case 5: setServoAngle(1, SERVO_FEEDER5_CHANNEL, angle); break;
        default: break;
    }
}

// Stop feeder servo
void stopFeederServo(int feederNum) {
    switch(feederNum) {
        case 1: stopServo(1, SERVO_FEEDER1_CHANNEL); break;
        case 2: stopServo(1, SERVO_FEEDER2_CHANNEL); break;
        case 3: stopServo(1, SERVO_FEEDER3_CHANNEL); break;
        case 4: stopServo(1, SERVO_FEEDER4_CHANNEL); break;
        case 5: stopServo(1, SERVO_FEEDER5_CHANNEL); break;
        default: break;
    }
}

// Stop all servos
void stopAllServos() {
    Serial.println("STOPPING ALL SERVOS - Removing signals");
    for(int i = 1; i <= 5; i++) {
        stopFeederServo(i);
    }
    for(int i = 1; i <= 25; i++) {
        stopDispenser(i);
    }
}

// ============================================
// FEEDER STATE MACHINE FUNCTIONS
// ============================================

// Initialize all feeders
void initFeeders() {
    for(int f = 0; f < 5; f++) {
        feeders[f].state = FeederProcess::FEEDER_IDLE;
        feeders[f].currentDispenser = 0;
        feeders[f].timer = 0;
        feeders[f].active = false;
        feeders[f].completed = false;
        
        int startDisp = f * 5;
        for(int d = 0; d < 5; d++) {
            feeders[f].dispenserNeedsFeed[d] = dispenserStates[startDisp + d];
        }
    }
}

// Reset feeders for new cycle
void resetFeedersForNewCycle() {
    for(int f = 0; f < 5; f++) {
        feeders[f].state = FeederProcess::FEEDER_IDLE;
        feeders[f].currentDispenser = 0;
        feeders[f].timer = 0;
        feeders[f].completed = false;
        
        int startDisp = f * 5;
        bool hasActive = false;
        for(int d = 0; d < 5; d++) {
            feeders[f].dispenserNeedsFeed[d] = dispenserStates[startDisp + d];
            if(feeders[f].dispenserNeedsFeed[d]) hasActive = true;
        }
        feeders[f].active = hasActive;
    }
}

// Update dispenser needs for all feeders
void updateFeederDispenserNeeds() {
    for(int f = 0; f < 5; f++) {
        int startDisp = f * 5;
        bool hasActive = false;
        
        for(int d = 0; d < 5; d++) {
            feeders[f].dispenserNeedsFeed[d] = dispenserStates[startDisp + d];
            if(feeders[f].dispenserNeedsFeed[d]) hasActive = true;
        }
        
        feeders[f].active = hasActive;
        
        if(!hasActive) {
            feeders[f].state = FeederProcess::FEEDER_COMPLETED;
            feeders[f].completed = true;
            stopFeederServo(f + 1);
        }
    }
}

// Process a single feeder's state machine
void processFeeder(int feederIndex) {
    if(currentState != RUNNING) return;
    if(!feeders[feederIndex].active) return;
    if(feeders[feederIndex].completed) return;
    
    unsigned long currentTime = millis();
    int feederNum = feederIndex + 1;
    int absoluteDispenserNum = (feederIndex * 5) + feeders[feederIndex].currentDispenser + 1;
    
    switch(feeders[feederIndex].state) {
        case FeederProcess::FEEDER_IDLE:
            feeders[feederIndex].currentDispenser = 0;
            
            while(feeders[feederIndex].currentDispenser < 5 && 
                  !feeders[feederIndex].dispenserNeedsFeed[feeders[feederIndex].currentDispenser]) {
                feeders[feederIndex].currentDispenser++;
            }
            
            if(feeders[feederIndex].currentDispenser < 5) {
                absoluteDispenserNum = (feederIndex * 5) + feeders[feederIndex].currentDispenser + 1;
                Serial.print("Feeder ");
                Serial.print(feederNum);
                Serial.print(" starting with Dispenser ");
                Serial.println(absoluteDispenserNum);
                feeders[feederIndex].state = FeederProcess::OPEN_DISPENSER;
            } else {
                feeders[feederIndex].completed = true;
                feeders[feederIndex].state = FeederProcess::FEEDER_COMPLETED;
            }
            break;
            
        case FeederProcess::OPEN_DISPENSER:
            setDispenserAngle(absoluteDispenserNum, dispenserAngle);
            Serial.print("Feeder ");
            Serial.print(feederNum);
            Serial.print(" - Dispenser ");
            Serial.print(absoluteDispenserNum);
            Serial.println(" opened");
            feeders[feederIndex].timer = currentTime;
            feeders[feederIndex].state = FeederProcess::WAIT_DISPENSER_OPEN;
            break;
            
        case FeederProcess::WAIT_DISPENSER_OPEN:
            if(currentTime - feeders[feederIndex].timer >= intervalBetweenFeederAndDispenser) {
                feeders[feederIndex].state = FeederProcess::MOVE_FEEDER;
            }
            break;
            
        case FeederProcess::MOVE_FEEDER:
            setFeederAngle(feederNum, feederAngle);
            Serial.print("Feeder ");
            Serial.print(feederNum);
            Serial.print(" moved to ");
            Serial.print(feederAngle);
            Serial.println(" degrees");
            feeders[feederIndex].timer = currentTime;
            feeders[feederIndex].state = FeederProcess::WAIT_FEEDER;
            break;
            
        case FeederProcess::WAIT_FEEDER:
            if(currentTime - feeders[feederIndex].timer >= feederInterval) {
                setFeederAngle(feederNum, 0);
                Serial.print("Feeder ");
                Serial.print(feederNum);
                Serial.println(" returning to 0 degrees");
                feeders[feederIndex].timer = currentTime;
                feeders[feederIndex].state = FeederProcess::WAIT_FEEDER_RETURN;
            }
            break;
            
        case FeederProcess::WAIT_FEEDER_RETURN:
            if(currentTime - feeders[feederIndex].timer >= 500) {
                stopFeederServo(feederNum);
                Serial.print("Feeder ");
                Serial.print(feederNum);
                Serial.println(" STOPPED");
                feeders[feederIndex].state = FeederProcess::CLOSE_DISPENSER;
                feeders[feederIndex].timer = currentTime;
            }
            break;
            
        case FeederProcess::CLOSE_DISPENSER:
            if(currentTime - feeders[feederIndex].timer >= dispenserOpenTime) {
                setDispenserAngle(absoluteDispenserNum, dispenserClosedAngle);
                Serial.print("Feeder ");
                Serial.print(feederNum);
                Serial.print(" - Dispenser ");
                Serial.print(absoluteDispenserNum);
                Serial.println(" closed");
                feeders[feederIndex].state = FeederProcess::WAIT_DISPENSER_CLOSE;
                feeders[feederIndex].timer = currentTime;
            }
            break;
            
        case FeederProcess::WAIT_DISPENSER_CLOSE:
            if(currentTime - feeders[feederIndex].timer >= intervalBetweenDispensers) {
                stopDispenser(absoluteDispenserNum);
                feeders[feederIndex].state = FeederProcess::NEXT_DISPENSER;
            }
            break;
            
        case FeederProcess::NEXT_DISPENSER:
            feeders[feederIndex].currentDispenser++;
            
            while(feeders[feederIndex].currentDispenser < 5 && 
                  !feeders[feederIndex].dispenserNeedsFeed[feeders[feederIndex].currentDispenser]) {
                feeders[feederIndex].currentDispenser++;
            }
            
            if(feeders[feederIndex].currentDispenser < 5) {
                feeders[feederIndex].state = FeederProcess::OPEN_DISPENSER;
            } else {
                Serial.print("Feeder ");
                Serial.print(feederNum);
                Serial.println(" completed all dispensers");
                stopFeederServo(feederNum);
                feeders[feederIndex].completed = true;
                feeders[feederIndex].state = FeederProcess::FEEDER_COMPLETED;
            }
            break;
            
        case FeederProcess::FEEDER_COMPLETED:
            break;
    }
}

// Check if all feeders are completed
bool areAllFeedersCompleted() {
    for(int f = 0; f < 5; f++) {
        if(feeders[f].active && !feeders[f].completed) {
            return false;
        }
    }
    return true;
}

// Main feeding cycle processor
void processFeedingCycle() {
    updateFeederDispenserNeeds();
    
    for(int f = 0; f < 5; f++) {
        processFeeder(f);
    }
    
    if(areAllFeedersCompleted()) {
        Serial.println("===========================================");
        Serial.println("=== ALL FEEDERS COMPLETED FEEDING CYCLE ===");
        Serial.println("===========================================");
        stopAllServos();
        currentState = IDLE;
    }
}

// ============================================
// WEB SERVER HANDLERS
// ============================================

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>ESP32 Feeder Controller</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;margin:20px;background:#f0f2f5;}";
    html += ".container{max-width:800px;margin:auto;background:white;padding:20px;border-radius:10px;}";
    html += "button{padding:10px 20px;margin:5px;background:#4CAF50;color:white;border:none;border-radius:5px;cursor:pointer;}";
    html += ".status{padding:10px;margin:10px 0;border-radius:5px;}</style></head><body>";
    html += "<div class='container'><h1>ESP32 Feeder Controller</h1>";
    html += "<p>Use the Flask web interface at: http://YOUR_FLASK_SERVER:5000</p>";
    html += "<p>ESP32 IP: " + WiFi.localIP().toString() + "</p>";
    html += "<p>System State: " + String(currentState == IDLE ? "IDLE" : currentState == RUNNING ? "RUNNING" : "PAUSED") + "</p>";
    html += "</div></body></html>";
    server.send(200, "text/html", html);
}

void handleGetSystemState() {
    String stateStr;
    switch(currentState) {
        case IDLE: stateStr = "IDLE"; break;
        case RUNNING: stateStr = "RUNNING"; break;
        case PAUSED: stateStr = "PAUSED"; break;
    }
    String json = "{\"state\":\"" + stateStr + "\",\"operation\":\"Feeding System\"}";
    server.send(200, "application/json", json);
}

void handleStartFeeding() {
    if(currentState == IDLE) {
        resetFeedersForNewCycle();
        currentState = RUNNING;
        server.send(200, "application/json", "{\"success\":true}");
        Serial.println("=== STARTING FEEDING PROCESS ===");
    } else {
        server.send(200, "application/json", "{\"success\":false,\"error\":\"System not idle\"}");
    }
}

void handleStopFeeding() {
    currentState = IDLE;
    stopAllServos();
    initFeeders();
    server.send(200, "application/json", "{\"success\":true}");
    Serial.println("=== STOPPED ===");
}

void handlePauseFeeding() {
    if(currentState == RUNNING) {
        currentState = PAUSED;
        server.send(200, "application/json", "{\"success\":true}");
        Serial.println("PAUSED");
    } else if(currentState == PAUSED) {
        currentState = RUNNING;
        server.send(200, "application/json", "{\"success\":true}");
        Serial.println("RESUMED");
    } else {
        server.send(200, "application/json", "{\"success\":false,\"error\":\"Cannot pause\"}");
    }
}

void handleSetDispenserState() {
    if(server.hasArg("index") && server.hasArg("state")) {
        int index = server.arg("index").toInt();
        bool state = server.arg("state") == "true";
        if(index >= 0 && index < 25) {
            dispenserStates[index] = state;
            server.send(200, "application/json", "{\"success\":true}");
        } else {
            server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid index\"}");
        }
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
    }
}

void handleGetDispenserStates() {
    String json = "{\"states\":[";
    for(int i = 0; i < 25; i++) {
        json += dispenserStates[i] ? "true" : "false";
        if(i < 24) json += ",";
    }
    json += "]}";
    server.send(200, "application/json", json);
}

void handleGetDispenserInfo() {
    // This is a simplified version - returns channel info
    String json = "{\"channels\":[";
    for(int i = 1; i <= 25; i++) {
        json += String(i + 4);  // Simplified channel mapping
        if(i < 25) json += ",";
    }
    json += "]}";
    server.send(200, "application/json", json);
}

void handleGetRelayStates() {
    String json = "{\"relay1\":" + String(relay1State ? "true" : "false") + 
                  ",\"relay2\":" + String(relay2State ? "true" : "false") + "}";
    server.send(200, "application/json", json);
}

void handleToggleRelay() {
    if(server.hasArg("channel")) {
        int channel = server.arg("channel").toInt();
        if(channel == 1) {
            toggleRelay1();
        } else if(channel == 2) {
            toggleRelay2();
        }
        handleGetRelayStates();
    } else {
        server.send(400, "application/json", "{\"error\":\"Channel not specified\"}");
    }
}

void handleSetBothRelays() {
    if(server.hasArg("state")) {
        bool state = server.arg("state") == "true";
        setBothRelays(state);
        handleGetRelayStates();
    } else {
        server.send(400, "application/json", "{\"error\":\"State not specified\"}");
    }
}

void handleGetSettings() {
    String json = "{";
    json += "\"feederInterval\":" + String(feederInterval) + ",";
    json += "\"feederAngle\":" + String(feederAngle) + ",";
    json += "\"dispenserOpenTime\":" + String(dispenserOpenTime) + ",";
    json += "\"dispenserAngle\":" + String(dispenserAngle) + ",";
    json += "\"dispenserClosedAngle\":" + String(dispenserClosedAngle) + ",";
    json += "\"intervalBetween\":" + String(intervalBetweenFeederAndDispenser) + ",";
    json += "\"intervalDispensers\":" + String(intervalBetweenDispensers);
    json += "}";
    server.send(200, "application/json", json);
}

void handleSaveSettings() {
    if(server.hasArg("plain")) {
        String body = server.arg("plain");
        
        // Simple JSON parsing
        if(body.indexOf("\"feederInterval\"") > 0) {
            int start = body.indexOf("\"feederInterval\"") + 18;
            int end = body.indexOf(",", start);
            if(end == -1) end = body.indexOf("}", start);
            feederInterval = constrain(body.substring(start, end).toInt(), 100, 10000);
        }
        if(body.indexOf("\"feederAngle\"") > 0) {
            int start = body.indexOf("\"feederAngle\"") + 15;
            int end = body.indexOf(",", start);
            if(end == -1) end = body.indexOf("}", start);
            feederAngle = constrain(body.substring(start, end).toInt(), 0, 180);
        }
        if(body.indexOf("\"dispenserOpenTime\"") > 0) {
            int start = body.indexOf("\"dispenserOpenTime\"") + 21;
            int end = body.indexOf(",", start);
            if(end == -1) end = body.indexOf("}", start);
            dispenserOpenTime = constrain(body.substring(start, end).toInt(), 1000, 30000);
        }
        if(body.indexOf("\"dispenserAngle\"") > 0) {
            int start = body.indexOf("\"dispenserAngle\"") + 18;
            int end = body.indexOf(",", start);
            if(end == -1) end = body.indexOf("}", start);
            dispenserAngle = constrain(body.substring(start, end).toInt(), 0, 180);
        }
        if(body.indexOf("\"dispenserClosedAngle\"") > 0) {
            int start = body.indexOf("\"dispenserClosedAngle\"") + 24;
            int end = body.indexOf(",", start);
            if(end == -1) end = body.indexOf("}", start);
            dispenserClosedAngle = constrain(body.substring(start, end).toInt(), 0, 180);
        }
        if(body.indexOf("\"intervalBetween\"") > 0) {
            int start = body.indexOf("\"intervalBetween\"") + 19;
            int end = body.indexOf(",", start);
            if(end == -1) end = body.indexOf("}", start);
            intervalBetweenFeederAndDispenser = constrain(body.substring(start, end).toInt(), 100, 5000);
        }
        if(body.indexOf("\"intervalDispensers\"") > 0) {
            int start = body.indexOf("\"intervalDispensers\"") + 21;
            int end = body.indexOf(",", start);
            if(end == -1) end = body.indexOf("}", start);
            intervalBetweenDispensers = constrain(body.substring(start, end).toInt(), 100, 5000);
        }
        
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Settings saved\"}");
        Serial.println("=== SETTINGS SAVED ===");
    } else {
        server.send(400, "application/json", "{\"success\":false}");
    }
}

// ============================================
// SETUP FUNCTION
// ============================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("===========================================");
    Serial.println("ESP32 FEEDER CONTROLLER - READY");
    Serial.println("5 Feeders | 25 Dispensers | 2 Relays");
    Serial.println("===========================================");
    
    // Initialize relay pins
    pinMode(RELAY1_PIN, OUTPUT);
    pinMode(RELAY2_PIN, OUTPUT);
    setRelay1(false);
    setRelay2(false);
    
    // Initialize I2C and PCA9685
    Wire.begin(21, 22);
    
    pwm1.begin();
    pwm1.setPWMFreq(SERVO_FREQUENCY);
    pwm2.begin();
    pwm2.setPWMFreq(SERVO_FREQUENCY);
    
    // Initialize servos to starting positions
    for(int i = 1; i <= 5; i++) {
        setFeederAngle(i, 0);
        delay(50);
    }
    for(int i = 1; i <= 25; i++) {
        setDispenserAngle(i, dispenserClosedAngle);
        delay(50);
    }
    delay(1000);
    
    // Initialize feeder state machines
    initFeeders();
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    int timeout = 0;
    while(WiFi.status() != WL_CONNECTED && timeout < 30) { 
        delay(500); 
        Serial.print("."); 
        timeout++; 
    }
    
    if(WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected!");
        Serial.print("ESP32 IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi connection failed!");
        Serial.println("Check your SSID and password");
    }
    
    // Setup web server routes
    server.on("/", handleRoot);
    server.on("/getSystemState", handleGetSystemState);
    server.on("/startFeeding", handleStartFeeding);
    server.on("/stopFeeding", handleStopFeeding);
    server.on("/pauseFeeding", handlePauseFeeding);
    server.on("/setDispenserState", handleSetDispenserState);
    server.on("/getDispenserStates", handleGetDispenserStates);
    server.on("/getDispenserInfo", handleGetDispenserInfo);
    server.on("/getRelayStates", handleGetRelayStates);
    server.on("/toggleRelay", handleToggleRelay);
    server.on("/setBothRelays", handleSetBothRelays);
    server.on("/getSettings", handleGetSettings);
    server.on("/saveSettings", HTTP_POST, handleSaveSettings);
    
    server.begin();
    Serial.println("HTTP server started");
    Serial.print("Access at: http://");
    Serial.println(WiFi.localIP());
    Serial.println("===========================================");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    server.handleClient();
    
    if(currentState == RUNNING) {
        processFeedingCycle();
    } else if(currentState == PAUSED) {
        delay(50);
    } else {
        delay(10);
    }
}