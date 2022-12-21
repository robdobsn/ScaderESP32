#ifndef HAL_ESP32_HAL_H_
#define HAL_ESP32_HAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include "ArduinoTime.h"
#include "ArduinoGPIO.h"

#ifndef F_CPU
#define F_CPU (CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ * 1000000U)
#endif

void yield(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_ESP32_HAL_H_ */
