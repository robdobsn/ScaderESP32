# Set source files
set(SCADER_SRCS
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
  "ScaderMarbleRun/ScaderMarbleRun.cpp"
  # "ScaderCat/ScaderCat.cpp"
  # "TinyPICO/TinyPICO.cpp"
  # "HWElemINA219/HWElemINA219.cpp"
  # "ScaderWaterer/ScaderWaterer.cpp"
  # "ScaderWaterer/MoistureSensors.cpp"
  # "ScaderWaterer/PumpControl.cpp"
)

# Set include directories
set(SCADER_INCLUDE_DIRS
  "ScaderCommon"
  "ScaderRelays"
  "ScaderShades"
  "ScaderLocks"
  "ScaderElecMeters"
  "ScaderRFID"
  "ScaderOpener"
  "ScaderPulseCounter"
  "ScaderLEDPixels"
  "ScaderBTHome"
  "ScaderMarbleRun"
  "ScaderTest"
  # "ScaderCat"
  # "TinyPICO"
  # "HWElemINA219"
  # "ScaderWaterer"
)

# Set requirements
set(SCADER_REQUIRES RaftCore hardware)

idf_component_register(
  SRCS ${SCADER_SRCS}
  INCLUDE_DIRS ${SCADER_INCLUDE_DIRS}
  REQUIRES ${SCADER_REQUIRES}
)
