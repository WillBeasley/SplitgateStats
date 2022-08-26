#include <Arduino.h>
#include "SplitgateStats.hpp"


void setup() {
    // Single run code
    Serial.begin(115200);

    if (!psramInit()) {
        Serial.println("PSRAM error");
        while(true);
    }
    
    // Initialise the main code
    CSplitgateStats::Initialise();

}

void loop() {
    // Every loop just call into the main code
    CSplitgateStats::MainLoop();
}

