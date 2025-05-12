#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "CM.h"

// Global variables
MCP2515 mcp2515(5);
can_frame canMsg;
CM cm;

// Device name
#define DEVICE_NAME "MyStoryteller"

// BLE Service and Characteristic UUIDs
#define VAN_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define COMMAND_CHAR_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define STATUS_CHAR_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define TANK_STATUS_CHAR_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26aa"

BLEServer* pServer = nullptr;
BLECharacteristic* pCommandCharacteristic = nullptr;
BLECharacteristic* pStatusCharacteristic = nullptr;
BLECharacteristic* pTankStatusCharacteristic = nullptr;
bool deviceConnected = false;

// Forward declarations for all command handlers
void handleACCommand(String command);
void handleVentCommand(String command);
void handleButtonCommand(String command);
void handleAwningCommand(String command);
void handleLightCommand(String command);
void handleMainCommand(String command);

// BLE Server callbacks
class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("BLE Device connected");
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("BLE Device disconnected");
        pServer->getAdvertising()->start();
    }
};

// Command handler implementations
void handleACCommand(String command) {
    can_frame frame;
    frame.can_id = 0x123;
    frame.can_dlc = 8;
    
    if (command == "acOn") {
        frame.data[0] = 0x01;
    } else if (command == "acOff") {
        frame.data[0] = 0x00;
    }
    
    mcp2515.sendMessage(&frame);
}

void handleVentCommand(String command) {
    can_frame frame;
    frame.can_id = 0x124;
    frame.can_dlc = 8;
    
    if (command == "ventOpen") {
        frame.data[0] = 0x01;
    } else if (command == "ventClose") {
        frame.data[0] = 0x00;
    }
    
    mcp2515.sendMessage(&frame);
}

void handleButtonCommand(String command) {
    can_frame frame;
    frame.can_id = 0x125;
    frame.can_dlc = 8;
    
    if (command == "buttonPress") {
        frame.data[0] = 0x01;
    }
    
    mcp2515.sendMessage(&frame);
}

void handleAwningCommand(String command) {
    can_frame frame;
    frame.can_id = 0x126;
    frame.can_dlc = 8;
    
    if (command == "awningOut") {
        frame.data[0] = 0x01;
    } else if (command == "awningIn") {
        frame.data[0] = 0x00;
    }
    
    mcp2515.sendMessage(&frame);
}

void handleLightCommand(String command) {
    can_frame frame;
    frame.can_id = 0x127;
    frame.can_dlc = 8;
    
    if (command == "lightOn") {
        frame.data[0] = 0x01;
    } else if (command == "lightOff") {
        frame.data[0] = 0x00;
    }
    
    mcp2515.sendMessage(&frame);
}

void handleMainCommand(String command) {
    if (command.startsWith("ac")) {
        handleACCommand(command);
    } else if (command.startsWith("vent")) {
        handleVentCommand(command);
    } else if (command.startsWith("button")) {
        handleButtonCommand(command);
    } else if (command.startsWith("awning")) {
        handleAwningCommand(command);
    } else if (command.startsWith("light")) {
        handleLightCommand(command);
    }
}

// BLE Characteristic callbacks
class CommandCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            String command = String(value.c_str());
            handleMainCommand(command);
        }
    }
};

void setup() {
    // Initialize Serial
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== ModeWifi BLE Starting ===");
    
    // Initialize SPI and CAN
    SPI.begin();
    mcp2515.reset();
    mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
    mcp2515.setNormalMode();
    
    // Initialize BLE
    BLEDevice::init(DEVICE_NAME);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    
    BLEService *pService = pServer->createService(VAN_SERVICE_UUID);
    
    pCommandCharacteristic = pService->createCharacteristic(
        COMMAND_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCommandCharacteristic->setCallbacks(new CommandCallbacks());
    
    pStatusCharacteristic = pService->createCharacteristic(
        STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pStatusCharacteristic->addDescriptor(new BLE2902());
    
    pTankStatusCharacteristic = pService->createCharacteristic(
        TANK_STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pTankStatusCharacteristic->addDescriptor(new BLE2902());
    
    pService->start();
    
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(VAN_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    pAdvertising->start();
    
    Serial.println("Setup complete - BLE is active");
    Serial.println("Device should be discoverable as 'ModeWifi'");
}

void loop() {
    // Handle CAN messages
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        // Update BLE characteristics if connected
        if (deviceConnected) {
            // Example: Update status based on CAN message
            if (canMsg.can_id == 0x123) { // AC status
                String status = "AC: " + String(canMsg.data[0] ? "ON" : "OFF");
                pStatusCharacteristic->setValue(status.c_str());
                pStatusCharacteristic->notify();
            }
            // Add more status updates as needed
        }
    }
    
    // Small delay to prevent CPU hogging
    delay(10);
} 