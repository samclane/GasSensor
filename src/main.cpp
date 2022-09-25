#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <Multichannel_Gas_GMXXX.h>
#include <samclane-project-1_inferencing.h>
#include <Seeed_Arduino_FreeRTOS.h>

#define FAN_PIN D0

GAS_GMXXX<TwoWire> gas;

TFT_eSPI tft;

bool aquireMode = true;

TaskHandle_t Handle_readTask;
TaskHandle_t Handle_inhaleTask;
TaskHandle_t Handle_classifyTask;
TaskHandle_t Handle_serialAcquireTask;
TaskHandle_t Handle_displayTask;

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

// NO2,C2H5OH,VOC,CO
GasData gasData[] = {
    {NO2, "NO2", 0.0, 650.0, 0.0, false},
    {C2H5OH, "C2H5OH", 0.0, 765.0, 0.0, false},
    {VOC, "VOC", 0.0, 775.0, 0.0, false},
    {CO, "CO", 0.0, 850.0, 0.0, false}};

ei_impulse_result_t result = {0};

void motorOn()
{
    digitalWrite(FAN_PIN, HIGH);
}

void motorOff()
{
    digitalWrite(FAN_PIN, LOW);
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

static void readSensorsThread(void *pvParameters)
{
    while (1)
    {
        gasData[NO2].concentration = gas.getGM102B();
        gasData[C2H5OH].concentration = gas.getGM302B();
        gasData[VOC].concentration = gas.getGM502B();
        gasData[CO].concentration = gas.getGM702B();
    }
}

static void inhaleThread(void *pvParameters)
{
    static bool motorRunning = false;
    while (1)
    {
        if (motorRunning)
        {
            motorOff();
            motorRunning = false;
        }
        else
        {
            motorOn();
            motorRunning = true;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void classifyThread(void *pvParameters)
{
    float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {0};
    while (1)
    {
        for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += 4)
        {
            uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000);
            tft.setCursor(0, 0);

            // add all data to the buffer
            buffer[ix + 0] = gasData[NO2].concentration;
            buffer[ix + 1] = gasData[C2H5OH].concentration;
            buffer[ix + 2] = gasData[VOC].concentration;
            buffer[ix + 3] = gasData[CO].concentration;
            vTaskDelay((next_tick - micros()) / portTICK_PERIOD_US);
        }

        signal_t signal;
        int err = numpy::signal_from_buffer(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
        if (err != 0)
        {
            ei_printf("Failed to create signal from buffer (%d)\n", err);
            return;
        }

        // Run the classifier
        result = {0};

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
    }
}

static void serialAcquireThread(void *pvParameters)
{
    Serial.println("Serial Acquire Mode");
    while (1)
    {
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
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void displayThread(void *pvParameters)
{
    while (1)
    {
        printGasDataToScreen(gasData);
        tft.println();
        printResultToScreen(&result);
        vTaskDelay((EI_CLASSIFIER_INTERVAL_MS) / portTICK_PERIOD_MS);
    }
}

void setup()
{
    Serial.begin(38400);
    vNopDelayMS(1000); // prevents usb driver crash on startup, do not omit this
    while (!Serial)
        ; // wait for serial port to connect. Needed for native USB port only

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

    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 4)
    {
        ei_printf("ERR: EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME should be equal to 4 (the 3 sensor axes)\n");
        return;
    }

    // finally
    beep(500);

    xTaskCreate(readSensorsThread, "readSensorsThread", 1024, NULL, tskIDLE_PRIORITY + 5, &Handle_readTask);
    xTaskCreate(classifyThread, "classifyThread", 1024, NULL, tskIDLE_PRIORITY + 4, &Handle_classifyTask);
    xTaskCreate(serialAcquireThread, "serialAcquireThread", 1024, NULL, tskIDLE_PRIORITY + 3, &Handle_serialAcquireTask);
    xTaskCreate(displayThread, "displayThread", 1024, NULL, tskIDLE_PRIORITY + 6, &Handle_displayTask);
    xTaskCreate(inhaleThread, "inhaleThread", 1024, NULL, tskIDLE_PRIORITY + 1, &Handle_inhaleTask);
    vTaskStartScheduler();
}

void loop()
{
    // FreeRTOS Threads handle everything
}
