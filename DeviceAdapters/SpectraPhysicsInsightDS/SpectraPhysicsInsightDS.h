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

#include <chrono>
#include <cstring>
#include <mutex>

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_PORT_CHANGE_FORBIDDEN       10001
#define ERR_WATCHDOG_CHANGE_FORBIDDEN   10002
#define ERR_PUMP_LASER_NOT_WARM         10003
#define ERR_WAVELENGTH_CHANGING         10004
#define ERR_PUMP_LASER_TURNING_ON       10005

class SpectraPhysicsInsightDS : public CShutterBase<SpectraPhysicsInsightDS>
{
    friend class WatchdogThread;
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

   // Pre-Init Actions
   int OnPort(MM::PropertyBase * pProp, MM::ActionType eAct);
   int OnWatchdog(MM::PropertyBase * pProp, MM::ActionType eAct);
   // Actions
   int OnWavelength(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnPumpLaser(MM::PropertyBase* pProp, MM::ActionType eAct);
   // Read-only Actions
   int OnWarmup(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnHumidity(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnDiode1Current(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnDiode2Current(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnDiode1Temp(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnDiode2Temp(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnPower(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
    // Device state
    bool initialized_;
    std::string port_;
    bool watchdogDisabled_;
    std::string lastCommand_;
    std::chrono::steady_clock::time_point lastCommandTime_;
    std::mutex serialMutex_;
    WatchdogThread* watchdogThread_;

    int ExecuteCommand(const std::string& cmd);
    int ExecuteCommand(const std::string& cmd, std::string& answer);
    int SendCommand(const std::string& cmd);
    int StatusBit(unsigned int bitNumber, bool& bit);
    int LaserState(int& state);
};

class WatchdogThread : public MMDeviceThreadBase
{
public:
    WatchdogThread(SpectraPhysicsInsightDS& device);
    ~WatchdogThread();
    int svc() override;
    void Start();
    void Stop();
private:
    SpectraPhysicsInsightDS& device_;
    std::mutex stopMutex_;
    std::condition_variable timerCV_;
    bool stop_;
    std::chrono::steady_clock::duration interval_;

};

