#include "SplitgateStats.hpp"
#include <Arduino.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include "LiquidCrystal_I2C.h"
#include "UserConfig.hpp"
#include "WiFi.h"
#include "Wire.h"
#include <HTTPClient.h>
#include "ArduinoJson.hpp"

// Static class variables
CSplitgateStats::PlayerDataStruct CSplitgateStats::PlayerData;
void* CSplitgateStats::pRxBuffer = NULL;
int CSplitgateStats::RxBufferLen = 0;
StaticJsonDocument<(16 * 1024)> CSplitgateStats::doc;
unsigned long CSplitgateStats::lastPollTime = millis() - POLLING_DELAY;
LiquidCrystal_I2C CSplitgateStats::DisplayController(0x3f, 16, 2);

// Namespaces for ArduinoJson, I don't like this but it makes the rest of the code cleaner
using namespace ArduinoJson;
using namespace DeserializationOption;

void CSplitgateStats::Initialise(){

    // Initialise I2C control on pin 14 and 15 for SCL/SDA respectively
    Wire.begin(14,15);
    DisplayController.init(); 
    DisplayController.backlight();
    DisplayController.clear();
    DisplayController.print("Splitgate Stats V1.0");

    // Create a buffer in PSRAM for the HTTP response, and zero initialise it
    pRxBuffer = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    memset(pRxBuffer, 0, BUFFER_SIZE);

    // Mark the buffer as empty
    RxBufferLen = 0;

    DisplayController.clear();
    DisplayController.print("Connecting");

    // Startup wifi connection
    WiFi.begin(USER_SSID, USER_SSID_PASSWORD);
    
    // Wait to be connected
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        DisplayController.print(".");
        //Serial.print(".");
    }

    // We are connected report our IP
    //Serial.println("WiFi connected");

}

void CSplitgateStats::MainLoop(){
    if ((millis() > lastPollTime + POLLING_DELAY)) {
        lastPollTime = millis();

        if (WiFi.status() == WL_CONNECTED) {
            
            // Get fresh data from the API
            if (RefreshData()){
                // Parse said data
                ParseJSON();
            }

        } else {
            // If for some reason we are disconnected attempt to reconnect
            WiFi.begin(USER_SSID, USER_SSID_PASSWORD);
            DisplayController.clear();
            DisplayController.print("re-connecting");
            while (WiFi.status() != WL_CONNECTED) {
                DisplayController.print(".");
                delay(500);
            }
        }
    }

    // Update the display with the latest info
    UpdateLCD();

}


bool CSplitgateStats::RefreshData(){
    bool ret = false;
    
    // Wipe out the buffer, just to make sure it is clear
    memset(pRxBuffer, 0, BUFFER_SIZE);
    RxBufferLen = 0;

    HTTPClient http;

    http.begin(TRACKER_GG_URL);
    http.addHeader("TRN-Api-Key", TRACKER_GG_API_KEY);

    //Serial.println("Polling API for stats");

    // Send the HTTP GET request to the API
    int httpCode = http.GET();

    // If the response is OK (200) parse the response
    if (httpCode == 200) {

        // We have to extend timeouts here otherwise it will timeout
        // after 1s
        http.getStreamPtr()->setTimeout(30);

        //Serial.print("Receiving Response");

        http.getStream().readBytes(reinterpret_cast<uint8_t*>(pRxBuffer), (BUFFER_SIZE)); 

        int runningOffset = 0;
        unsigned int sectionLengthInBytes = 0;
        int payloadBufferIndex = 0;

        // Use the overall buffer size as the limit to prevent an infinite while loop ( and to range check array access)
        //
        while (runningOffset < BUFFER_SIZE && payloadBufferIndex < BUFFER_SIZE) {

            // Format for the response should be:

            // Section Length (in hex) (4 chars)
            // CR (1 byte)
            // LF (1 byte)
            // Section Payload (n chars)
            // CR (1 byte)
            // LR (1 byte)
            // Repeats n times
            // Until section length = 0

            // Read in the section length & convert the hex string to unsigned in.
            std::stringstream ss;
            for (int j =0; j < 4; j++)
                ss << std::hex << reinterpret_cast<char*>(pRxBuffer)[runningOffset++];
            ss >> sectionLengthInBytes;

            // If the next section is length zero then we are done
            if (sectionLengthInBytes == 0)
                break;

            // Skip the CR and LR on end of section length
            runningOffset += 2;

            for (int j = 0; j < sectionLengthInBytes; j++) {
                reinterpret_cast<char*>(pRxBuffer)[payloadBufferIndex++] = reinterpret_cast<char*>(pRxBuffer)[runningOffset++];
            }
            
            // Skip CR and LR on end of data
            runningOffset += 2;

            // Some indication to the console that something is happening
            //Serial.print(".");            

        }  

        //Serial.println();

        RxBufferLen = payloadBufferIndex;
        ret = true;

    } else {
        //Serial.println(http.errorToString(httpCode));
    }

    // Make sure we end things here
    http.end();

    return ret;
}

void CSplitgateStats::ParseJSON(){
    //Serial.println("Parsing JSON");

    StaticJsonDocument<144> filter;

    JsonObject filter_data_0_stats =
        filter["data"][0].createNestedObject("stats");
    filter_data_0_stats["progressionLevel"]["value"] = true;
    filter_data_0_stats["progressionXp"]["value"] = true;
    filter_data_0_stats["kills"]["value"] = true;

    DeserializationError error = deserializeJson(
        doc, reinterpret_cast<char*>(pRxBuffer), RxBufferLen,
        DeserializationOption::Filter(filter));

    if (error) {
        //Serial.println("there was an error with json");
        //Serial.println(error.c_str());
    } else {

        JsonArray data = doc["data"];
        for (JsonPair data_0_stat :
                data[0]["stats"].as<JsonObject>()) {
            const char* data_0_stat_key =
                data_0_stat.key()
                    .c_str();  // "kills", "progressionLevel",
                                // "progressionXp"

            long data_0_stat_value_value =
                data_0_stat.value()["value"];  // 44261, 280, 30171

            if (data_0_stat.key() == "kills") {
                PlayerData.OverallKills = data_0_stat_value_value;
            } else if (data_0_stat.key() == "progressionLevel") {
                PlayerData.ProgressionLevel =
                    data_0_stat_value_value;
            } else if (data_0_stat.key() == "progressionXp") {
                PlayerData.ProgressionXp = data_0_stat_value_value;
            }
        }
    }
}

void CSplitgateStats::UpdateLCD(){

    static int lastUpdate = millis();
    if (millis() < lastUpdate + 2000)
        return;
    lastUpdate = millis();

// Figure out how much XP is needed to get to target
    const int levelDifference =
        targetLevel - PlayerData.ProgressionLevel;

    // Calculate XP to get to next round level
    const int targetForCurrentLevelUp =
        50000 - (cumulativeXPPerLevelApprox * (levelDifference - 1));
    const int remainingCurrentLevelXP =
        targetForCurrentLevelUp - PlayerData.ProgressionXp;

    long requiredXP = 0;
    for (int i = 1; i < levelDifference; i++) {
        requiredXP += 50000 - (cumulativeXPPerLevelApprox * (i - 1));
    }

    //Serial.print("CurrentLevel: ");
    //Serial.println(PlayerData.ProgressionLevel);

    //Serial.print("Current XP: ");
    //Serial.println(PlayerData.ProgressionXp);

    //Serial.print("Current Kills: ");
    //Serial.println(PlayerData.OverallKills);

    //Serial.print("Levels Remaining: ");
    //Serial.println(levelDifference);

    //Serial.print("XP Remaining ( this level):");
    //Serial.println(remainingCurrentLevelXP);
    //Serial.print("XP Remaining ( whole levels):");
    //Serial.println(requiredXP);
    //Serial.print("XP Remaining ( total):");
    //Serial.println((remainingCurrentLevelXP + requiredXP));

    static int screenState = 1;
    static int count = 0;

    DisplayController.clear();

    switch (screenState) {
        case 0:
            DisplayController.print("Lvl:");
            DisplayController.print(PlayerData.ProgressionLevel);
            DisplayController.print(" rem:");
            DisplayController.print(levelDifference);
            DisplayController.setCursor(0, 1);
            DisplayController.print("XPReq:");
            DisplayController.print((remainingCurrentLevelXP + requiredXP));
            break;

        case 1:
            DisplayController.clear();
            DisplayController.print("Kills:");
            DisplayController.print(PlayerData.OverallKills);
            break;

        default:
            break;
    }

    count++;
    if (count % 1 == 0) {
        screenState ++;
    }
    if (screenState > 1) {
        screenState = 0;
    }
}