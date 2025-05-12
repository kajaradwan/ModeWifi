#include "ble_interface.h"

// Global variables for callbacks
BLEInterface* g_bleInterface = nullptr;
bool g_deviceConnected = false;

// Server callbacks implementation
void ServerCallbacks::onConnect(BLEServer* pServer) {
    g_deviceConnected = true;
    Serial.println("Device connected");
    if (g_bleInterface) {
        g_bleInterface->sendStateUpdate();
    }
}

void ServerCallbacks::onDisconnect(BLEServer* pServer) {
    g_deviceConnected = false;
    Serial.println("Device disconnected");
    // Restart advertising
    pServer->getAdvertising()->start();
}

// Command callbacks implementation
void CommandCallbacks::onWrite(BLECharacteristic *pCharacteristic) {
    if (g_bleInterface) {
        std::string value = pCharacteristic->getValue();
        g_bleInterface->handleCommand(value);
    }
}

// BLEInterface implementation
BLEInterface::BLEInterface(CM* cm) : cm(cm), deviceConnected(false), isPaired(false) {
    pairingCode = "112018";
    g_bleInterface = this;
}

void BLEInterface::begin() {
    Serial.println("BLE: Initializing BLE device...");
    // Initialize BLE
    BLEDevice::init(DEVICE_NAME);
    Serial.println("BLE: Device initialized");
    
    // Create the BLE Server
    Serial.println("BLE: Creating server...");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    Serial.println("BLE: Server created");
    
    // Create the BLE Service
    Serial.println("BLE: Creating service...");
    pService = pServer->createService(SERVICE_UUID);
    Serial.println("BLE: Service created");
    
    // Create BLE Characteristics
    Serial.println("BLE: Creating characteristics...");
    pCommandCharacteristic = pService->createCharacteristic(
        COMMAND_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCommandCharacteristic->setCallbacks(new CommandCallbacks());
    
    pStateCharacteristic = pService->createCharacteristic(
        STATE_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pStateCharacteristic->addDescriptor(new BLE2902());
    
    pPairingCharacteristic = pService->createCharacteristic(
        PAIRING_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ
    );
    Serial.println("BLE: Characteristics created");

    // Create additional state characteristics
    pSystemStateCharacteristic = pService->createCharacteristic(
        "beb5483e-36e1-4688-b7f5-ea07361b26ab",
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pSystemStateCharacteristic->addDescriptor(new BLE2902());

    pDeviceStateCharacteristic = pService->createCharacteristic(
        "beb5483e-36e1-4688-b7f5-ea07361b26ac",
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pDeviceStateCharacteristic->addDescriptor(new BLE2902());

    pSensorStateCharacteristic = pService->createCharacteristic(
        "beb5483e-36e1-4688-b7f5-ea07361b26ad",
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pSensorStateCharacteristic->addDescriptor(new BLE2902());

    pControlStateCharacteristic = pService->createCharacteristic(
        "beb5483e-36e1-4688-b7f5-ea07361b26ae",
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pControlStateCharacteristic->addDescriptor(new BLE2902());
    
    // Start the service
    Serial.println("BLE: Starting service...");
    pService->start();
    Serial.println("BLE: Service started");
    
    // Start advertising
    Serial.println("BLE: Starting advertising...");
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    pAdvertising->start();
    Serial.println("BLE: Advertising started");
    
    Serial.println("BLE: Device ready and advertising");
}

void BLEInterface::update() {
    if (deviceConnected) {
        uint32_t now = millis();
        if (now - lastStateUpdate >= STATE_UPDATE_INTERVAL) {
            sendStateUpdate();
            lastStateUpdate = now;
        }
    }
}

void BLEInterface::sendStateUpdate() {
    StaticJsonDocument<1024> doc;
    
    // System info with versioning
    doc["timestamp"] = millis();
    doc["version"] = "1.0.0";  // Match api_spec.json version
    
    // Device states
    JsonObject ac = doc.createNestedObject("ac");
    ac["mode"] = cm->ac.operatingMode;
    ac["fan_mode"] = cm->ac.fanMode;
    ac["fan_speed"] = cm->ac.fanSpeed;
    ac["setpoint"] = cm->ac.fSetpointCool;
    
    JsonObject heater = doc.createNestedObject("heater");
    heater["mode"] = cm->heater.bAutoMode ? "auto" : "manual";
    heater["fan_speed"] = cm->heater.nFanSpeed;
    heater["setpoint"] = cm->heater.fTargetTemp;
    heater["hot_water"] = cm->heater.bHotWater;
    heater["glycol_inlet"] = cm->heater.fGlycolInletTemp;
    heater["glycol_outlet"] = cm->heater.fGlycolOutletTemp;
    
    JsonObject vent = doc.createNestedObject("vent");
    vent["position"] = cm->roofFan.nDomePosition;
    vent["speed"] = cm->roofFan.nSpeed;
    vent["direction"] = cm->roofFan.nWindDirection;
    
    // Sensor readings
    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["ambient_temp"] = cm->fAmbientTemp;
    sensors["fresh_water"] = cm->nFreshTankLevel;
    sensors["gray_water"] = cm->nGrayTankLevel;
    
    // Control states
    JsonObject controls = doc.createNestedObject("controls");
    JsonObject lights = controls.createNestedObject("lights");
    lights["cabin"] = cm->pdm1_output[4].bCommand > 0;
    lights["reading"] = cm->pdm1_output[3].bCommand > 0;
    lights["cargo"] = cm->pdm1_output[2].bCommand > 0;
    lights["awning"] = cm->pdm1_output[5].bCommand > 0;
    
    JsonObject water = controls.createNestedObject("water");
    water["pump"] = cm->pdm1_output[12].bCommand > 0;
    water["hot_water"] = cm->heater.bHotWater;
    
    JsonObject awning = controls.createNestedObject("awning");
    awning["extended"] = cm->pdm1_output[7].bCommand > 0;
    
    String state;
    serializeJson(doc, state);
    pStateCharacteristic->setValue(state.c_str());
    pStateCharacteristic->notify();
}

void BLEInterface::handleCommand(const std::string& command) {
    if (!isPaired) {
        if (command == pairingCode) {
            isPaired = true;
            sendCommandResponse("success", "paired");
            return;
        }
        sendCommandResponse("error", "not_paired");
        return;
    }
    
    // Parse the JSON command
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, command);
    
    if (error) {
        sendCommandResponse("error", "invalid_json");
        return;
    }
    
    // Process the command
    processCommand(doc);
}

void BLEInterface::sendCommandResponse(const char* status, const char* message) {
    StaticJsonDocument<128> response;
    response["status"] = status;
    if (message) {
        response["message"] = message;
    }
    
    String responseStr;
    serializeJson(response, responseStr);
    pCommandCharacteristic->setValue(responseStr.c_str());
    pCommandCharacteristic->notify();
}

void BLEInterface::processCommand(const JsonDocument& doc) {
    if (!doc.containsKey("command")) {
        sendCommandResponse("error", "missing_command");
        return;
    }
    
    String cmd = doc["command"].as<String>();
    
    if (cmd == "awning") handleAwningCommand(doc);
    else if (cmd == "vent") handleVentCommand(doc);
    else if (cmd == "ac") handleACCommand(doc);
    else if (cmd == "heater") handleHeaterCommand(doc);
    else if (cmd == "lights") handleLightsCommand(doc);
    else if (cmd == "water") handleWaterCommand(doc);
    else {
        sendCommandResponse("error", "unknown_command");
    }
}

void BLEInterface::handleAwningCommand(const JsonDocument& doc) {
    if (!doc.containsKey("action")) {
        sendCommandResponse("error", "missing_action");
        return;
    }
    
    String action = doc["action"].as<String>();
    
    if (action == "out") {
        cm->pressdigitalbutton(cm->lastPDM1inputs7to12, 7, 2);
        sendCommandResponse("success", "awning_out");
    }
    else if (action == "in") {
        cm->pressdigitalbutton(cm->lastPDM1inputs7to12, 7, 3);
        sendCommandResponse("success", "awning_in");
    }
    else if (action == "lights") {
        cm->pressdigitalbutton(cm->lastPDM1inputs1to6, 5, 0);
        sendCommandResponse("success", "awning_lights_toggled");
    }
    else {
        sendCommandResponse("error", "invalid_action");
    }
}

void BLEInterface::handleVentCommand(const JsonDocument& doc) {
    if (!doc.containsKey("action")) {
        sendCommandResponse("error", "missing_action");
        return;
    }
    
    String action = doc["action"].as<String>();
    
    if (action == "open") {
        cm->openVent();
        sendCommandResponse("success", "vent_opened");
    }
    else if (action == "close") {
        cm->closeVent();
        sendCommandResponse("success", "vent_closed");
    }
    else if (action == "speed" && doc.containsKey("speed")) {
        int speed = doc["speed"].as<int>();
        cm->setVentSpeed(speed);
        sendCommandResponse("success", "vent_speed_set");
    }
    else if (action == "direction" && doc.containsKey("direction")) {
        bool direction = doc["direction"].as<bool>();
        cm->setVentDirection(direction);
        sendCommandResponse("success", "vent_direction_set");
    }
    else {
        sendCommandResponse("error", "invalid_action");
    }
}

void BLEInterface::handleACCommand(const JsonDocument& doc) {
    if (!doc.containsKey("action")) {
        sendCommandResponse("error", "missing_action");
        return;
    }
    
    String action = doc["action"].as<String>();
    
    if (action == "on") {
        cm->acCommand(1, 1, 64);
        sendCommandResponse("success", "ac_on");
    }
    else if (action == "off") {
        cm->acCommand(0, 0, 0);
        sendCommandResponse("success", "ac_off");
    }
    else if (action == "mode" && doc.containsKey("mode")) {
        int mode = doc["mode"].as<int>();
        cm->setACOperatingMode(mode);
        sendCommandResponse("success", "ac_mode_set");
    }
    else if (action == "fan" && doc.containsKey("mode")) {
        int mode = doc["mode"].as<int>();
        cm->setACFanMode(mode);
        sendCommandResponse("success", "ac_fan_mode_set");
    }
    else if (action == "speed" && doc.containsKey("speed")) {
        int speed = doc["speed"].as<int>();
        cm->setACFanSpeed(speed);
        sendCommandResponse("success", "ac_fan_speed_set");
    }
    else if (action == "temp" && doc.containsKey("temp")) {
        float temp = doc["temp"].as<float>();
        cm->acSetTemp(temp);
        sendCommandResponse("success", "ac_temp_set");
    }
    else {
        sendCommandResponse("error", "invalid_action");
    }
}

void BLEInterface::handleHeaterCommand(const JsonDocument& doc) {
    if (!doc.containsKey("action")) {
        sendCommandResponse("error", "missing_action");
        return;
    }
    
    String action = doc["action"].as<String>();
    
    if (action == "on") {
        // Implement heater on command
        sendCommandResponse("success", "heater_on");
    }
    else if (action == "off") {
        // Implement heater off command
        sendCommandResponse("success", "heater_off");
    }
    else if (action == "temp" && doc.containsKey("temp")) {
        float temp = doc["temp"].as<float>();
        // Implement set temperature command
        sendCommandResponse("success", "heater_temp_set");
    }
    else if (action == "fan" && doc.containsKey("speed")) {
        int speed = doc["speed"].as<int>();
        // Implement set fan speed command
        sendCommandResponse("success", "heater_fan_speed_set");
    }
    else if (action == "auto" && doc.containsKey("enabled")) {
        bool enabled = doc["enabled"].as<bool>();
        // Implement auto mode command
        sendCommandResponse("success", "heater_auto_mode_set");
    }
    else {
        sendCommandResponse("error", "invalid_action");
    }
}

void BLEInterface::handleLightsCommand(const JsonDocument& doc) {
    if (!doc.containsKey("action")) {
        sendCommandResponse("error", "missing_action");
        return;
    }
    
    String action = doc["action"].as<String>();
    
    if (action == "cabin") {
        cm->pressdigitalbutton(cm->lastPDM1inputs1to6, 4, 0);
        sendCommandResponse("success", "cabin_lights_toggled");
    }
    else if (action == "reading") {
        cm->pressdigitalbutton(cm->lastPDM1inputs1to6, 3, 0);
        sendCommandResponse("success", "reading_lights_toggled");
    }
    else if (action == "cargo") {
        cm->pressdigitalbutton(cm->lastPDM1inputs1to6, 2, 0);
        sendCommandResponse("success", "cargo_lights_toggled");
    }
    else {
        sendCommandResponse("error", "invalid_action");
    }
}

void BLEInterface::handleWaterCommand(const JsonDocument& doc) {
    if (!doc.containsKey("action")) {
        sendCommandResponse("error", "missing_action");
        return;
    }
    
    String action = doc["action"].as<String>();
    
    if (action == "pump") {
        if (!doc.containsKey("action")) {
            sendCommandResponse("error", "missing_pump_action");
            return;
        }
        String pumpAction = doc["action"].as<String>();
        
        if (pumpAction == "on") {
            cm->pressdigitalbutton(cm->lastPDM2inputs1to6, 7, 3);
            sendCommandResponse("success", "pump_on");
        }
        else if (pumpAction == "off") {
            cm->pressdigitalbutton(cm->lastPDM2inputs1to6, 7, 3);
            sendCommandResponse("success", "pump_off");
        }
        else if (pumpAction == "minute" && doc.containsKey("minutes")) {
            int minutes = doc["minutes"].as<int>();
            cm->handleMinutePump(minutes);
            sendCommandResponse("success", "pump_minute_set");
        }
        else {
            sendCommandResponse("error", "invalid_pump_action");
        }
    }
    else if (action == "hot" && doc.containsKey("enabled")) {
        bool enabled = doc["enabled"].as<bool>();
        // Implement hot water control
        sendCommandResponse("success", "hot_water_control_set");
    }
    else {
        sendCommandResponse("error", "invalid_action");
    }
} 