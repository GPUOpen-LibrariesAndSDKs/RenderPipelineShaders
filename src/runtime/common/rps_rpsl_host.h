// Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the AMD INTERNAL EVALUATION LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_RPSL_HOST_H
#define RPS_RPSL_HOST_H

/// @brief Bitflags for the type of entry calls.
enum RpslEntryCallFlagBits
{
    RPSL_ENTRY_CALL_DEFAULT    = 0,         ///< Default entry call.
    RPSL_ENTRY_CALL_SUBPROGRAM = 1 << 0,    ///< The current entry call is used to execute a subprogram for a node in a
                                            ///  parent subprogram.
};

#endif  //RPS_RPSL_HOST_H
