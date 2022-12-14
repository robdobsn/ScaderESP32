/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ROSTopicManager
// Some content from Marty V1 rostopics.hpp
//
// Sandy Enoch and Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <vector>
#include <ProtocolBase.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// New ROSTOPICS for V2
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ROSTOPIC_V2_SMART_SERVOS    120
#define ROSTOPIC_V2_ACCEL           121
#define ROSTOPIC_V2_POWER_STATUS    122
#define ROSTOPIC_V2_ADDONS          123
#define ROSTOPIC_V2_ROBOT_STATUS    124

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// V2 ROSTOPIC SMART_SERVOS message layout
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ROS_SMART_SERVOS_MAX_NUM_SERVOS 15
#define ROS_SMART_SERVOS_ATTR_GROUP_BYTES 6
#define ROS_SMART_SERVOS_ATTR_ID_IDX 0
#define ROS_SMART_SERVOS_ATTR_SERVO_POS_IDX 1
#define ROS_SMART_SERVOS_ATTR_MOTOR_CURRENT_IDX 3
#define ROS_SMART_SERVOS_ATTR_STATUS_IDX 5
#define ROS_SMART_SERVOS_INVALID_POS -32768
#define ROS_SMART_SERVOS_INVALID_CURRENT -32768

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// V2 ROSTOPIC ACCEL message layout
//
// float32 x
// float32 y
// float32 z
// uint8_t IDNo
// uint8_t Bit 0 == freefall flag
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ROS_ACCEL_BYTES 14
#define ROS_ACCEL_POS_X 0
#define ROS_ACCEL_POS_Y 4
#define ROS_ACCEL_POS_Z 8
#define ROS_ACCEL_POS_IDNO 12
#define ROS_ACCEL_POS_FLAGS 13

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// V2 ROSTOPIC POWER STATUS message layout
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ROS_POWER_STATUS_BYTES 13
#define ROS_POWER_STATUS_REMCAPPC 0
#define ROS_POWER_STATUS_BATTTEMPC 1
#define ROS_POWER_STATUS_REMCAPMAH 2
#define ROS_POWER_STATUS_FULLCAPMAH 4
#define ROS_POWER_STATUS_CURRENTMA 6
#define ROS_POWER_STATUS_5V_ON_SECS 8
#define ROS_POWER_STATUS_POWERFLAGS 10
#define ROS_POWER_STATUS_FLAGBIT_USB_POWER 0
#define ROS_POWER_STATUS_FLAGBIT_5V_ON 1
#define ROS_POWER_STATUS_FLAGBIT_BATT_INFO_INVALID 2
#define ROS_POWER_STATUS_FLAGBIT_USB_POWER_INFO_INVALID 3
#define ROS_POWER_STATUS_IDNO 12

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// V2 ROSTOPIC ROBOT STATUS message layout
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ROS_ROBOT_STATUS_BYTES 26
#define ROS_ROBOT_STATUS_MOTION_FLAGS 0
#define ROS_ROBOT_STATUS_IS_MOVING_MASK 0x01
#define ROS_ROBOT_STATUS_IS_PAUSED_MASK 0x02
#define ROS_ROBOT_STATUS_FW_UPDATE_MASK 0x04
#define ROS_ROBOT_STATUS_QUEUED_WORK_COUNT 1
#define ROS_ROBOT_STATUS_HEAP_FREE_POS 2
#define ROS_ROBOT_STATUS_HEAP_FREE_BYTES 4
#define ROS_ROBOT_STATUS_HEAP_MIN_POS 6
#define ROS_ROBOT_STATUS_HEAP_MIN_BYTES 4
#define ROS_ROBOT_STATUS_INDICATORS_POS 10
#define ROS_ROBOT_STATUS_INDICATOR_BYTES 4
#define ROS_ROBOT_STATUS_INDICATORS_NUM 3
#define ROS_ROBOT_STATUS_SYSMOD_LOOPMS_AVG_POS 22
#define ROS_ROBOT_STATUS_SYSMOD_LOOPMS_AVG_SIZE 1
#define ROS_ROBOT_STATUS_SYSMOD_LOOPMS_MAX_POS 23
#define ROS_ROBOT_STATUS_SYSMOD_LOOPMS_MAX_SIZE 1
#define ROS_ROBOT_STATUS_WIFI_RSSI_POS 24
#define ROS_ROBOT_STATUS_WIFI_RSSI_SIZE 1
#define ROS_ROBOT_STATUS_BLE_RSSI_POS 25
#define ROS_ROBOT_STATUS_BLE_RSSI_SIZE 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// V2 ROSTOPIC ADDONS message layout
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ROS_ADDONS_MAX_NUM_ADDONS 15
#define ROS_ADDON_GROUP_BYTES 12
#define ROS_ADDON_IDNO_POS 0
#define ROS_ADDON_FLAGS_POS 1
#define ROS_ADDON_DATAVALID_FLAG_MASK 0x80
#define ROS_ADDON_STATUS_POS 2
#define ROS_ADDON_STATUS_BYTES 10
