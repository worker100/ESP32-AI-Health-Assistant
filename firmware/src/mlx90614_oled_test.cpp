#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MLX90614.h>
#include <math.h>

namespace {
constexpr uint8_t kI2cSda = 8;
constexpr uint8_t kI2cScl = 9;
constexpr uint8_t kOledAddr = 0x3C;
constexpr int kScreenWidth = 128;
constexpr int kScreenHeight = 64;
constexpr uint32_t kRefreshMs = 1200;
constexpr uint32_t kBootDelayMs = 300;
constexpr uint32_t kScanSpeeds[] = {50000, 100000, 20000};

Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, -1);
Adafruit_MLX90614 mlx;

bool g_oledReady = false;
bool g_mlxReady = false;
uint8_t g_mlxAddr = 0;
uint32_t g_lastRefreshMs = 0;
char g_diagStatus[24] = "Booting";
char g_diagHint[24] = "";

const char* i2cErrorText(uint8_t err) {
  switch (err) {
    case 0:
      return "OK";
    case 1:
      return "DATA_TOO_LONG";
    case 2:
      return "ADDR_NACK";
    case 3:
      return "DATA_NACK";
    case 4:
      return "OTHER";
    case 5:
      return "TIMEOUT";
    default:
      return "UNKNOWN";
  }
}

bool probeAddress(uint8_t addr, uint8_t* errOut = nullptr) {
  Wire.beginTransmission(addr);
  const uint8_t err = Wire.endTransmission();
  if (errOut != nullptr) {
    *errOut = err;
  }
  return err == 0;
}

bool readWordRaw(uint8_t addr, uint8_t reg, uint16_t* valueOut) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const uint8_t received = Wire.requestFrom(static_cast<int>(addr), 3, static_cast<int>(true));
  if (received != 3) {
    return false;
  }

  const uint8_t low = Wire.read();
  const uint8_t high = Wire.read();
  (void)Wire.read();

  if (valueOut != nullptr) {
    *valueOut = static_cast<uint16_t>(low) | (static_cast<uint16_t>(high) << 8);
  }
  return true;
}

float rawToCelsius(uint16_t raw) {
  return static_cast<float>(raw) * 0.02f - 273.15f;
}

bool looksLikeValidTempRaw(uint16_t raw) {
  const float c = rawToCelsius(raw);
  return isfinite(c) && c > -70.0f && c < 380.0f;
}

bool readMlxIdentity(uint8_t addr, uint16_t* id1, uint16_t* id2, uint16_t* id3, uint16_t* id4,
                     uint16_t* configReg, uint16_t* addrReg) {
  return readWordRaw(addr, 0x3C, id1) && readWordRaw(addr, 0x3D, id2) &&
         readWordRaw(addr, 0x3E, id3) && readWordRaw(addr, 0x3F, id4) &&
         readWordRaw(addr, 0x25, configReg) && readWordRaw(addr, 0x2E, addrReg);
}

void printScan() {
  Serial.println("I2C scan start...");
  for (uint8_t addr = 1; addr < 127; ++addr) {
    uint8_t err = 0;
    if (probeAddress(addr, &err)) {
      Serial.print("  found 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.println(addr, HEX);
    }
  }
  Serial.println("I2C scan done.");
}

void printScanAtSpeed(uint32_t speed) {
  Wire.setClock(speed);
  Serial.print("I2C scan @ ");
  Serial.print(speed);
  Serial.println(" Hz");
  printScan();
}

void drawText(const char* line1, const char* line2, const char* line3, const char* line4,
              const char* line5) {
  if (!g_oledReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(line1);
  display.println(line2);
  display.println(line3);
  display.println(line4);
  display.println(line5);
  display.display();
}

void initDisplay() {
  g_oledReady = display.begin(SSD1306_SWITCHCAPVCC, kOledAddr);
  if (g_oledReady) {
    drawText("MLX90614 TEST", "OLED OK", "SDA=GPIO8", "SCL=GPIO9", "Booting...");
  } else {
    Serial.println("OLED init failed at 0x3C");
  }
}

void setDiag(const char* status, const char* hint) {
  snprintf(g_diagStatus, sizeof(g_diagStatus), "%s", status);
  snprintf(g_diagHint, sizeof(g_diagHint), "%s", hint);
}

void printWiringReminder() {
  Serial.println("Check wiring on module silk:");
  Serial.println("  VIN -> 3.3V first, then try 5V if no ACK");
  Serial.println("  GND -> GND");
  Serial.println("  SCL -> GPIO9");
  Serial.println("  SDA -> GPIO8");
}

void initMlx() {
  static const uint8_t kCandidates[] = {0x5A, 0x5B, 0x5C, 0x5D};
  g_mlxReady = false;
  g_mlxAddr = 0;
  setDiag("Probe start", "");

  Serial.println("MLX probe start...");
  bool sawAnyAck = false;
  bool sawReadableRaw = false;
  bool sawIdentity = false;

  for (uint32_t speed : kScanSpeeds) {
    Wire.setClock(speed);
    Serial.print("Trying MLX at ");
    Serial.print(speed);
    Serial.println(" Hz");

    for (uint8_t addr : kCandidates) {
      uint8_t err = 0;
      const bool ack = probeAddress(addr, &err);
      Serial.print("  probe 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.print(addr, HEX);
      Serial.print(" -> ");
      Serial.println(i2cErrorText(err));

      if (!ack) {
        continue;
      }
      sawAnyAck = true;

      uint16_t rawAmb = 0;
      uint16_t rawObj = 0;
      const bool rawOk = readWordRaw(addr, 0x06, &rawAmb) && readWordRaw(addr, 0x07, &rawObj);
      if (rawOk) {
        Serial.print("    raw amb=");
        Serial.print(rawToCelsius(rawAmb), 2);
        Serial.print("C, obj=");
        Serial.print(rawToCelsius(rawObj), 2);
        Serial.println("C");
        if (looksLikeValidTempRaw(rawAmb) && looksLikeValidTempRaw(rawObj)) {
          sawReadableRaw = true;
        }
      } else {
        Serial.println("    raw read failed");
      }

      uint16_t id1 = 0;
      uint16_t id2 = 0;
      uint16_t id3 = 0;
      uint16_t id4 = 0;
      uint16_t cfg = 0;
      uint16_t eeAddr = 0;
      const bool idOk = readMlxIdentity(addr, &id1, &id2, &id3, &id4, &cfg, &eeAddr);
      if (idOk) {
        sawIdentity = true;
        Serial.print("    ID=");
        Serial.print(id1, HEX);
        Serial.print("-");
        Serial.print(id2, HEX);
        Serial.print("-");
        Serial.print(id3, HEX);
        Serial.print("-");
        Serial.print(id4, HEX);
        Serial.print(" cfg=0x");
        Serial.print(cfg, HEX);
        Serial.print(" eeAddr=0x");
        Serial.println(eeAddr, HEX);
      } else {
        Serial.println("    ID/config read failed");
      }

      if (mlx.begin(addr, &Wire)) {
        g_mlxReady = true;
        g_mlxAddr = addr;
        Serial.print("MLX library init OK at 0x");
        if (addr < 16) {
          Serial.print('0');
        }
        Serial.print(addr, HEX);
        Serial.print(" @ ");
        Serial.print(speed);
        Serial.println(" Hz");
        setDiag("MLX OK", "Read OK");
        return;
      }

      Serial.println("    library init failed");
    }
  }

  Wire.setClock(50000);

  if (!sawAnyAck) {
    setDiag("No ACK", "Check VIN/GND");
    Serial.println("Diagnosis: no ACK on 0x5A/0x5B/0x5C/0x5D.");
    printWiringReminder();
    Serial.println("If module gets hot, stop powering it. That suggests damage or wiring fault.");
    return;
  }

  if (!sawReadableRaw) {
    setDiag("ACK only", "Raw read fail");
    Serial.println("Diagnosis: address ACK exists, but temp registers are unreadable.");
    Serial.println("Possible causes: damaged chip, fake/odd-compatible chip, or bus instability.");
    return;
  }

  if (!sawIdentity) {
    setDiag("Raw only", "ID read fail");
    Serial.println("Diagnosis: temp regs readable, but ID/config area unreadable.");
    Serial.println("Possible causes: partial damage or non-standard compatible chip.");
    return;
  }

  setDiag("Compat?", "Lib init fail");
  Serial.println("Diagnosis: raw registers readable, but library init fails.");
  Serial.println("This points more to compatibility or edge-case behavior than total hardware death.");
  Serial.println("MLX init failed on 0x5A/0x5B/0x5C/0x5D");
}

void renderStatus(float objC, float ambC, const char* status) {
  char line1[24];
  char line2[24];
  char line3[24];
  char line4[24];
  char line5[24];

  snprintf(line1, sizeof(line1), "MLX90614 TEST");
  snprintf(line2, sizeof(line2), "OLED: %s", g_oledReady ? "OK" : "FAIL");
  if (g_mlxReady) {
    snprintf(line3, sizeof(line3), "MLX: 0x%02X OK", g_mlxAddr);
    snprintf(line4, sizeof(line4), "Obj: %.1f C", objC);
    snprintf(line5, sizeof(line5), "Amb: %.1f C", ambC);
  } else {
    snprintf(line3, sizeof(line3), "MLX: NOT FOUND");
    snprintf(line4, sizeof(line4), "%s", status);
    snprintf(line5, sizeof(line5), "%s", g_diagHint[0] ? g_diagHint : "See Serial");
  }
  drawText(line1, line2, line3, line4, line5);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(kBootDelayMs);
  Serial.println();
  Serial.println("=== MLX90614 + OLED TEST ===");
  Serial.println("Wiring:");
  Serial.println("  OLED SDA -> GPIO8");
  Serial.println("  OLED SCL -> GPIO9");
  Serial.println("  MLX SDA  -> GPIO8");
  Serial.println("  MLX SCL  -> GPIO9");
  Serial.println("  OLED VCC -> 3.3V");
  Serial.println("  MLX VIN  -> 3.3V or 5V module VIN");
  Serial.println("  GND      -> GND");

  Wire.begin(kI2cSda, kI2cScl);
  Wire.setClock(50000);
  Wire.setTimeOut(80);

  initDisplay();
  for (uint32_t speed : kScanSpeeds) {
    printScanAtSpeed(speed);
  }
  initMlx();

  if (!g_mlxReady) {
    renderStatus(NAN, NAN, g_diagStatus);
  }
}

void loop() {
  const uint32_t now = millis();
  if (now - g_lastRefreshMs < kRefreshMs) {
    return;
  }
  g_lastRefreshMs = now;

  if (!g_mlxReady) {
    Serial.println("MLX status: not initialized");
    renderStatus(NAN, NAN, g_diagStatus);
    return;
  }

  const float objC = mlx.readObjectTempC();
  const float ambC = mlx.readAmbientTempC();
  const bool ok = isfinite(objC) && isfinite(ambC);

  if (ok) {
    Serial.print("MLX OK | addr=0x");
    if (g_mlxAddr < 16) {
      Serial.print('0');
    }
    Serial.print(g_mlxAddr, HEX);
    Serial.print(" | Obj=");
    Serial.print(objC, 2);
    Serial.print(" C | Amb=");
    Serial.print(ambC, 2);
    Serial.println(" C");
    renderStatus(objC, ambC, "Read OK");
  } else {
    Serial.println("MLX read failed after init");
    renderStatus(NAN, NAN, "Read failed");
  }
}
