#ifndef BLE_INTERFACE_H
#define BLE_INTERFACE_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include "CM.h"

// Command states
enum class CommandState {
    IDLE,
    IN_PROGRESS,
    COMPLETED,
    ERROR
};

// Error types
enum class CommandError {
    NONE,
    TIMEOUT,
    DISCONNECTED,
    INVALID_STATE,
    CAN_ERROR
};

// Command tracking structure
struct CommandContext {
    CommandState state;
    CommandError error;
    unsigned long startTime;
    unsigned long timeout;
    String commandType;
    bool requiresAck;
    bool ackReceived;
};

// Service and Characteristic UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define COMMAND_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define STATE_UUID          "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define PAIRING_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26aa"

// BLE device name
#define DEVICE_NAME "ModeWifi"

// State structure (packed for efficient transmission)
#pragma pack(push, 1)
struct SystemState {
    uint32_t version;          // State version for consistency checking
    uint32_t timestamp;        // Timestamp of state
    uint16_t flags;           // System flags
    
    // Device states (1 byte each)
    uint8_t ac_mode;          // AC operating mode
    uint8_t ac_fan_mode;      // AC fan mode
    uint8_t ac_fan_speed;     // AC fan speed
    uint8_t heater_mode;      // Heater mode
    uint8_t heater_fan_speed; // Heater fan speed
    uint8_t vent_position;    // Vent position
    uint8_t vent_speed;       // Vent speed
    uint8_t vent_direction;   // Vent direction
    
    // Sensor readings (2 bytes each)
    int16_t ambient_temp;     // Ambient temperature (scaled by 100)
    int16_t glycol_inlet;     // Glycol inlet temp (scaled by 100)
    int16_t glycol_outlet;    // Glycol outlet temp (scaled by 100)
    uint16_t fresh_water;     // Fresh water level
    uint16_t gray_water;      // Gray water level
    
    // Control states (bit-packed)
    uint8_t control_states;   // Bit-packed control states
};
#pragma pack(pop)

// Control state bit definitions
#define CONTROL_CABIN_LIGHTS  0x01
#define CONTROL_READING_LIGHTS 0x02
#define CONTROL_CARGO_LIGHTS  0x04
#define CONTROL_AWNING_LIGHTS 0x08
#define CONTROL_WATER_PUMP    0x10
#define CONTROL_HOT_WATER     0x20
#define CONTROL_AWNING_OUT    0x40
#define CONTROL_AWNING_IN     0x80

class BLEInterface {
public:
    BLEInterface(CM* cm);
    void begin();
    void update();
    void handleCommand(const std::string& command);
    void sendStatus();
    void sendInitialState();

private:
    BLEServer* pServer;
    BLEService* pService;
    BLECharacteristic* pCommandCharacteristic;
    BLECharacteristic* pStateCharacteristic;
    BLECharacteristic* pPairingCharacteristic;
    bool deviceConnected;
    CM* cm;
    
    // Security
    bool isPaired;
    std::string pairingCode;
    
    // Command handling
    CommandContext currentCommand;
    static const unsigned long COMMAND_TIMEOUT = 5000; // 5 seconds
    static const unsigned long CAN_RETRY_TIMEOUT = 1000; // 1 second
    static const int MAX_RETRIES = 3;
    
    // State management
    SystemState currentState;
    SystemState lastSentState;
    uint32_t lastStateUpdate;
    static const uint32_t STATE_UPDATE_INTERVAL = 1000; // 1 second
    
    // Error handling
    void handleCommandError(CommandError error);
    void resetCommandState();
    bool validateCommandState();
    void sendErrorResponse(CommandError error);
    
    // Command execution
    bool executeCommand(const JsonDocument& doc);
    bool waitForAck();
    void retryCommand();
    
    // State protection
    bool isCommandSafe(const String& command);
    bool canExecuteCommand();
    void emergencyStop();
    
    // State handling
    void updateState();
    void sendState();
    bool hasStateChanged();
    void packState();
    void unpackState(const uint8_t* data, size_t length);
    
    // Command handling
    void processCommand(const JsonDocument& doc);
    void handleAwningCommand(const JsonDocument& doc);
    void handleVentCommand(const JsonDocument& doc);
    void handleACCommand(const JsonDocument& doc);
    void handleHeaterCommand(const JsonDocument& doc);
    void handleLightsCommand(const JsonDocument& doc);
    void handleWaterCommand(const JsonDocument& doc);
    
    // Status updates
    void updateStatus();
    
    // State synchronization
    void sendSystemState();
    void sendDeviceState();
    void sendSensorState();
    void sendControlState();
    
    // Status characteristics
    BLECharacteristic* pSystemStateCharacteristic;
    BLECharacteristic* pDeviceStateCharacteristic;
    BLECharacteristic* pSensorStateCharacteristic;
    BLECharacteristic* pControlStateCharacteristic;
};

// Callback class for BLE server
class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;
};

// Callback class for command characteristic
class CommandCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override;
};

#endif // BLE_INTERFACE_H 