// Copyright 2019 SoloKeys Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT>,
// at your option. This file may not be copied, modified, or distributed except
// according to those terms.
//
// STM32F411 has no touch sensing controller (TSC); the Solo capacitive
// touch button does not exist on this port.  These stubs keep the device.c /
// fido2 call sites unchanged: tsc_sensor_exists() always reports "absent",
// so the board falls back to the physical KEY button on PA0.
#include "sense.h"
#include "device.h"
#include "log.h"

void tsc_init(void)
{
}

void tsc_set_electrode(uint32_t channel_ids)
{
    (void) channel_ids;
}

void tsc_start_acq(void)
{
}

void tsc_wait_on_acq(void)
{
}

uint32_t tsc_read(uint32_t indx)
{
    (void) indx;
    return 0;
}

uint32_t tsc_read_button(uint32_t index)
{
    (void) index;
    return 0;
}

int tsc_sensor_exists(void)
{
    return 0;
}
