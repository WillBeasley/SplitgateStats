#pragma once

#include "UserConfig.hpp"
#define ARDUINOJSON_USE_LONG_LONG 0
#define ARDUINOJSON_USE_DOUBLE 0
#include "ArduinoJson.hpp"
#include "LiquidCrystal_I2C.h"

// Handle getting splitgate player data from tracker.gg and display it on the screen.

#ifndef PLAYER_ID
#error "You must specify a PLAYER_ID so we know who to get data for"
#endif

#ifndef PLATFORM
#error "You must specify a PLATFORM so we know what kind of account you have e.g \"steam\""
#endif

#ifndef TRACKER_GG_API_KEY
#error "You must specify a TRACKER_GG_API_KEY for this to work!"
#endif

// User must have defined SSID and SSID_PASSWORD in UserConfig.h
#ifndef USER_SSID
#error "You must define USER_SSID in UserConfig.h!"
#endif

#ifndef USER_SSID_PASSWORD
#error "You must define USER_SSID_PASSWORD in UserConfig.h!"
#endif

#define TRACKER_GG_URL "https://public-api.tracker.gg/v2/splitgate/standard/profile/" PLATFORM "/" PLAYER_ID "/segments/overview?season=0"


#define BUFFER_SIZE (356 * 1024)
#define JSON_DOC_SIZE (16 * 1024)

#ifndef POLLING_DELAY
    #define POLLING_DELAY 30000
#endif

using namespace ArduinoJson;
using namespace DeserializationOption;

class CSplitgateStats{
    
public:
static void Initialise();

static void MainLoop();

struct PlayerDataStruct {
    int OverallKills;
    int ProgressionXp;
    int ProgressionLevel;
};

protected:
static bool RefreshData();

static void ParseJSON();

static void UpdateLCD();

// What level we want to hit
static const int targetLevel = 301;
// How much extra xp it is per level
static const int cumulativeXPPerLevelApprox = 500;


static unsigned long lastPollTime;

// RAW Buffer for HTTP response
static void* pRxBuffer;
// Length of the last response
static int RxBufferLen;

// Statically defined document for the JSON document
static StaticJsonDocument<JSON_DOC_SIZE> doc;

static LiquidCrystal_I2C DisplayController;

static PlayerDataStruct PlayerData;

};