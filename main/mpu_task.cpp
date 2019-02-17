#include "mpu_task.hpp"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "config.h"
#include "I2Cbus.hpp"
#include "MPU.hpp"
#include "mpu/math.hpp"
#include "mpu/types.hpp"


static constexpr gpio_num_t SDA = GPIO_NUM_21;
static constexpr gpio_num_t SCL = GPIO_NUM_22;
static constexpr uint32_t CLOCK_SPEED = 240000;

static const char* TAG = "MPUmodule";

extern "C" void mpu_task(void *pvParameters){

	QueueHandle_t q = pvParameters;
	SMessage_t msg;
	MPU_t MPU;  // create a default MPU object
	TickType_t xLastWakeTime;
	static constexpr mpud::gyro_fs_t kGyroFS   = mpud::GYRO_FS_1000DPS;


	i2c0.begin(SDA, SCL, CLOCK_SPEED);

	MPU.setBus(i2c0);  // set bus port, not really needed since default is i2c0
	MPU.setAddr(mpud::MPU_I2CADDRESS_AD0_LOW);  // set address, default is AD0_LOW

    while (esp_err_t err = MPU.testConnection()) {
	        ESP_LOGE(TAG, "Failed to connect to the MPU, error=%#X", err);
	        vTaskDelay(1000 / portTICK_PERIOD_MS);
	    }
	ESP_LOGI(TAG, "MPU connection successful!");

	ESP_ERROR_CHECK(MPU.initialize());  // initialize the chip and set initial configurations
	// Setup with your configurations
	ESP_ERROR_CHECK(MPU.setSampleRate(100));  // set sample rate to 50 Hz
	ESP_ERROR_CHECK(MPU.setGyroFullScale(kGyroFS));
	ESP_ERROR_CHECK(MPU.setAccelFullScale(mpud::ACCEL_FS_8G));


	mpud::raw_axes_t accelRaw;   // x, y, z axes as int16
	mpud::raw_axes_t gyroRaw;    // x, y, z axes as int16
	float yaw,pitch,roll;
	yaw = 0;
	pitch = 0;
	roll = 0;

	xLastWakeTime = xTaskGetTickCount ();

	while(1){
		vTaskDelayUntil( &xLastWakeTime, SAMP_MS / portTICK_PERIOD_MS );
		MPU.acceleration(&accelRaw);  // fetch raw data from the registers
		MPU.rotation(&gyroRaw);
		msg.ts = xLastWakeTime;
		msg.ax = accelRaw.x;
		msg.ay = accelRaw.y;
		msg.az = accelRaw.z;
		msg.gx = gyroRaw.x;
		msg.gy = gyroRaw.y;
		msg.gz = gyroRaw.z;

		constexpr double kRadToDeg = 57.2957795131;
		constexpr float kDeltaTime = 1.f / FREQ;
		float gyroRoll             = roll + mpud::math::gyroDegPerSec(msg.gx, kGyroFS) * kDeltaTime;
		float gyroPitch            = pitch + mpud::math::gyroDegPerSec(msg.gy, kGyroFS) * kDeltaTime;
		float gyroYaw              = yaw + mpud::math::gyroDegPerSec(msg.gz, kGyroFS) * kDeltaTime;
		float accelRoll            = atan2(-msg.ax, msg.az) * kRadToDeg;
		float accelPitch = atan2(msg.ay, sqrt(msg.ax * msg.ax + msg.az * msg.az)) * kRadToDeg;
		// Fusion
		roll  = gyroRoll * 0.95f + accelRoll * 0.05f;
		pitch = gyroPitch * 0.95f + accelPitch * 0.05f;
		yaw   = gyroYaw;
		// correct yaw
		if (yaw > 180.f)
		   yaw -= 360.f;
		else if (yaw < -180.f)
		   yaw += 360.f;

		msg.yaw = yaw;
		msg.pitch = pitch;
		msg.roll = roll;

	    xQueueSend( q, ( void * ) &msg, ( TickType_t ) 0 );

	}

}

