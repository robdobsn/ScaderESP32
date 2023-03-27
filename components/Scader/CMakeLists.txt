# # Set location of base FastLED directory.
# get_filename_component(FASTLED_DIR ${BUILD_DIR}/_deps/fastled-src/src ABSOLUTE)
# message(STATUS "---------------------------- FASTLED_DIR: ${FASTLED_DIR}")

# set(FASTLED_SOURCE_FILES
#     ${FASTLED_DIR}/bitswap.cpp
# )

idf_component_register(
                SRCS 
                  # "ScaderCommon/ScaderCommon.cpp"
                  "ScaderRelays/ScaderRelays.cpp"
                  "ScaderShades/ScaderShades.cpp"
                  "ScaderDoors/ScaderDoors.cpp"
                  "ScaderDoors/DoorStrike.cpp"
                  "ScaderRFID/ScaderRFID.cpp"
                  "ScaderRFID/RFIDModuleBase.cpp"
                  "ScaderRFID/RFIDModule_EccelA1SPI.cpp"
                  "ScaderOpener/ScaderOpener.cpp"
                  "ScaderLEDPixels/ScaderLEDPixels.cpp"
                  "ScaderOpener/DoorOpener.cpp"
                  "ScaderOpener/UIModule.cpp"
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
                  "ScaderDoors"
                  "ScaderRFID"
                  "ScaderOpener"
                  # "ScaderCat"
                  "ScaderLEDPixels"
                  # "TinyPICO"
                  # "HWElemINA219"
                  # ${FASTLED_DIR}
                  # "FastLED"
                  # "ScaderWaterer"
                REQUIRES
                  raftcore-src
                  comms
                  system
                  FastLED-idf
                  HWSteppers
                  hardware
                  rafti2c-src
                )

