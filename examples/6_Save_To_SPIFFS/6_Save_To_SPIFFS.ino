/**
 * Example 6: Store pictures to SPIFFS
 * This sketch shows how to save a picture on the SPIFFS
 * filesystem using automatic incrementing file naming.
 *
 * BE SURE TO SET "TOOLS > CORE DEBUG LEVEL = DEBUG"
 * to turn on debug messages
 */
#include <SPIFFS.h>
#include "esp32cam.h"


void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println("Init");

    camera.aithinker();
    camera.vga();
    camera.highQuality();

    while (!camera.begin())
        Serial.println(camera.getErrorMessage());

    while (!camera.storage.spiffs())
        Serial.println(camera.getErrorMessage());

    Serial.println("OK");
}


void loop() {
    if (!Serial.available())
        return;

    if (Serial.readStringUntil('\n') != "capture") {
        Serial.println("Only 'capture'");
        return;
    }

    if (!camera.capture()) {
        Serial.println(camera.getErrorMessage());
        return;
    }

    Serial.println("Capture ok, saving...");
    camera.storage.save();
}