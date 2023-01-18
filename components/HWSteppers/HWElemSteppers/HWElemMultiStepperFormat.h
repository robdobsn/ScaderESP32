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
// and objects wanting to communicate with them. To add a binary communication format a diffent format, code
// should be added and each end written to communicate with a set binary format which then doesn't change.

// Format for MultiStepper
static const uint32_t MULTISTEPPER_CMD_BINARY_FORMAT_1 = 0;
static const uint32_t MULTISTEPPER_MOTION_ARGS_BINARY_FORMAT_1 = 0;

static const uint32_t MULTISTEPPER_OPCODE_POS = 0;
static const uint32_t MULTISTEPPER_MOVETO_OPCODE = 0;
static const uint32_t MULTISTEPPER_MOVETO_BINARY_FORMAT_POS = 0;
static const uint32_t MULTISTEPPER_MOVETO_AXES_COUNT_POS = 1;
static const uint32_t MULTISTEPPER_MOVETO_AXES_START_POS = 2;
static const uint32_t MULTISTEPPER_MOVETO_AXES_BLOCK_SIZE = 4;
static const uint32_t MULTISTEPPER_MAX_AXES = 5;
