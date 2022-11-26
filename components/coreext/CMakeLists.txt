idf_component_register(
                SRCS 
                  "Config/ConfigNVS.cpp"
                  "Logger/LoggerPapertrail.cpp"
                  "ArPreferences/ArPreferences.cpp"
                INCLUDE_DIRS
                  "ArPreferences"
                  "Config"
                  "Logger"
                REQUIRES
                  nvs_flash
                  raftcore-src
                )
                