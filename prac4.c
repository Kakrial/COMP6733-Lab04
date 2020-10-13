/**
 * \file
 *         A TCP socket echo server. Listens and replies on port 8080
 * \author
 *         mds
 */

#include "contiki.h"
#include "contiki-net.h"
#include "sys/cc.h"
#include "dev/leds.h"
#include "sys/etimer.h"
#include "buzzer.h"
#include "dev/serial-line.h"
#include "dev/cc26xx-uart.h"
#include "sys/ctimer.h"
#include "ieee-addr.h"
// #include "sys/node-id.h"

#include "board-peripherals.h"
#include "ti-lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERVER_PORT 8080
#define DEFAULT_BUZZ_FREQ 1000

static struct tcp_socket socket;

#define INPUTBUFSIZE 400
static uint8_t inputbuf[INPUTBUFSIZE];

#define OUTPUTBUFSIZE 400
static uint8_t outputbuf[OUTPUTBUFSIZE];

static char myBuff[INPUTBUFSIZE];

PROCESS(tcp_server_process, "TCP echo process");
PROCESS(etimer_thread, "Etimer for counter");
PROCESS(my_sensors_thread, "Sensors process");
// AUTOSTART_PROCESSES(&tcp_server_process, &my_sensors_thread);
AUTOSTART_PROCESSES(&tcp_server_process, &etimer_thread, &my_sensors_thread);

// static uint8_t get_received;
// static int bytes_to_send;

static int g_state = 0;
static int r_state = 0;

static int buz_state = 0;
static int buz_freq = DEFAULT_BUZZ_FREQ;

static uint8_t ieee_addr[8];
char nodeId[100] = {0};

static struct ctimer c_timer;

static struct ctimer humidity_timer;
static struct ctimer pressure_timer;

int humidity_counter = 0;
int humidity_max = 0;
int pressure_counter = 0;
int pressure_max = 0;
int sensorEndFlag = 0;
int hitCounter = 0;

int secondsCounter = 0;

clock_time_t begin;

/*---------------------------------------------------------------------------*/
// Function definitions
void process_key(char c);
void pressure_init(void*);
void humidity_init(void*);
void setLeds();
static void processRequest(int bytes, char* data);
void setBuzzer(int);
// HTTP return endpoints
static void startPage();
// static void mainPage(char *data, char * a, char*b, char*c, int);
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
//Input data handler
static int input(struct tcp_socket *s, void *ptr, const uint8_t *inputptr, int inputdatalen) {

  	printf("input %d bytes '%s'\n\r", inputdatalen, inputptr);

	// tcp_socket_send_str(&socket, inputptr);	//Reflect byte
	processRequest(inputdatalen, inputptr);

	// process_key((char)inputptr[0]);

	//Clear buffer
	memset(inputptr, 0, inputdatalen);
    return 0; // all data consumed 
}

bool startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : memcmp(pre, str, lenpre) == 0;
}

int parseHeader(const char *data, int len, char *p1, char *p2, char *p3) {
	int num = 0;
	int slashCounter = 0;
	int index = 0;
	for (int i = 0; i < len; i++) {
		char c = data[i];
		if (c == '\0') break;
		if (c == '/') {
			slashCounter++;
			index = 0;
			continue;
		}
		if (c == ' ' && slashCounter > 0) break;
		if (slashCounter == 0) continue;
		if (slashCounter == 1) {
			p1[index] = c;
			num = 1;
		} else if (slashCounter == 2) {
			p2[index] = c;
			num = 2;
		} else if (slashCounter == 3) {
			p3[index] = c;
			num = 3;
		}
		index++;
	}
	return num;
}

void ledHTML() {
	if (g_state) {
		tcp_socket_send_str(&socket, "<p>Green LED: ON</p>");
	} else {
		tcp_socket_send_str(&socket, "<p>Green LED: OFF</p>");
	}
	if (r_state) {
		tcp_socket_send_str(&socket, "<p>Red LED: ON</p>");
	} else {
		tcp_socket_send_str(&socket, "<p>Red LED: OFF</p>");
	}
}

void endHTTP() {
	char *string = "</body> </html>\r\n";
	tcp_socket_send_str(&socket, string);
	tcp_socket_close(&socket);
	tcp_socket_close(&socket);
}

static void processRequest(int bytes, char* data) {
	// make this more secure
	if (bytes < 3) return;
	data[bytes-1]='\0';
	char par1[100] = {0};//] = calloc(bytes, sizeof(char));
	char par2[100] = {0};// = calloc(bytes, sizeof(char));
	char par3[100] = {0};// = calloc(bytes, sizeof(char));
	int waitFlag = 0;
	if (startsWith("GET", data)) {
		hitCounter++;
		startPage();
		int numParams = parseHeader(data, bytes, par1, par2, par3);
		if (numParams == 2) {
			if (strcmp(par1,"buzzer") == 0) {
			 	int num = atoi(par2);
			 	if (num >= 0) {
					 if (num) {
						 buz_state = 1;
					 }
					 setBuzzer(num);
			 	}
			} else if (strcmp(par1,"humidity") == 0) {
			 	int num = atoi(par2);
			 	if (num > 0) {
					 humidity_max = num;
					 process_key('h');
					 waitFlag = 1;
			 	}
			} else if (strcmp(par1,"pressure") == 0) {
			 	int num = atoi(par2);
			 	if (num > 0) {
					 pressure_max = num;
					 process_key('p');
					 waitFlag = 1;
			 	}
			}
		} else if (numParams == 3) {
			if (!strcmp(par1,"leds")) {
				int val = atoi(par3);
				if (val != 1) val = 0;
				switch (par2[0]) {
					case 'r':
						r_state = val;
						break;
					case 'g':
						g_state = val;
						break;
					case 'a':
						r_state = val;
						g_state = val;
						break;
				}
				setLeds();
			}
		}
		// mainPage(data, par1, par2, par3, numParams);
		ledHTML();
		if (!waitFlag) {
			endHTTP();
		}
	}
}

void setLeds() {
	if (g_state) leds_on(LEDS_GREEN);
	else leds_off(LEDS_GREEN);
	if (r_state) leds_on(LEDS_RED);
	else leds_off(LEDS_RED);
}

static void startPage() {
	char result[1000] = {0};
	sprintf(result, "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n <!DOCTYPE html> <html> <head> <title>Page Title</title> </head> <body> <h1>Welcome to the SensorTag page</h1> <p>Uplink time: %d seconds </p><p>NodeId = %s</p><p>Number of hits = %d.</p> ", secondsCounter, nodeId, hitCounter);
	// sprintf(result, "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n <!DOCTYPE html> <html> <head> <title>Page Title</title> </head> <body> <h1>Welcome to the SensorTag page</h1> <p>Uplink time: </p><p>NodeId = %s</p><p>Number of hits = %d.</p> <p>The recieved request as follows:<p>Data: %s</p><p>Number parameters: %d</p><p>Parameter 1: %s</p><p>Parameter 2: %s</p><p>Parameter 3: %s</p> </body> </html>\r\n", nodeId, hitCounter, data, numParams, a, b, c);

	tcp_socket_send_str(&socket, result);
}


// void send_string(const char * string) {
// 	tcp_socket_send_str(&socket, string);
// }
/*---------------------------------------------------------------------------*/
//Event handler
static void event(struct tcp_socket *s, void *ptr, tcp_socket_event_t ev) {
  printf("event %d\n", ev);
}


/*---------------------------------------------------------------------------*/

void setBuzzer(int freq) {
	buz_freq = freq;
	if (buz_state) {
		buzzer_stop();
		buzzer_start(buz_freq);
	}
}

void callback(void *ptr) {
	setBuzzer(DEFAULT_BUZZ_FREQ);
}

void mychange_buzzer(int delta) {
	if (!buz_state) return;
	setBuzzer(buz_freq + delta);
	if (ctimer_expired(&c_timer))
		ctimer_set(&c_timer, 5 * CLOCK_SECOND, callback, NULL);
	else
		ctimer_restart(&c_timer);
}

void stringCopy(char *src, char * dest, int num_bytes, int dest_begin) {
	for (int i = 0; i < num_bytes; i++) {
		dest[i+dest_begin] = src[i];
	}
}

void setNodeId() {
	for (int i = 0; i < 8; i++) {
		char val[4] = {0}; 
		sprintf(val, "%02X ", ieee_addr[i]);
		stringCopy(val, nodeId, 3, i*3);
	}
}

void process_key(char c) {
	switch (c) {
		case 'p':
			pressure_init(NULL);
			break;
		case 'h':
			humidity_init(NULL);
			break;
		default:
			break;
	}
}

void humidity_init(void *nothing) {
	if (humidity_counter < humidity_max) {
		if (humidity_counter == humidity_max-1)
			sensorEndFlag = 1;
		SENSORS_ACTIVATE(hdc_1000_sensor);
		humidity_counter++;
	} else {
		humidity_counter = 0;
	}
}

void pressure_init(void *nothing) {
	if (pressure_counter < pressure_max) {
		if (pressure_counter == pressure_max-1)
			sensorEndFlag = 1;
		SENSORS_ACTIVATE(bmp_280_sensor);
		pressure_counter++;
	} else {
		pressure_counter = 0;
	}
}
/*---------------------------------------------------------------------------*/
//TCP Server process
PROCESS_THREAD(tcp_server_process, ev, data) {
	ieee_addr_cpy_to(ieee_addr, 8);
	setNodeId();
  	PROCESS_BEGIN();

	//Register TCP socket
  	tcp_socket_register(&socket, NULL,
               inputbuf, sizeof(inputbuf),
               outputbuf, sizeof(outputbuf),
               input, event);
  	tcp_socket_listen(&socket, SERVER_PORT);

	printf("Listening on %d\n", SERVER_PORT);
	
	while(1) {
	
		//Wait for event to occur
		PROCESS_PAUSE();
	}
	PROCESS_END();
}

/*---------------------------------------------------------------------------*/
//Execution counter thread
PROCESS_THREAD(etimer_thread, ev, data) {
  static struct etimer timer_etimer;

  PROCESS_BEGIN();

  while(1) {
    etimer_set(&timer_etimer, CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
    secondsCounter++;
  }

  PROCESS_END();
}


/*---------------------------------------------------------------------------*/
//Sensors thread
void send_sensor_data(char *message, int value) {
	int len = sprintf(myBuff,"<p>%s = %d.%02d</p>", message, value/100, value%100);
	tcp_socket_send_str(&socket, myBuff);
	memset(myBuff, 0, len);
	if (sensorEndFlag) {
		endHTTP();
		sensorEndFlag = 0;
	}
}

PROCESS_THREAD(my_sensors_thread, ev, data) {
	int humidity_val;
	int pressure;
	PROCESS_BEGIN();

	while(1) {
		PROCESS_YIELD();

		if (ev == sensors_event) {
			if (data == &hdc_1000_sensor) {
				humidity_val = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_HUMIDITY);
				send_sensor_data("Humidity", humidity_val);
				SENSORS_DEACTIVATE(hdc_1000_sensor);
				ctimer_set(&humidity_timer, CLOCK_SECOND, humidity_init, NULL);
			} else if (data == &bmp_280_sensor) {
				pressure = bmp_280_sensor.value(BMP_280_SENSOR_TYPE_PRESS);
				send_sensor_data("Pressure", pressure);
				SENSORS_DEACTIVATE(bmp_280_sensor);
				ctimer_set(&pressure_timer, CLOCK_SECOND, pressure_init, NULL);
			}
		}

	}
	PROCESS_END();
}
