/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RampGenTimer
//
// Rob Dobson 2016-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RampGenTimer.h"
#include "Logger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/timer.h"

// #define USE_ESP_IDF_NEW_TIMER_FUNCTIONS

static const char* MODULE_PREFIX = "RampGenTimer";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Statics
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Flag indicating timer setup
bool RampGenTimer::_timerIsSetup = false;
bool RampGenTimer::_timerIsEnabled = false;

// ESP32 timer
timer_isr_handle_t RampGenTimer::_rampTimerHandle = NULL;
timer_group_t RampGenTimer::_timerGroup = TIMER_GROUP_0;
timer_idx_t RampGenTimer::_timerIdx = TIMER_0;
uint32_t RampGenTimer::_timerPeriodUs = RampGenTimer::rampGenPeriodUs_default;

// Vector of callback hooks
std::vector<RampGenTimer::TimerCBHook> RampGenTimer::_timerCBHooks;

#ifdef USE_SEMAPHORE_FOR_LIST_ACCESS
// Mutex for callback hooks vector
SemaphoreHandle_t RampGenTimer::_hookListMutex = NULL;
#endif

// Debug timer count
volatile uint64_t RampGenTimer::_timerCount = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RampGenTimer::RampGenTimer()
{
}

RampGenTimer::~RampGenTimer()
{
    if (_rampTimerHandle)
        esp_intr_free(_rampTimerHandle);
#ifdef USE_SEMAPHORE_FOR_LIST_ACCESS
    if (_hookListMutex)
		vSemaphoreDelete(_hookListMutex);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RampGenTimer::setup(uint32_t timerPeriodUs, 
            timer_group_t timerGroup,
            timer_idx_t timerIdx)
{
    // Check already setup
    if (_timerIsSetup)
        return true;
    _timerGroup = timerGroup;
    _timerIdx = timerIdx;
    _timerPeriodUs = timerPeriodUs;
    
#ifdef USE_SEMAPHORE_FOR_LIST_ACCESS
    // Mutex controlling hook vector access
    _hookListMutex = xSemaphoreCreateMutex();
#endif

    // Setup timer
    timer_config_t timerConfig = {
        .alarm_en = TIMER_ALARM_EN,
        .counter_en = TIMER_PAUSE,
        .intr_type = TIMER_INTR_LEVEL,
        .counter_dir = TIMER_COUNT_UP,
        .auto_reload = TIMER_AUTORELOAD_EN,
        .divider = 80   /* 1 us per tick */
    };
    if (timer_init(timerGroup, timerIdx, &timerConfig) == ESP_OK)
    {
        timer_set_counter_value(timerGroup, timerIdx, 0);
        timer_set_alarm_value(timerGroup, timerIdx, timerPeriodUs);
        timer_enable_intr(timerGroup, timerIdx);
        timer_isr_register(timerGroup, timerIdx, &_staticISR, 
                    NULL, 0, &_rampTimerHandle);
        _timerIsSetup = true;
        _timerIsEnabled = false;
        LOG_I(MODULE_PREFIX, "Started ISR timer for direct stepping");
        return true;
    }
    else
    {
        LOG_E(MODULE_PREFIX, "Failed to start ISR timer for direct stepping");            
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Enable
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RampGenTimer::enable(bool en)
{
    if (en)
    {
        if (_timerIsSetup)
            timer_start(_timerGroup, _timerIdx);
    }
    else
    {
        if (_timerIsSetup)
            timer_pause(_timerGroup, _timerIdx);
    }
    _timerIsEnabled = en;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Hook / unhook
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RampGenTimer::hookTimer(RampGenTimerCB timerCB, void* pObject)
{
    // Check for max hooks
    if (_timerCBHooks.size() > MAX_TIMER_CB_HOOKS)
        return false;

#ifdef USE_SEMAPHORE_FOR_LIST_ACCESS
    // Get semaphore on hooks vector
	if (xSemaphoreTake(_hookListMutex, pdMS_TO_TICKS(1)) != pdTRUE)
		return false;
#else
    // Stop timer ISR
    timer_disable_intr(_timerGroup, _timerIdx);
#endif

    // Add new hook
    TimerCBHook newHook = { timerCB, pObject };
    _timerCBHooks.push_back(newHook);
    
#ifdef USE_SEMAPHORE_FOR_LIST_ACCESS
	// Release semaphore
	xSemaphoreGive(_hookListMutex);
#else
    // Re-enable ISR if setup
    if (_timerIsSetup)
        timer_enable_intr(_timerGroup, _timerIdx);
#endif
    return true;
}

void RampGenTimer::unhookTimer(void* pObject)
{
#ifdef USE_SEMAPHORE_FOR_LIST_ACCESS
    // Get semaphore on hooks vector
	if (xSemaphoreTake(_hookListMutex, pdMS_TO_TICKS(1)) != pdTRUE)
		return;
#else
    // Stop timer ISR
    timer_disable_intr(_timerGroup, _timerIdx);
#endif

    // Check for hook to remove
    for (auto it = _timerCBHooks.begin(); it != _timerCBHooks.end(); it++)
    {
        if (it->pObject == pObject)
        {
            _timerCBHooks.erase(it);
        }
        break;
    }
    
#ifdef USE_SEMAPHORE_FOR_LIST_ACCESS
	// Release semaphore
	xSemaphoreGive(_hookListMutex);
    return true;
#else
    // Restart timer ISR if setup
    if (_timerIsSetup)
        timer_enable_intr(_timerGroup, _timerIdx);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ISR
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR RampGenTimer::_staticISR(void* arg)
{
    // Handle ISR by calling each hooked callback
    _timerCount++;
    
#ifdef USE_SEMAPHORE_FOR_LIST_ACCESS
    // Get semaphore on hooks vector
    BaseType_t xTaskWokenBySemphoreTake = pdFALSE;
    BaseType_t xTaskWokenBySemphoreGive = pdFALSE;
	if (xSemaphoreTakeFromISR(_hookListMutex, &xTaskWokenBySemphoreTake) != pdTRUE)
		return;
#endif

    // Call each function
    for (auto& hook : _timerCBHooks)
    {
        if (hook.timerCB)
        {
            hook.timerCB(hook.pObject);
        }
    }

#ifdef USE_SEMAPHORE_FOR_LIST_ACCESS
    // Release semaphore
	xSemaphoreGiveFromISR(_hookListMutex, &xTaskWokenBySemphoreGive);

    // Check if a task was woken
    // See notes on this here: https://www.freertos.org/xSemaphoreTakeFromISR.html
    if ((xTaskWokenBySemphoreTake != pdFALSE) || ((xTaskWokenBySemphoreGive != pdFALSE)))
    {
        taskYIELD ();
    }
#endif

#ifndef USE_ESP_IDF_NEW_TIMER_FUNCTIONS
    if (_timerGroup == 0)
    {
        if (_timerIdx == 0)
        {
            TIMERG0.int_clr_timers.t0 = 1;
            TIMERG0.hw_timer[0].config.alarm_en = 1;
        }
        else
        {
            TIMERG0.int_clr_timers.t1 = 1;
            TIMERG0.hw_timer[1].config.alarm_en = 1;
        }
    }
    else
    {
        if (_timerIdx == 0)
        {
            TIMERG1.int_clr_timers.t0 = 1;
            TIMERG1.hw_timer[0].config.alarm_en = 1;
        }
        else
        {
            TIMERG1.int_clr_timers.t1 = 1;
            TIMERG1.hw_timer[1].config.alarm_en = 1;
        }
    }    
#else
    timer_spinlock_take(_timerGroup);
    timer_group_clr_intr_status_in_isr(_timerGroup, _timerIdx);    
    timer_group_enable_alarm_in_isr(_timerGroup, _timerIdx);
    timer_spinlock_give(_timerGroup);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String RampGenTimer::getDebugStr()
{
    char tmpStr[50];
    snprintf(tmpStr, sizeof(tmpStr), "%lld", _timerCount);
    return tmpStr;
}
