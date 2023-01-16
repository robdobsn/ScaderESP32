/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RampGenTimer
//
// Rob Dobson 2016-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <vector>
#include <ArduinoOrAlt.h>
#include "driver/timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// #define USE_SEMAPHORE_FOR_LIST_ACCESS

// This is intended to be used statically in the RampGenerator class
// setup() must be called to initialise and the timer will be started by the
// first RampGenerator object that calls start()

typedef void (*RampGenTimerCB)(void* pObject);

class RampGenTimer
{
public:
    RampGenTimer();
    virtual ~RampGenTimer();
    bool setup(uint32_t timerPeriodUs, 
            timer_group_t timerGroup = TIMER_GROUP_0,
            timer_idx_t timerIdx = TIMER_0);
    void enable(bool en);
    uint32_t getPeriodUs()
    {
        return _timerPeriodUs;
    }
    bool hookTimer(RampGenTimerCB timerCB, void* pObject);
    void unhookTimer(void* pObject);
    String getDebugStr();
    
    // Default ramp generation timer period us
    static constexpr uint32_t rampGenPeriodUs_default = 20;

private:
    static bool _timerIsSetup;
    static bool _timerIsEnabled;

    // ESP32 timer
    static timer_isr_handle_t _rampTimerHandle;
    static constexpr uint32_t RAMP_TIMER_DIVIDER = 80;
    static timer_group_t _timerGroup;
    static timer_idx_t _timerIdx;
    static uint32_t _timerPeriodUs;

    // Hook info for timer callback
    struct TimerCBHook
    {
        RampGenTimerCB timerCB;
        void* pObject;
    };
    // Vector of callback hooks
    static const uint32_t MAX_TIMER_CB_HOOKS = 20;
    static std::vector<TimerCBHook> _timerCBHooks;

#ifdef USE_SEMAPHORE_FOR_LIST_ACCESS
    // Mutex for callback hooks vector
    static SemaphoreHandle_t _hookListMutex;
#endif

    // ISR
    static void IRAM_ATTR _staticISR(void* arg);

    // Debug timer count
    static volatile uint64_t _timerCount;
};
