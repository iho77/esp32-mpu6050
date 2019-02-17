/* ADC1 Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <lwip/sockets.h>
#include "mpu_task.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "driver/rtc_io.h"
#include "driver/rtc_cntl.h"
#include "driver/dac.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "string.h"
#include "lora.h"
#include "config.h"
#include "features.h"
#include "udp_sender.h"



#define USE_TASK_NOTIFICATIONS 1

#define STORAGE_NAMESPACE "storage"
#define SENDER_PORT_NUM 9999
#define RECEIVER_PORT_NUM 9999
#define RECEIVER_IP_ADDR "192.168.1.2"

char my_ip[32];



// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;




// Mount path for the partition
const char *base_path = "/spiflash";

int32_t window[NO_OF_SAMPLES][3], batch[NO_OF_SAMPLES*NUM_OF_WINDOWS][3];

float features[FEATURES_NUM];

int32_t restart_counter;



typedef enum {
	READ = 0,
	SAVE = 1,
	RESET = 2
} opcode_t;

typedef enum {
	LoRa = 0,
	RAW = 1
} op_mode_t;

op_mode_t op_mode = LoRa, check_mode();

esp_err_t restart_counter_op(opcode_t), data_op(opcode_t);

static void  lorasend(), proceed_features();

QueueHandle_t xQueue = NULL;




void app_main()
{
	static const char *TAG = "main";


	esp_err_t err = nvs_flash_init();

	       if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
	           // NVS partition was truncated and needs to be erased
	           // Retry nvs_flash_init
	           ESP_ERROR_CHECK(nvs_flash_erase());
	           err = nvs_flash_init();
	       }
	       ESP_ERROR_CHECK( err );

    op_mode = check_mode();

    xQueue = xQueueCreate( 50, sizeof(SMessage_t));

    switch (op_mode){
        	case RAW:
        		ESP_LOGI(TAG, "Entereing RAW mode loop");
        		xQueue = xQueueCreate( 50, sizeof(SMessage_t));
        		if (xQueue != NULL){
        			ESP_LOGI(TAG, "Queue created");
        			xTaskCreate(&udp_sender, "udp_sender", 2048, xQueue, 5, NULL);
        			ESP_LOGI(TAG, "UDP sender created");
        			xTaskCreate(&mpu_task, "mpu_task", 2048, xQueue, 5, NULL);
        			ESP_LOGI(TAG, "MPU task created");
        		} else {
        			ESP_LOGI(TAG, "Queue created failed");
        		}

         		break;
        	case LoRa:

        		break;
        }







}

void proceed_features(){
	features_init(batch);
	for(int i=0;i<FEATURES_NUM;i++) {
		features[i] = features_calc[i]();
		printf("Feature #%d = %f\n",i,features[i]);
	}
}




esp_err_t restart_counter_op(opcode_t cmd){
	nvs_handle my_handle;
	esp_err_t err;

	// Open
	err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) return err;

	// Read
	switch(cmd){
	case READ:
		 restart_counter = 0; // value will default to 0, if not set yet in NVS
	     err = nvs_get_i32(my_handle, "restart_conter", &restart_counter);
	 	 if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
	 	 break;

	case SAVE:
		restart_counter++;
	    err = nvs_set_i32(my_handle, "restart_conter", restart_counter);
	    if (err != ESP_OK) return err;
	    break;

	case RESET:
		err = nvs_set_i32(my_handle, "restart_conter", 0);
        if (err != ESP_OK) return err;
        break;
	}
	    // Commit written value.
	    // After setting any values, nvs_commit() must be called to ensure changes are written
	    // to flash storage. Implementations may write to storage at other times,
	    // but this is not guaranteed.
     	err = nvs_commit(my_handle);
	    if (err != ESP_OK) return err;

	    // Close
	    nvs_close(my_handle);
	    return ESP_OK;
}



esp_err_t data_op(opcode_t cmd){

	static const char *TAG = "data_op";

	ESP_LOGI(TAG, "Mounting FAT filesystem");
	FILE *f;
	// To mount device we need name of device partition, define base_path
	// and allow format partition in case if it is new one and was not formated before
	const esp_vfs_fat_mount_config_t mount_config = {
	            .max_files = 4,
	            .format_if_mount_failed = true
	            //.allocation_unit_size = CONFIG_WL_SECTOR_SIZE  Outdated API????
	    };
	esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, "storage", &mount_config, &s_wl_handle);
	if (err != ESP_OK) {
	        ESP_LOGE(TAG, "Failed to mount FATFS");
	        return err;
	   }

	switch(cmd){
	case SAVE:
		f = fopen("/spiflash/data.bin", "ab");
	    if (f == NULL) {
		  ESP_LOGE(TAG, "Failed to open file for writing");
		  return ESP_FAIL;}
	    ESP_LOGI(TAG,"Writing %d bytes to file",sizeof(window));
	    fwrite(window,sizeof(window),1,f);
	    fclose(f);
	    ESP_LOGI(TAG, "File written");
	    break;
	case READ:
		f = fopen("/spiflash/data.bin", "rb");
		if (f == NULL) {
		  ESP_LOGE(TAG, "Failed to open file for writing");
		return ESP_FAIL;}
		ESP_LOGI(TAG,"Reading %d bytes from file",sizeof(batch));
		fread(batch,sizeof(batch),1,f);
		fclose(f);
		ESP_LOGI(TAG, "File read");
		break;
	case RESET:
		f = fopen("/spiflash/data.bin", "w");
		if (f == NULL) {
		  ESP_LOGE(TAG, "Failed to open file for writing");
		 return ESP_FAIL;}
		fclose(f);
		ESP_LOGI(TAG, "File erased");
		break;

	}

	 ESP_LOGI(TAG, "Unmounting FAT filesystem");
	 ESP_ERROR_CHECK( esp_vfs_fat_spiflash_unmount(base_path, s_wl_handle));
	 ESP_LOGI(TAG, "Done");
	 return ESP_OK;
}


void lorasend(){
	static const char *TAG = "lora_send";
	lora_init();
	lora_set_frequency(433e6);
   	lora_enable_crc();
	uint8_t msg[6+1+FEATURES_NUM*sizeof(float)]; //features + addr + number of features
	//printf("Size of LoRa packet is:%d\n", sizeof(msg));
	esp_efuse_mac_get_default(msg);
	//printf("Device Address is:%02x:%02x:%02x:%02x:%02x:%02x\n",msg[0],msg[1],msg[2],msg[3],msg[4],msg[5]);
	msg[6]=FEATURES_NUM;
	memcpy(&msg[7],features,sizeof(features));
    lora_send_packet((uint8_t *)(&msg), sizeof(msg));

}







mode_t check_mode(){
	return RAW;
}

