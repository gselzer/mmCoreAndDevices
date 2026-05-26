// Micro-Manager Device Adapter for Spectra-Physics InSight DS+ Laser System
// Author: Gabriel Selzer
//
// Copyright 2026 Board of Regents of the University of Wisconsin System
//
// This file is distributed under the BSD license. License text is included
// with the source distribution.
//
// This file is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.
//
// IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.

#pragma once

#include "MMDevice.h"
#include "DeviceBase.h"

#include <cstring>

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_PORT_CHANGE_FORBIDDEN    10002

const char* g_DeviceName = "Spectra-Physics Insight DS+";
const char* g_On = "On";
const char* g_Off = "Off";

class SpectraPhysicsInsightDS : public CShutterBase<SpectraPhysicsInsightDS>
{
public:
    SpectraPhysicsInsightDS();
    ~SpectraPhysicsInsightDS();

    // MMDevice API
    int Initialize();
    int Shutdown();
    void GetName(char* name) const;
    bool Busy();

   // Shutter API
   int SetOpen(bool open = true);
   int GetOpen(bool& open);
   int Fire(double deltaT);

   // Actions
   int OnPort(MM::PropertyBase * pProp, MM::ActionType eAct);
   int OnWavelength(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnWarmup(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnPumpLaser(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
    // Device state
    bool initialized_;
    std::string port_;

    int ExecuteCommand(const std::string& cmd);
    int ExecuteCommand(const std::string& cmd, std::string& answer);
    int StatusBit(unsigned int bitNumber, bool& bit);
};


