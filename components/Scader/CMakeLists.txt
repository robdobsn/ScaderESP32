# # Set location of base FastLED directory.
# get_filename_component(FASTLED_DIR ${BUILD_DIR}/_deps/fastled-src/src ABSOLUTE)
# message(STATUS "---------------------------- FASTLED_DIR: ${FASTLED_DIR}")

# set(FASTLED_SOURCE_FILES
#     ${FASTLED_DIR}/bitswap.cpp
# )

idf_component_register(
                SRCS 
                  "ScaderCommon/ScaderCommon.cpp"
                  "ScaderRelays/ScaderRelays.cpp"
                  "ScaderRelays/SPIDimmer.cpp"
                  "ScaderShades/ScaderShades.cpp"
                  "ScaderLocks/ScaderLocks.cpp"
                  "ScaderLocks/DoorStrike.cpp"
                  "ScaderElecMeters/ScaderElecMeters.cpp"
                  "ScaderRFID/ScaderRFID.cpp"
                  "ScaderRFID/RFIDModuleBase.cpp"
                  "ScaderRFID/RFIDModule_EccelA1SPI.cpp"
                  "ScaderOpener/ScaderOpener.cpp"
                  "ScaderOpener/OpenerStatus.cpp"
                  "ScaderLEDPixels/ScaderLEDPixels.cpp"
                  "ScaderOpener/DoorOpener.cpp"
                  "ScaderOpener/UIModule.cpp"
                  "ScaderPulseCounter/ScaderPulseCounter.cpp"
                  "ScaderBTHome/ScaderBTHome.cpp"
                  # "ScaderCat/ScaderCat.cpp"
                  # "TinyPICO/TinyPICO.cpp"
                  # "HWElemINA219/HWElemINA219.cpp"
                  # ${FASTLED_SOURCE_FILES}
                  # "ScaderWaterer/ScaderWaterer.cpp"
                  # "ScaderWaterer/MoistureSensors.cpp"
                  # "ScaderWaterer/PumpControl.cpp"
                INCLUDE_DIRS
                  "ScaderCommon"
                  "ScaderRelays"
                  "ScaderShades"
                  "ScaderLocks"
                  "ScaderElecMeters"
                  "ScaderRFID"
                  "ScaderOpener"
                  "ScaderPulseCounter"
                  # "ScaderCat"
                  "ScaderLEDPixels"
                  "ScaderBTHome"
                  # "TinyPICO"
                  # "HWElemINA219"
                  # ${FASTLED_DIR}
                  # "FastLED"
                  # "ScaderWaterer"
                  "ScaderTest"
                REQUIRES
                  RaftCore
                  # FastLED-idf
                  hardware
                )
