#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <SPI.h>
#include <mcp2515.h>

// Device name
#define DEVICE_NAME "ModeWifi"

// Service and Characteristic UUIDs
#define VAN_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define COMMAND_CHAR_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define STATUS_CHAR_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define TANK_STATUS_CHAR_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26aa"

// CAN bus setup
MCP2515 mcp2515(5); // CS pin 5

BLEServer* pServer = nullptr;
BLECharacteristic* pCommandChar = nullptr;
BLECharacteristic* pStatusChar = nullptr;
BLECharacteristic* pTankStatusChar = nullptr;
bool deviceConnected = false;

// Forward declarations of command handlers
void handleACCommand(String command);
void handleVentCommand(String command);
void handleButtonCommand(String command);
void handleAwningCommand(String command);
void handleLightCommand(String command);

class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Device connected");
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected");
        pServer->getAdvertising()->start();
    }
};

class CommandCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            String command = String(value.c_str());
            Serial.print("Received command: ");
            Serial.println(command);
            
            // Parse and execute command
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
    }
};

// Command handler implementations
void handleACCommand(String command) {
    can_frame frame;
    frame.can_id = 0x123;
    frame.can_dlc = 3;
    
    if (command == "acOn") {
        frame.data[0] = 0x01;
        frame.data[1] = 0x01;
        frame.data[2] = 0x40;
    } else if (command == "acOff") {
        frame.data[0] = 0x01;
        frame.data[1] = 0x00;
        frame.data[2] = 0x00;
    }
    mcp2515.sendMessage(&frame);
}

void handleVentCommand(String command) {
    can_frame frame;
    frame.can_id = 0x124;
    frame.can_dlc = 3;
    
    if (command == "openVent") {
        frame.data[0] = 0x02;
        frame.data[1] = 0x01;
        frame.data[2] = 0x00;
    } else if (command == "closeVent") {
        frame.data[0] = 0x02;
        frame.data[1] = 0x00;
        frame.data[2] = 0x00;
    }
    mcp2515.sendMessage(&frame);
}

void handleButtonCommand(String command) {
    can_frame frame;
    frame.can_id = 0x125;
    frame.can_dlc = 3;
    
    if (command == "pressCargo") {
        frame.data[0] = 0x03;
        frame.data[1] = 0x01;
        frame.data[2] = 0x00;
    } else if (command == "pressCabin") {
        frame.data[0] = 0x03;
        frame.data[1] = 0x02;
        frame.data[2] = 0x00;
    }
    mcp2515.sendMessage(&frame);
}

void handleAwningCommand(String command) {
    can_frame frame;
    frame.can_id = 0x126;
    frame.can_dlc = 3;
    
    if (command == "awningOut") {
        frame.data[0] = 0x04;
        frame.data[1] = 0x01;
        frame.data[2] = 0x00;
    } else if (command == "awningIn") {
        frame.data[0] = 0x04;
        frame.data[1] = 0x00;
        frame.data[2] = 0x00;
    }
    mcp2515.sendMessage(&frame);
}

void handleLightCommand(String command) {
    can_frame frame;
    frame.can_id = 0x127;
    frame.can_dlc = 3;
    
    if (command == "lightsOn") {
        frame.data[0] = 0x05;
        frame.data[1] = 0x01;
        frame.data[2] = 0x00;
    } else if (command == "lightsOff") {
        frame.data[0] = 0x05;
        frame.data[1] = 0x00;
        frame.data[2] = 0x00;
    }
    mcp2515.sendMessage(&frame);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n\n\n");
    Serial.println("==========================================");
    Serial.println("=== ModeWifi Van Control Starting ===");
    Serial.println("==========================================");

    // Initialize CAN bus
    Serial.println("Initializing CAN bus...");
    SPI.begin();
    mcp2515.reset();
    mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
    mcp2515.setNormalMode();
    Serial.println("CAN bus initialized");

    // Initialize BLE
    Serial.println("Initializing BLE...");
    BLEDevice::init(DEVICE_NAME);
    Serial.println("BLE initialized");

    // Create the BLE Server
    Serial.println("Creating server...");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    Serial.println("Server created");

    // Create the BLE Service
    BLEService *pService = pServer->createService(VAN_SERVICE_UUID);
    Serial.println("Service created");

    // Create BLE Characteristics
    pCommandChar = pService->createCharacteristic(
        COMMAND_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCommandChar->setCallbacks(new CommandCallbacks());
    Serial.println("Command characteristic created");

    pStatusChar = pService->createCharacteristic(
        STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pStatusChar->addDescriptor(new BLE2902());
    Serial.println("Status characteristic created");

    pTankStatusChar = pService->createCharacteristic(
        TANK_STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pTankStatusChar->addDescriptor(new BLE2902());
    Serial.println("Tank status characteristic created");

    // Start the service
    pService->start();
    Serial.println("Service started");

    // Start advertising
    Serial.println("Starting advertising...");
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(VAN_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinInterval(0x20);  // 32ms
    pAdvertising->setMaxInterval(0x40);  // 64ms
    pAdvertising->start();
    Serial.println("Advertising started");
    
    Serial.println("==========================================");
    Serial.println("=== ModeWifi Ready - Device should be discoverable ===");
    Serial.println("==========================================");
}

void loop() {
    static unsigned long lastStatusUpdate = 0;
    unsigned long now = millis();
    
    // Update status every second
    if (now - lastStatusUpdate > 1000) {
        if (deviceConnected) {
            // Read CAN bus for status updates
            can_frame frame;
            if (mcp2515.readMessage(&frame) == MCP2515::ERROR_OK) {
                // Update appropriate characteristic based on message ID
                switch (frame.can_id) {
                    case 0x200: // Status updates
                        pStatusChar->setValue(frame.data, frame.can_dlc);
                        pStatusChar->notify();
                        break;
                    case 0x201: // Tank status
                        pTankStatusChar->setValue(frame.data, frame.can_dlc);
                        pTankStatusChar->notify();
                        break;
                }
            }
        }
        lastStatusUpdate = now;
    }
    
    delay(10);
} 