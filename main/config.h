/*
 Acc Z - GPIO36  ADC1_CHANNEL_0 data[0][i]
 Acc Y - GPIO37  ADC1_CHANNEL_1  data[1][i]
 Acc X - GPIO38 ADC1_CHANNEL_2  data[2][i]

 */



#ifndef MAIN_CONFIG_H_
#define MAIN_CONFIG_H_
#define FEATURES_NUM 7
#define DEFAULT_VREF    1109        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   512   //ULP got 512 readings i.e.
#define FREQ 50 // sampling frequency
#define SAMP_MS 1000.0/FREQ

#define DATA_BUF_SIZE 512 //size of buffer for acc readings
#define NUM_OF_WINDOWS 4 //number of winodws size = NO_OF_SAMPLES to store before calculate features
#define BATCH_SIZE NUM_OF_WINDOWS*NO_OF_SAMPLES

#define EXAMPLE_WIFI_SSID "**********"
#define EXAMPLE_WIFI_PASS "***********"

#define UDP_PORT CONFIG_EXAMPLE_PORT

typedef struct
{
	int32_t ts;
	int16_t ax,ay,az,gx,gy,gz;
	float yaw,pitch,roll;
} SMessage_t;



#endif /* MAIN_CONFIG_H_ */
