/*
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *      Example resource
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include "contiki.h"

#if 1
// #if PLATFORM_HAS_RADIO

#include <string.h>
#include "rest-engine.h"
#include "dev/radio-sensor.h"

static void res_get_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/* A simple getter example. Returns the reading of the rssi/lqi from radio sensor */
RESOURCE(res_radio,
         "title=\"RADIO: ?p=lqi|rssi\";rt=\"RadioSensor\"",
         res_get_handler,
         NULL,
         NULL,
         NULL);

static void
res_get_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  size_t len = 0;
  const char *p = NULL;
  uint8_t param = 0;
  int success = 1;

  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);

  if((len = REST.get_query_variable(request, "p", &p))) {
    if(strncmp(p, "lqi", len) == 0) {
      param = RADIO_SENSOR_LAST_VALUE;
    } else if(strncmp(p, "rssi", len) == 0) {
      param = RADIO_SENSOR_LAST_PACKET;
    } else {
      success = 0;
    }
  } else {
    success = 0;
  } if(success) {
    if(accept == -1 || accept == REST.type.TEXT_PLAIN) {
      REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
      snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "%d", radio_sensor.value(param));

      REST.set_response_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
    } else if(accept == REST.type.APPLICATION_JSON) {
      REST.set_header_content_type(response, REST.type.APPLICATION_JSON);

      if(param == RADIO_SENSOR_LAST_VALUE) {
        snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "{'lqi':%d}", radio_sensor.value(param));
      } else if(param == RADIO_SENSOR_LAST_PACKET) {
        snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "{'rssi':%d}", radio_sensor.value(param));
      }
      REST.set_response_payload(response, buffer, strlen((char *)buffer));
    } else {
      REST.set_response_status(response, REST.status.NOT_ACCEPTABLE);
      const char *msg = "Supporting content-types text/plain and application/json";
      REST.set_response_payload(response, msg, strlen(msg));
    }
  } else {
    REST.set_response_status(response, REST.status.BAD_REQUEST);
  }
}
#endif /* PLATFORM_HAS_RADIO */
