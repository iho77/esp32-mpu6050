#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "config.h"
#include "udp_sender.h"

static EventGroupHandle_t wifi_event_group;
static void initialise_wifi();

const int IPV4_GOTIP_BIT = BIT0;
char my_ip[32];


esp_err_t event_handler(void *ctx, system_event_t *event)
{    static const char *TAG = "WiFi event handler";

	 switch(event->event_id) {
	    case SYSTEM_EVENT_STA_START:
	        esp_wifi_connect();
	        ESP_LOGI(TAG,"WiFi connected");
	        break;
	    case SYSTEM_EVENT_STA_GOT_IP:
	        xEventGroupSetBits(wifi_event_group, IPV4_GOTIP_BIT);
	        ESP_LOGI(TAG,"WiFi got IP");
	        sprintf(my_ip,IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
	        break;
	    case SYSTEM_EVENT_STA_DISCONNECTED:
	        /* This is a workaround as ESP32 WiFi libs don't currently
	           auto-reassociate. */
	        esp_wifi_connect();
	        xEventGroupClearBits(wifi_event_group, IPV4_GOTIP_BIT);
	        break;
	    default:
	        break;
	    }
	    return ESP_OK;
}



static void initialise_wifi(void)
{   static const char *TAG = "init_wifi";
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}




void udp_sender(void *pvParameters){

	 static const char *TAG = "sender_thread";

	 QueueHandle_t q = pvParameters;

	 int socket_fd;

	 unsigned long st;

	 uint32_t ulNotifiedValue;


	 struct sockaddr_in sa,ra;

	 int sent_data;

	 ESP_LOGI(TAG, "Waiting for AP connection...");
     initialise_wifi();
	 xEventGroupWaitBits(wifi_event_group, IPV4_GOTIP_BIT, false, true, portMAX_DELAY);
	 ESP_LOGI(TAG, "Connected to AP");

	 /* Creates an UDP socket (SOCK_DGRAM) with Internet Protocol Family (PF_INET).
	  * Protocol family and Address family related. For example PF_INET Protocol Family and AF_INET family are coupled.
	 */
	 socket_fd = socket(PF_INET, SOCK_DGRAM, 0);

	 if ( socket_fd < 0 )
	 {

	     ESP_LOGI(TAG,"socket call failed");
	     return;

	 }

	 memset(&sa, 0, sizeof(struct sockaddr_in));

	 sa.sin_family = AF_INET;
	 sa.sin_addr.s_addr = inet_addr(my_ip);
	 sa.sin_port = htons(SENDER_PORT_NUM);


	 /* Bind the TCP socket to the port SENDER_PORT_NUM and to the current
	 * machines IP address (Its defined by SENDER_IP_ADDR).
	 * Once bind is successful for UDP sockets application can operate
	 * on the socket descriptor for sending or receiving data.
	 */
	 if (bind(socket_fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) == -1)
	 {
	   printf("Bind to Port Number %d ,IP address %s failed\n",SENDER_PORT_NUM,my_ip /*SENDER_IP_ADDR*/);
	   close(socket_fd);
	   return;
	 }
	 printf("Bind to Port Number %d ,IP address %s SUCCESS!!!\n",SENDER_PORT_NUM,my_ip);



	 memset(&ra, 0, sizeof(struct sockaddr_in));
	 ra.sin_family = AF_INET;
	 ra.sin_addr.s_addr = inet_addr(RECEIVER_IP_ADDR);
	 ra.sin_port = htons(RECEIVER_PORT_NUM);

	 SMessage_t msg;

     while(1){

     if( xQueueReceive(q,&msg,( TickType_t ) 10 )){
    	 sent_data = sendto(socket_fd, &msg,sizeof(SMessage_t),0,(struct sockaddr*)&ra,sizeof(ra));
 	     vTaskDelay(20 / portTICK_PERIOD_MS);
       //printf("sending sample #:%d\n",i);

  	     if(sent_data < 0)
  	 	     {
   	 	        printf("send failed\n");
   	 	     }

     }
     }




 }
