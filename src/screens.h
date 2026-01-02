#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "nightscout_client.h"
#include "logs.h"

void drawMainScreen(TFT_eSprite& sprite, const NightscoutData& ns);
void drawClockScreen(TFT_eSprite& sprite);
void drawLogScreen(TFT_eSprite& sprite, const LogBuffer& logs);
