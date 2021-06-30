#pragma once
#include <stdint.h>

uint16_t GetHajackFirstTargetControlPort();

uint16_t GetHajackLastTargetControlPort();

uint16_t GetHajackRemoteServerPort();

uint16_t GetHajackForwardPortBase();

uint16_t GetHajackForwardTargetControlOffset();

uint16_t GetHajackForwardRemoteServerOffset();

uint16_t GetHajackForwardPortStride();
