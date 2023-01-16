/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware Elem Formats
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <stdint.h>

// The formats declared here are intended to help future-proof the separation between hardware elements
// and objects wanting to communicate with them. To add a binary communication format a diffent format code
// should be added and each end coded to communicated with a set binary format which then doesn't change.

// Smart servo message formats
static const uint32_t SMART_SERVO_CMD_BINARY_FORMAT_1 = 0;
static const uint32_t SMART_SERVO_OPCODE_POS = 0;
static const uint32_t SMART_SERVO_SET_KEYP_OPCODE = 0;
static const uint32_t SMART_SERVO_APPEND_KEYP_OPCODE = 1;
static const uint32_t SMART_SERVO_INST_MOVE_OPCODE = 5;
static const uint32_t SMART_SERVO_FLAGS_OPCODE = 0x20;
static const uint32_t SMART_SERVO_SET_INTERN_PARAM_OPCODE = 0xF1;   // deprecated
static const uint32_t SMART_SERVO_SET_PARAM_OPCODE = 0xFF;
static const uint32_t SMART_SERVO_FLASH_WRITE_OPCODE = 0x40;
static const uint32_t SMART_SERVO_KEYP_COUNT_POS = 1;
static const uint32_t SMART_SERVO_KEYP_START_POS = 2;
static const uint32_t SMART_SERVO_KEYP_BLOCK_SIZE = 4;
static const uint32_t SMART_SERVO_MAX_KEYPOINTS = 7;
static const uint32_t SMART_SERVO_FLAG_ENABLE_MASK = 0x01;
static const uint32_t SMART_SERVO_FLAG_CLEARQUEUE_MASK = 0x02;
static const uint32_t SMART_SERVO_FLAG_CLEARMOVE_MASK = 0x04;
static const uint32_t SMART_SERVO_FLAG_FINISHMOVE_MASK = 0x08;
static const uint32_t SMART_SERVO_FLAG_FINISHQUEUE_MASK = 0x10;
static const uint32_t SMART_SERVO_FLAG_PAUSE_MASK = 0x20;
static const uint32_t SMART_SERVO_SET_PARAM_CALIBRATE = 0x0D;
static const uint32_t SMART_SERVO_FLASH_ADDR_P_GAIN = 0x4014;
static const uint32_t SMART_SERVO_FLASH_ADDR_I_GAIN = 0x4016;
static const uint32_t SMART_SERVO_FLASH_ADDR_D_GAIN = 0x4018;
static const uint32_t SMART_SERVO_FLASH_ADDR_CURR_LIM_INST = 0x401A;
static const uint32_t SMART_SERVO_FLASH_ADDR_CURR_LIM_SUS = 0x401C;
static const uint32_t SMART_SERVO_FLASH_ADDR_POLARITY = 0x401E;
static const uint32_t SMART_SERVO_FLASH_ADDR_DIRECTION = 0x401F;
static const uint32_t SMART_SERVO_FLASH_ADDR_CALIBRATED_ZERO = 0x4020;
static const uint32_t SMART_SERVO_FLASH_ADDR_MULT_TORQUE = 0x4022;
static const uint32_t SMART_SERVO_FLASH_ADDR_MULT_POS = 0x4024;
static const uint32_t SMART_SERVO_FLASH_ADDR_BUZZ_RED = 0x4026;
static const uint32_t SMART_SERVO_FLASH_ADDR_CURRENT_BUZZ = 0x4027;
static const uint32_t SMART_SERVO_FLASH_ADDR_POS_MIN = 0x4029;
static const uint32_t SMART_SERVO_FLASH_ADDR_POS_MAX = 0x4031;
static const uint32_t SMART_SERVO_FLASH_ADDR_BRAKING = 0x4033;
static const uint32_t SMART_SERVO_FLASH_ADDR_DIRECTION_FLIP = 0x4034;
static const uint32_t SMART_SERVO_STATUS_EN_MASK = 0x01;
static const uint32_t SMART_SERVO_STATUS_CURR_LIM_INST_MASK = 0x02;
static const uint32_t SMART_SERVO_STATUS_CURR_LIM_SUST_MASK = 0x04;
static const uint32_t SMART_SERVO_STATUS_BUSY_MASK = 0x08;
static const uint32_t SMART_SERVO_STATUS_POS_RESTR_MASK = 0x10;
static const uint32_t SMART_SERVO_STATUS_PAUSED_MASK = 0x20;
static const uint32_t SMART_SERVO_STATUS_COMM_MASK = 0x80;

// This format code is used for the ROSSERIAL smart_servo format including the ID which can be left as 0
static const uint32_t SMART_SERVO_VALS_FORMAT_ROSSERIAL = 0;

// Audio Out formats
static const uint32_t AUDIO_OUT_CMD_JSON_FORMAT_1 = 0;
static const uint32_t AUDIO_OUT_FILE_BLOCK_FORMAT_1 = 1;

// IMU formats
static const uint32_t IMU_VALS_BINARY_FORMAT_ROSSERIAL = 0;

// This format code is used for the ROSSERIAL RSAddOn format including the ID which can be left as 0
static const uint32_t ADD_ON_STATUS_FORMAT_ROSSERIAL = 0;

