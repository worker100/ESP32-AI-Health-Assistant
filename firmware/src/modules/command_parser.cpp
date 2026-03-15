#include "command_parser.h"

namespace command_parser {

ParsedCommand parseSerialCommand(const String& raw) {
  ParsedCommand out;
  out.normalized = raw;
  out.normalized.trim();
  out.normalized.toUpperCase();

  const String& cmd = out.normalized;
  if (cmd == "MODE TEST" || cmd == "FALL TEST") {
    out.id = CommandId::ModeTest;
  } else if (cmd == "MODE NORMAL" || cmd == "FALL NORMAL") {
    out.id = CommandId::ModeNormal;
  } else if (cmd == "MODE?" || cmd == "FALL?") {
    out.id = CommandId::ModeQuery;
  } else if (cmd == "VOL LOW") {
    out.id = CommandId::VolLow;
  } else if (cmd == "VOL MED" || cmd == "VOL MEDIUM") {
    out.id = CommandId::VolMedium;
  } else if (cmd == "VOL HIGH") {
    out.id = CommandId::VolHigh;
  } else if (cmd == "VOL?" || cmd == "VOLUME?") {
    out.id = CommandId::VolQuery;
  } else if (cmd == "LOG DEMO") {
    out.id = CommandId::LogDemo;
  } else if (cmd == "LOG FULL") {
    out.id = CommandId::LogFull;
  } else if (cmd == "LOG?") {
    out.id = CommandId::LogQuery;
  } else if (cmd == "HELP") {
    out.id = CommandId::Help;
  } else if (cmd == "HEALTH?") {
    out.id = CommandId::HealthQuery;
  } else if (cmd == "BACKEND?") {
    out.id = CommandId::BackendQuery;
  } else if (cmd == "BACKEND ON") {
    out.id = CommandId::BackendOn;
  } else if (cmd == "BACKEND OFF") {
    out.id = CommandId::BackendOff;
  } else {
    out.id = CommandId::Unknown;
  }
  return out;
}

}  // namespace command_parser

