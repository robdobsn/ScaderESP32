idf_component_register(
                SRCS
                  "Helpers/SampleCollector.cpp"
                  "MotorMechanism/MotorMechanism.cpp"
                INCLUDE_DIRS
                  "HX711"
                  "MMWave"
                  "MotorMechanism"
                  "Helpers"
                REQUIRES
                  RaftCore
                  RaftI2C
                  RaftMotorControl
                )
