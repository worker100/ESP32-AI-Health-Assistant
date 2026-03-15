#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <math.h>

namespace {
constexpr uint8_t kI2cSda = 8;
constexpr uint8_t kI2cScl = 9;
constexpr uint32_t kI2cClockHz = 50000;
constexpr uint32_t kReadPeriodMs = 1000;
constexpr uint32_t kRetryInitMs = 3000;
constexpr uint8_t kCandidates[] = {0x5A, 0x5B, 0x5C, 0x5D};

Adafruit_MLX90614 g_mlx;
bool g_ready = false;
uint8_t g_addr = 0;
uint32_t g_lastReadMs = 0;
uint32_t g_lastRetryMs = 0;

bool probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void scanI2cBus() {
  Serial.println("I2C scan:");
  bool found = false;
  for (uint8_t addr = 1; addr < 127; ++addr) {
    if (probe(addr)) {
      found = true;
      Serial.print("  - 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.println(addr, HEX);
    }
  }
  if (!found) {
    Serial.println("  (none)");
  }
}

bool tryInitMlx() {
  for (uint8_t addr : kCandidates) {
    if (!probe(addr)) {
      continue;
    }
    if (g_mlx.begin(addr, &Wire)) {
      g_addr = addr;
      g_ready = true;
      Serial.print("MLX90614 init OK at 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.println(addr, HEX);
      return true;
    }
  }
  g_ready = false;
  g_addr = 0;
  Serial.println("MLX90614 init failed on 0x5A/0x5B/0x5C/0x5D");
  return false;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== MLX90614 QUICK TEST ===");
  Serial.println("Wiring:");
  Serial.println("  VIN -> 3.3V (or module VIN 5V if board supports it)");
  Serial.println("  GND -> GND");
  Serial.println("  SDA -> GPIO8");
  Serial.println("  SCL -> GPIO9");

  Wire.begin(kI2cSda, kI2cScl);
  Wire.setClock(kI2cClockHz);
  Wire.setTimeOut(80);

  scanI2cBus();
  tryInitMlx();
}

void loop() {
  const uint32_t now = millis();

  if (!g_ready && (now - g_lastRetryMs >= kRetryInitMs)) {
    g_lastRetryMs = now;
    Serial.println("Retry init...");
    tryInitMlx();
  }

  if (!g_ready || (now - g_lastReadMs < kReadPeriodMs)) {
    return;
  }
  g_lastReadMs = now;

  const float obj = g_mlx.readObjectTempC();
  const float amb = g_mlx.readAmbientTempC();
  if (isfinite(obj) && isfinite(amb)) {
    Serial.print("MLX 0x");
    if (g_addr < 16) {
      Serial.print('0');
    }
    Serial.print(g_addr, HEX);
    Serial.print(" | Obj=");
    Serial.print(obj, 2);
    Serial.print(" C | Amb=");
    Serial.print(amb, 2);
    Serial.println(" C");
  } else {
    Serial.println("Read failed (NaN), module may be unstable.");
    g_ready = false;
  }
}
