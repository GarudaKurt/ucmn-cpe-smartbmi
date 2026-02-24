#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <Adafruit_MLX90614.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

#include "configFirebase.h"
CONFIGFIREBASE conFirebase;

// ===== TFT PINS =====
#define TFT_MOSI 6
#define TFT_SCLK 7
#define TFT_CS   10
#define TFT_DC   5
#define TFT_RST  4

// ===== I2C PINS =====
#define SDA_PIN 8
#define SCL_PIN 9

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

MAX30105 particleSensor;
Adafruit_MLX90614 mlx;

// ===== SPO2 BUFFERS =====
#define BUFFER_SIZE 100
uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];

int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;

float temperature = 0;

unsigned long lastSampleTime   = 0;
unsigned long lastComputeTime  = 0;
unsigned long lastTempReadTime = 0;
unsigned long lastUIUpdate     = 0;
unsigned long prevTime = 0;

const unsigned long SAMPLE_INTERVAL   = 10;
const unsigned long COMPUTE_INTERVAL  = 4000;
const unsigned long TEMP_INTERVAL     = 5000;
const unsigned long UI_INTERVAL       = 120;

int bufferIndex = 0;
bool bufferReady = false;

// ===== Layout =====
#define ICON_X   40
#define VALUE_X  80
#define HR_Y     30
#define SPO2_Y   100
#define TEMP_Y   170

// ===== Animation =====
int heartPulse = 0;
bool heartGrow = true;
int waveOffset = 0;

// ===== UI Functions =====
void drawStaticUI() {
  tft.fillScreen(GC9A01A_BLACK);

  tft.setTextColor(GC9A01A_WHITE);
  tft.setTextSize(1);

  tft.setCursor(VALUE_X, HR_Y - 8); tft.print("HR");
  tft.setCursor(VALUE_X, SPO2_Y - 8); tft.print("SpO2");
  tft.setCursor(VALUE_X, TEMP_Y - 8); tft.print("Temp");
}

void updateValues() {
  // Clear previous values
  tft.fillRect(VALUE_X, HR_Y, 100, 20, GC9A01A_BLACK);
  tft.fillRect(VALUE_X, SPO2_Y, 100, 20, GC9A01A_BLACK);
  tft.fillRect(VALUE_X, TEMP_Y, 100, 20, GC9A01A_BLACK);

  tft.setTextColor(GC9A01A_GREEN);
  tft.setTextSize(2);

  tft.setCursor(VALUE_X, HR_Y); tft.print(validHeartRate ? heartRate : 0); tft.print(" bpm");
  tft.setCursor(VALUE_X, SPO2_Y); tft.print(validSPO2 ? spo2 : 0); tft.print(" %");
  tft.setCursor(VALUE_X, TEMP_Y); tft.print(temperature, 1); tft.print(" C");
}

void drawAnimations() {
  // Clear icon areas only
  tft.fillRect(ICON_X - 10, HR_Y - 5, 30, 30, GC9A01A_BLACK);
  tft.fillRect(ICON_X - 10, SPO2_Y - 5, 30, 30, GC9A01A_BLACK);
  tft.fillRect(ICON_X - 10, TEMP_Y - 5, 30, 30, GC9A01A_BLACK);

  int size = 3 + heartPulse;
  tft.fillCircle(ICON_X, HR_Y + 8, size, GC9A01A_RED);
  tft.fillCircle(ICON_X + 6, HR_Y + 8, size, GC9A01A_RED);
  tft.fillTriangle(ICON_X - 4, HR_Y + 8, ICON_X + 8, HR_Y + 8, ICON_X + 2, HR_Y + 20, GC9A01A_RED);

  tft.drawCircle(ICON_X, SPO2_Y + 8, 6, GC9A01A_CYAN);
  tft.setTextColor(GC9A01A_CYAN);
  tft.setTextSize(1);
  tft.setCursor(ICON_X - 3, SPO2_Y + 4); tft.print("O");
  tft.setCursor(ICON_X + 5, SPO2_Y + 10); tft.print("2");

  int bx = ICON_X - 3;
  int by = TEMP_Y;
  tft.drawRoundRect(bx, by, 6, 16, 3, GC9A01A_ORANGE);
  tft.fillCircle(bx + 3, by + 18, 3, GC9A01A_ORANGE);
  int mercury = 4 + (waveOffset * 2);
  tft.fillRect(bx + 1, by + 16 - mercury, 4, mercury, GC9A01A_ORANGE);

   heartPulse += heartGrow ? 1 : -1;
  if (heartPulse > 2 || heartPulse < 0) heartGrow = !heartGrow;
  waveOffset = (waveOffset + 1) % 4;
}

void setup() {
  Serial.begin(115200);

  conFirebase.initFirebase();
  delay(1500);
  Wire.begin(SDA_PIN, SCL_PIN);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  SPI.setFrequency(1000000);

  tft.begin();
  tft.setRotation(0);
  drawStaticUI();

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 not found!"); while(true);
  }
  particleSensor.setup(60, 4, 2, 100, 411, 4096);
  particleSensor.setPulseAmplitudeRed(0x1F);
  particleSensor.setPulseAmplitudeIR(0x1F);

  if (!mlx.begin()) { Serial.println("MLX90614 not found!"); while(true); }
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = currentTime;
    if (particleSensor.available()) {
      redBuffer[bufferIndex] = particleSensor.getRed();
      irBuffer[bufferIndex]  = particleSensor.getIR();
      particleSensor.nextSample();
      bufferIndex++;
      if (bufferIndex >= BUFFER_SIZE) { bufferIndex = 0; bufferReady = true; }
    } else particleSensor.check();
  }

  if (bufferReady && currentTime - lastComputeTime >= COMPUTE_INTERVAL) {
    lastComputeTime = currentTime;

    maxim_heart_rate_and_oxygen_saturation(
      irBuffer, BUFFER_SIZE,
      redBuffer,
      &spo2, &validSPO2,
      &heartRate, &validHeartRate
    );

    float temp = particleSensor.readTemperature();
    temperature = temp;

    Serial.print("HR: "); Serial.print(validHeartRate ? heartRate : 0);
    Serial.print(" | SpO2: "); Serial.print(validSPO2 ? spo2 : 0);
    Serial.print(" | Temp: "); Serial.print(temp); Serial.println(" °C");
  }

  if (currentTime - lastTempReadTime >= TEMP_INTERVAL) {
    lastTempReadTime = currentTime;
    float tempC = mlx.readObjectTempC();
    if (!isnan(tempC)) temperature = tempC;
  }

  if (currentTime - lastUIUpdate >= UI_INTERVAL) {
    lastUIUpdate = currentTime;
    updateValues();
    drawAnimations();
  }

  if(currentTime - prevTime >= 5000) {
    prevTime = currentTime;
    if(!conFirebase.WiFiError()) {
      conFirebase.sendFirebaseData(heartRate, spo2, temperature);
    }
  }
}