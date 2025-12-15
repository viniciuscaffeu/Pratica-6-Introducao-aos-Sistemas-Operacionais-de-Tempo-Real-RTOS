#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- Configurações de Hardware e FreeRTOS ---
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Objeto MPU6050
Adafruit_MPU6050 mpu;
Adafruit_Sensor *mpu_temp, *mpu_accel, *mpu_gyro;

// Mutex (Semáforo de exclusão mútua) para proteger a estrutura mpu6050
SemaphoreHandle_t xMutexMPU6050 = NULL;
TickType_t timeOut = pdMS_TO_TICKS(1000);  // Timeout de 1 segundo (1000 ms)

// Estrutura de Dados Compartilhada
typedef struct {
  float temp;
  float accX;
  float accY;
  float accZ;
  float gyroX;
  float gyroY;
  float gyroZ;
} MPU6050;

MPU6050 mpu6050; // Estrutura global compartilhada

// --- Task 1: Leitura do MPU6050 ---
void mpu6050Task(void *pvParam) {
  if (!mpu.begin()) {
    Serial.println("Falha ao encontrar o chip MPU6050. I2C?");
    vTaskDelay(pdMS_TO_TICKS(5000));
    vTaskSuspend(NULL); 
  }

  mpu_temp = mpu.getTemperatureSensor();
  mpu_accel = mpu.getAccelerometerSensor();
  mpu_gyro = mpu.getGyroSensor();

  sensors_event_t accel, gyro, temp;

  while (1) {
    if (xSemaphoreTake(xMutexMPU6050, timeOut) == pdPASS) {
      mpu_temp->getEvent(&temp);
      mpu_accel->getEvent(&accel);
      mpu_gyro->getEvent(&gyro);

      mpu6050.temp = temp.temperature;
      mpu6050.accX = accel.acceleration.x;
      mpu6050.accY = accel.acceleration.y;
      mpu6050.accZ = accel.acceleration.z;
      mpu6050.gyroX = gyro.gyro.x;
      mpu6050.gyroY = gyro.gyro.y;
      mpu6050.gyroZ = gyro.gyro.z;

      xSemaphoreGive(xMutexMPU6050);
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// --- Task 2: Exibição no LCD ---
void lcdTask(void *ptParam) {
  lcd.init();
  lcd.backlight();

  const byte lcdChar = 20;
  char line0[lcdChar + 1], line1[lcdChar + 1], line2[lcdChar + 1], line3[lcdChar + 1];
  char *line[] = {line0, line1, line2, line3};

  while (1) {
    if (xSemaphoreTake(xMutexMPU6050, timeOut) == pdPASS) {
      sprintf(line0, "MPU6050 T:%d s", xTaskGetTickCount() / 1000);
      snprintf(line1, lcdChar + 1, "Temp: %.1f C", mpu6050.temp);
      snprintf(line2, lcdChar + 1, "ACC: X%.1f Y%.1f Z%.1f", mpu6050.accX, mpu6050.accY, mpu6050.accZ);
      snprintf(line3, lcdChar + 1, "GYRO: X%.1f Y%.1f Z%.1f", mpu6050.gyroX, mpu6050.gyroY, mpu6050.gyroZ);

      xSemaphoreGive(xMutexMPU6050);
    }

    for (int i = 0; i < 4; i++) {
      lcd.setCursor(0, i);
      lcd.print(line[i]);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  Wire.begin();
  Serial.begin(115200);

  xMutexMPU6050 = xSemaphoreCreateMutex();
  if (xMutexMPU6050 == NULL) {
    Serial.println("Erro ao criar o Mutex!");
    return;
  }

  xTaskCreatePinnedToCore(mpu6050Task, "MPU6050_Reader", 4096, NULL, configMAX_PRIORITIES - 2, NULL, 1);
  vTaskDelay(pdMS_TO_TICKS(1500)); 
  xTaskCreatePinnedToCore(lcdTask, "LCD_Display", 4096, NULL, tskIDLE_PRIORITY + 1, NULL, 0);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(100)); 
}
