#pragma once

#include "cy_result.h"
#include <stdbool.h>

void network_service_task(void *param);
cy_rslt_t network_svc_send_data(const char *buffer, int length) ;