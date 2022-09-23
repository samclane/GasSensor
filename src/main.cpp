#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <Multichannel_Gas_GMXXX.h>
#include <samclane-project-1_inferencing.h>

#define FAN_PIN D0

GAS_GMXXX<TwoWire> gas;

TFT_eSPI tft;

bool aquireMode = true;

// Create an enum for each gas type
enum GasType
{
  NO2,
  C2H5OH,
  VOC,
  CO
};

struct GasData
{
  GasType type;
  String name;
  float concentration;
  float maxConcentration;
  float minConcentration;
  bool alarm;
};

GasData gasData[] = {
    {NO2, "NO2", 0.0, 650.0, 0.0, false},
    {C2H5OH, "C2H5OH", 0.0, 765.0, 0.0, false},
    {VOC, "VOC", 0.0, 775.0, 0.0, false},
    {CO, "CO", 0.0, 850.0, 0.0, false}};

void motorOn()
{
  digitalWrite(FAN_PIN, HIGH);
}

void motorOff()
{
  digitalWrite(FAN_PIN, LOW);
}

void inhale(unsigned int dutyCycle = 1000)
{
  static unsigned int startTime = millis();
  static unsigned int endTime = millis();
  static bool inhaleState = false;

  if (inhaleState)
  {
    if (millis() - endTime > dutyCycle)
    {
      inhaleState = false;
      motorOff();
      startTime = millis();
    }
  }
  else
  {
    if (millis() - startTime > dutyCycle)
    {
      inhaleState = true;
      motorOn();
      endTime = millis();
    }
  }
}

void beep(unsigned int hold = 1000)
{
  static unsigned int startTime = millis();
  if (millis() - startTime > hold)
  {
    analogWrite(WIO_BUZZER, 128);
    startTime = millis();
  }
  else
  {
    analogWrite(WIO_BUZZER, 0);
  }
}

void printGasDataToScreen(GasData gasData[])
{
  tft.setTextSize(2);
  tft.println("Gas Concentration:");
  for (int i = 0; i < 4; i++)
  {
    if (gasData[i].alarm)
    {
      tft.setTextColor(TFT_RED, TFT_BLACK);
    }
    else
    {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    tft.print(gasData[i].name);
    tft.print(": ");
    tft.print(gasData[i].concentration);
    tft.println(" ppm");
  }
}

void printResultToScreen(ei_impulse_result_t *result)
{
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.println("Inferencing Result:");
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
  {
    tft.printf("%s: %.2f\n", result->classification[ix].label, result->classification[ix].value);
  }
}

void setup()
{
  Serial.begin(38400);
  pinMode(FAN_PIN, OUTPUT);

  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);

  pinMode(WIO_5S_UP, INPUT_PULLUP);
  pinMode(WIO_5S_DOWN, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);

  pinMode(WIO_BUZZER, OUTPUT);

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  digitalWrite(LCD_BACKLIGHT, HIGH);
  gas.begin(Wire, 0x08); // use the hardware I2C
  motorOff();
  beep(500);

  if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 4)
  {
    ei_printf("ERR: EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME should be equal to 4 (the 3 sensor axes)\n");
    return;
  }
}

void loop()
{
  inhale();
  float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {0};
  for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += 4)
  {
    uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000);
    tft.setCursor(0, 0);
    // inhale(1000);

    // GM102B NO2 sensor
    gasData[NO2].concentration = gas.getGM102B();
    gasData[NO2].alarm = gasData[NO2].concentration > gasData[NO2].maxConcentration;

    // GM302B C2H5CH sensor
    gasData[C2H5OH].concentration = gas.getGM302B();
    gasData[C2H5OH].alarm = gasData[C2H5OH].concentration > gasData[C2H5OH].maxConcentration;

    // GM502B VOC sensor
    gasData[VOC].concentration = gas.getGM502B();
    gasData[VOC].alarm = gasData[VOC].concentration > gasData[VOC].maxConcentration;

    // GM702B CO sensor
    gasData[CO].concentration = gas.getGM702B();
    gasData[CO].alarm = gasData[CO].concentration > gasData[CO].maxConcentration;

    // if any alarm is triggered, beep
    // for (int i = 0; i < 4; i++) {
    //   if (gasData[i].concentration > gasData[i].maxConcentration) {
    //     beep(100);
    //     break;
    //   }
    // }

    // add all data to the buffer
    buffer[ix + 0] = gasData[NO2].concentration;
    buffer[ix + 1] = gasData[C2H5OH].concentration;
    buffer[ix + 2] = gasData[VOC].concentration;
    buffer[ix + 3] = gasData[CO].concentration;

    if (aquireMode)
    {
      for (int i = 0; i < 4; i++)
      {
        Serial.print(gasData[i].concentration);
        if (i < 3)
          Serial.print(",");
      }
      Serial.println();
    }
    delayMicroseconds(next_tick - micros());
  }

  signal_t signal;
  int err = numpy::signal_from_buffer(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
  if (err != 0)
  {
    ei_printf("Failed to create signal from buffer (%d)\n", err);
    return;
  }

  // Run the classifier
  ei_impulse_result_t result = {0};

  err = run_classifier(&signal, &result, false);
  if (err != EI_IMPULSE_OK)
  {
    ei_printf("ERR: Failed to run classifier (%d)\n", err);
    return;
  }

  // print the predictions
  if (!aquireMode)
  {  
    ei_printf("Predictions ");
    ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
        result.timing.dsp, result.timing.classification, result.timing.anomaly);
    ei_printf(": \n");
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
    {
      ei_printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);
    }
  }
  printGasDataToScreen(gasData);
  tft.println();
  printResultToScreen(&result);
}
