/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Bus Consts
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

struct BusElemAddrAndStatus
{
    uint16_t address;
    bool isChangeToOnline;
};

enum BusOperationStatus
{
    BUS_OPERATION_UNKNOWN,
    BUS_OPERATION_OK,
    BUS_OPERATION_FAILING
};
