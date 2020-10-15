#include "contiki.h"
#include "../shared_processes.h"

#include "sensortag/board-peripherals.h"
#include "sys/ctimer.h"
#include "rest-engine.h"
// #include <unistd.h>

#define G_BUFF_SIZE 1000
#define CHUNKS_TOTAL 2050

PROCESS(gyro_thread, "Gyro sensor processing thread");

static struct ctimer gyro_timer;

int axis;
int num_samples = 10;
int counter = 0;
// int hit_flag = 0;

int last_data_reading = -1;

char gyro_buffer[G_BUFF_SIZE];

int32_t *r_offset;

uint8_t *r_buffer;

int32_t strpos;

static void gyro_get_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
void init_gyro(void *);

PARENT_RESOURCE(res_gyro,
        "title=\"Gyro Data\";rt=\"Data\"",
        gyro_get_handler,
        NULL,
        NULL,
        NULL);

static void
gyro_get_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

    strpos = 0;
    r_offset = offset;
    r_buffer = buffer;

    // if (!hit_flag) {
    char *url;
    REST.get_url(request, &url);
    num_samples = get_url_num_samples(url);
    set_axis(url);

    init_gyro(NULL);


    // buff_pos += snprintf((char *)gyro_buffer + buff_pos, G_BUFF_SIZE - buff_pos, "X axis reading = (%d) degrees %f, %d\n", (int)(last_data_reading *1.0) / (65536 / 500), 1.60, 4);

    // hit_flag = 1;
    // }
    // REST.set_response_payload();
    // // Last iteration
    // if (!axis) {
    //     hit_flag = 0;
    // }

    // while (axis) {
    //     // usleep(100);

    // }

    if(strpos > preferred_size) {
        strpos = preferred_size;
        /* Truncate if above CHUNKS_TOTAL bytes. */
    }
    if(*offset + (int32_t)strpos > CHUNKS_TOTAL) {
        strpos = CHUNKS_TOTAL - *offset;
    }
    REST.set_response_payload(response, buffer, strpos);

    /* IMPORTANT for chunk-wise resources: Signal chunk awareness to REST engine. */
    *offset += strpos;

    // REST.set_response_payload(response, buffer, strpos);
    REST.set_response_payload(response, gyro_buffer, strpos);
    strpos = 0;
        
}

void send_return(int x, int y, int z) {
    int data = 300;
    char c;
    if (axis == 1) {
        data = x;
        c = 'X';
    } else if (axis == 2) {
        data = y;
        c = 'Y';
    } else if (axis == 3) {
        data = z;
        c = 'Z';
    } else {
        return;
    }
    last_data_reading = data;
    strpos += snprintf((char *)gyro_buffer + strpos, CHUNKS_TOTAL - strpos, "%c = %d\n", c, (int)(data * 1.0) / (65536 / 500));
    if (counter == num_samples) {
        r_offset = -1;
    }
}

int get_url_num_samples(char *url) {
    return 5;
}

void set_axis(char *url) {
    axis = 3;
}

void init_gyro(void *ptr) {
    if (counter >= num_samples) {
        counter = 0;
        axis = 0;
        return;
    }
    if (!axis) return;
    counter++;
    mpu_9250_sensor.configure(SENSORS_ACTIVE, MPU_9250_SENSOR_TYPE_GYRO);
    SENSORS_ACTIVATE(mpu_9250_sensor);
}

PROCESS_THREAD(gyro_thread, ev, data) {
    int val_x;
    int val_y;
    int val_z;

    PROCESS_BEGIN();

    while(1) {
        PROCESS_YIELD();

        if (ev == sensors_event) {
            if (data == &mpu_9250_sensor) {
                val_x = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_X);
                val_y = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_Y);
                val_z = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_Z);
                SENSORS_DEACTIVATE(mpu_9250_sensor);
                ctimer_set(&gyro_timer, 0.1*CLOCK_SECOND, init_gyro, NULL);
                send_return(val_x, val_y, val_z);
            }
        }
    }

    PROCESS_END();
}

