// Mutex.ino
// Copied from Arduino_FreeRTOS
// Task creation changed for RP2040.

/*
   Example of a FreeRTOS mutex
   https://www.freertos.org/Real-time-embedded-RTOS-mutexes.html
*/

// Include Arduino FreeRTOS library
#ifdef ARDUINO_ARCH_RP2040
#include <FreeRTOS.h>
#else
#include <Arduino_FreeRTOS.h>
#endif

// Include mutex support
#include <semphr.h>

/*
   Declaring a global variable of type SemaphoreHandle_t

*/
SemaphoreHandle_t mutex;

int globalCount = 0;

void setup() {

  Serial.begin(115200);
#ifdef ARDUINO_ARCH_RP2040
  Serial.println("\nArduino RP2040");
#endif
  delay(5000);

  /**
       Create a mutex.
       https://www.freertos.org/CreateMutex.html
  */
  mutex = xSemaphoreCreateMutex();
  if (mutex != NULL) {
    Serial.println("Mutex created");
  }

  /**
     Create tasks
  */
#ifdef ARDUINO_ARCH_RP2040
  xTaskCreate(TaskMutex, "Task1", 128, nullptr, 1, nullptr);
#else
  xTaskCreate(TaskMutex, // Task function
              "Task1", // Task name for humans
              128, 
              1000, // Task parameter
              1, // Task priority
              NULL);
#endif

#ifdef ARDUINO_ARCH_RP2040
  xTaskCreate(TaskMutex, "Task2", 128, nullptr, 1, nullptr);
#else
  xTaskCreate(TaskMutex, "Task2", 128, 1000, 1, NULL);
#endif
}

void loop() {
  Serial.printf("val: %d\n", v);
  delay(1000);

}

void TaskMutex(void *pvParameters)
{
  TickType_t delayTime = *((TickType_t*)pvParameters); // Use task parameters to define delay

  for (;;)
  {
    /**
       Take mutex
       https://www.freertos.org/a00122.html
    */
    if (xSemaphoreTake(mutex, 10) == pdTRUE)
    {
      Serial.print(pcTaskGetName(NULL)); // Get task name
      Serial.print(", Count read value: ");
      Serial.print(globalCount);

      globalCount++;

      Serial.print(", Updated value: ");
      Serial.print(globalCount);

      Serial.println();
      /**
         Give mutex
         https://www.freertos.org/a00123.html
      */
      xSemaphoreGive(mutex);
    }

    vTaskDelay(delayTime / portTICK_PERIOD_MS);
  }
}
