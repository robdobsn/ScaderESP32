idf_component_register(
                SRCS 
                  "Logger/LoggerPapertrail.cpp"
                INCLUDE_DIRS
                  "Logger"
                REQUIRES
                  raftcore-src
                )
                