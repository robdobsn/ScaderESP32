/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Main entry point
//
// Scader ESP32 V5 Firmware
// Rob Dobson 2015-24
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RaftCoreApp.h"
#include "RegisterSysMods.h"
#include "RegisterWebServer.h"
#include "DetectHardware.h"
// Scader components
#include "ScaderRelays.h"
#include "ScaderShades.h"
#include "ScaderLocks.h"
#include "ScaderOpener.h"
#include "ScaderLEDPixels.h"
#include "ScaderRFID.h"
#include "ScaderPulseCounter.h"
#include "ScaderElecMeters.h"
#include "ScaderBTHome.h"
// #include "ScaderCat.h"
// #include "ScaderWaterer.h"
#include "MotorControl.h"
#include "DeviceHX711.h"
#include "DeviceMMWave.h"
#include "RaftBusSystem.h"
#include "DeviceFactory.h"
#include "BusBLE.h"

#ifdef FEATURE_INCLUDE_SCADER_TEST_SYS_MOD
#include "ScaderTest.h"
#endif

// Entry point
extern "C" void app_main(void)
{
    RaftCoreApp raftCoreApp;

    // Detect hardware
    DetectHardware::detectHardware(raftCoreApp);

    // Register SysMods from RaftSysMods library
    RegisterSysMods::registerSysMods(raftCoreApp.getSysManager());

    // Register WebServer
    RegisterSysMods::registerWebServer(raftCoreApp.getSysManager(), true);

    // Register BusI2C
    raftBusSystem.registerBus("I2C", BusI2C::createFn);
    
    // Register BusSerial
    raftBusSystem.registerBus("Serial", BusSerial::createFn);

    // Register BusBLE
    raftBusSystem.registerBus("BusBLE", BusBLE::createFn);

    // Register MotorControl
    deviceFactory.registerDevice("MotorControl", MotorControl::create);

    // Register other hardware
    deviceFactory.registerDevice("HX711", DeviceHX711::create);
    deviceFactory.registerDevice("MMWave", DeviceMMWave::create);

    // Scader hardware support
    raftCoreApp.registerSysMod("ScaderRelays", ScaderRelays::create, true);
    raftCoreApp.registerSysMod("ScaderShades", ScaderShades::create, true);
    raftCoreApp.registerSysMod("ScaderOpener", ScaderOpener::create, true);
    raftCoreApp.registerSysMod("ScaderRFID", ScaderRFID::create, true);
    raftCoreApp.registerSysMod("ScaderElecMeters", ScaderElecMeters::create, true);

    // Optional components
    raftCoreApp.registerSysMod("ScaderLocks", ScaderLocks::create, true);
    raftCoreApp.registerSysMod("ScaderLEDPix", ScaderLEDPixels::create, true);
    raftCoreApp.registerSysMod("ScaderPulseCounter", ScaderPulseCounter::create, true);
    raftCoreApp.registerSysMod("ScaderBTHome", ScaderBTHome::create, true);
    // raftCoreApp.registerSysMod("ScaderCat", ScaderCat::create, true);
    // raftCoreApp.registerSysMod("ScaderWaterer", ScaderWaterer::create, true);


#ifdef FEATURE_INCLUDE_SCADER_TEST_SYS_MOD
    // Test component
    raftCoreApp.registerSysMod("ScaderTest", ScaderTest::create, true);
#endif

    // Loop forever
    while (1)
    {
        // Loop the RaftCoreApp
        raftCoreApp.loop();
    }
}
