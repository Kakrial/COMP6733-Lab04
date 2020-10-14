#include "contiki.h"

#include "sensortag/board-peripherals.h"
#include "sys/ctimer.h"
#include "rest-engine.h"
// #include <unistd.h>

#define G_BUFF_SIZE 1000

PROCESS(gyro_thread, "Gyro sensor processing thread");

static struct ctimer gyro_timer;

int axis;
int num_samples = 10;
int counter = 0;
// int hit_flag = 0;

int last_data_reading = -1;

char gyro_buffer[G_BUFF_SIZE];
int buff_pos = 0;

static void gyro_get_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
void init_gyro(void *);

RESOURCE(res_gyro,
        "title=\"Gyro Data\";rt=\"Data\"",
        gyro_get_handler,
        NULL,
        NULL,
        NULL);

static void
gyro_get_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

    int32_t strpos = 0;
    // if (!hit_flag) {
    char *url;
    REST.get_url(request, &url);
    num_samples = get_url_num_samples(url);
    set_axis(url);

    init_gyro(NULL);


    buff_pos += snprintf((char *)gyro_buffer + buff_pos, G_BUFF_SIZE - buff_pos, "X axis reading: %lf degrees\n", (last_data_reading *1.0) / (65536 / 500));

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

    if(buff_pos > preferred_size) {
        buff_pos = preferred_size;
        /* Truncate if above CHUNKS_TOTAL bytes. */
    }

    // REST.set_response_payload(response, buffer, strpos);
    REST.set_response_payload(response, gyro_buffer, buff_pos);
    buff_pos = 0;
    
}

void send_return(int x, int y, int z) {
    int data = 0;
    char c;
    if (axis == 1) {
        data = x;
        c = "X";
    } else if (axis == 2) {
        data = y;
        c = "Y";
    } else if (axis == 3) {
        data = z;
        c = "Z";
    } else {
        return;
    }
    last_data_reading = data;
    buff_pos += snprintf((char *)gyro_buffer + buff_pos, G_BUFF_SIZE - buff_pos, "%s axis reading: %lf degrees\n",(data * 1.0) / (65536 / 500));
}

int get_url_num_samples(char *url) {
    return 5;
}

void set_axis(char *url) {
    axis = 1;
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

