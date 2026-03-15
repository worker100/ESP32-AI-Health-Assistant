#pragma once

#include <Arduino.h>

namespace command_parser {

enum class CommandId : uint8_t {
  Unknown = 0,
  ModeTest,
  ModeNormal,
  ModeQuery,
  VolLow,
  VolMedium,
  VolHigh,
  VolQuery,
  LogDemo,
  LogFull,
  LogQuery,
  Help,
  HealthQuery,
  BackendQuery,
  BackendOn,
  BackendOff,
};

struct ParsedCommand {
  CommandId id = CommandId::Unknown;
  String normalized;
};

ParsedCommand parseSerialCommand(const String& raw);

}  // namespace command_parser

