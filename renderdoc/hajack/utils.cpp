#include "utils.h"

#include "hajack.h"

uint16_t GetHajackFirstTargetControlPort() {
  if (!Hajack::GetInst().IsSelfCompiledApk()) {
    return 38920;
  }
  return Hajack::GetInst().FirstTargetControlPort;
}

uint16_t GetHajackLastTargetControlPort() {
  if (!Hajack::GetInst().IsSelfCompiledApk()) {
    return 38920 + 7;
  }
  return Hajack::GetInst().LastTargetControlPort;
}

uint16_t GetHajackRemoteServerPort() {
  if (!Hajack::GetInst().IsSelfCompiledApk()) {
    return 39920;
  }
  return Hajack::GetInst().RemoteServerPort;
}

uint16_t GetHajackForwardPortBase() {
  if (!Hajack::GetInst().IsSelfCompiledApk()) {
    return 38950;
  }
  return Hajack::GetInst().ForwardPortBase;
}

uint16_t GetHajackForwardTargetControlOffset() {
  if (!Hajack::GetInst().IsSelfCompiledApk()) {
    return 0;
  }
  return Hajack::GetInst().ForwardTargetControlOffset;
}

uint16_t GetHajackForwardRemoteServerOffset() {
  if (!Hajack::GetInst().IsSelfCompiledApk()) {
    return 9;
  }
  return Hajack::GetInst().ForwardRemoteServerOffset;
}

uint16_t GetHajackForwardPortStride() {
  if (!Hajack::GetInst().IsSelfCompiledApk()) {
    return 10;
  }
  return Hajack::GetInst().ForwardPortStride;
}
