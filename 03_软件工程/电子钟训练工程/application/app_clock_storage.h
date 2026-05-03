/*
 * Application-level settings serialization.
 * This module decides which clock parameters are persistent and stores them through a driver.
 */
#ifndef APP_CLOCK_STORAGE_H
#define APP_CLOCK_STORAGE_H

#include "app_clock_internal.h"

void AppClockStorage_Init(void);
uint8_t AppClockStorage_Load(ClockContext_t *clock);
void AppClockStorage_Save(const ClockContext_t *clock);

#endif
