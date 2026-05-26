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
// MyNewDevice.cpp: A minimal device adapter

#include "SpectraPhysicsInsightDS.h"

#include "ModuleInterface.h"

#include <cstring>

MODULE_API void InitializeModuleData()
{
    RegisterDevice(g_DeviceName, MM::ShutterDevice, "TODO");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
    if (deviceName == 0)
        return 0;

    if (std::strcmp(deviceName, g_DeviceName) == 0)
    {
        return new SpectraPhysicsInsightDS();
    }
    return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
    delete pDevice;
}

// Implementation of MyNewDevice methods
SpectraPhysicsInsightDS::SpectraPhysicsInsightDS() :
    port_("Undefined"),
    initialized_(false)
{
    InitializeDefaultErrorMessages();
    SetErrorText(ERR_PORT_CHANGE_FORBIDDEN, "Cannot change port after initialization");

	CPropertyAction* pActPort = new CPropertyAction (this, &SpectraPhysicsInsightDS::OnPort);
	CreateStringProperty(MM::g_Keyword_Port, "Undefined", false, pActPort, true);

}

SpectraPhysicsInsightDS::~SpectraPhysicsInsightDS()
{
    Shutdown();
}

void SpectraPhysicsInsightDS::GetName(char* name) const
{
    CDeviceUtils::CopyLimitedString(name, g_DeviceName);
}

int SpectraPhysicsInsightDS::Initialize()
{
    int ret;
    // Setup watchdog timer to 3 seconds (timeout recommended by manual)
    ret = ExecuteCommand("TIM:WATC 3");

    // Configure wavelength property
	CPropertyAction* pActWavelength = new CPropertyAction(this, &SpectraPhysicsInsightDS::OnWavelength);
    ret = CreateIntegerProperty("Wavelength", 800, false, pActWavelength, false);
    if (ret != 0)
        return ret;
    std::string wave_min, wave_max;
    ret = ExecuteCommand("WAV:min?", wave_min);
    if (ret != 0)
        return ret;
    ret = ExecuteCommand("WAV:max?", wave_max);
    if (ret != 0)
        return ret;
    ret = SetPropertyLimits("Wavelength", stoi(wave_min), stoi(wave_max));
    if (ret != 0)
        return ret;
	CPropertyAction* pActWavelength = new CPropertyAction(this, &SpectraPhysicsInsightDS::OnWavelength);
    ret = CreateIntegerProperty("Wavelength", 800, false, pActWavelength, false);
    if (ret != 0)
        return ret;

    // Configure warmup percentage property (read-only)
	CPropertyAction* pActWarmup = new CPropertyAction(this, &SpectraPhysicsInsightDS::OnWarmup);
    ret = CreateIntegerProperty("Warmup Percentage", 0, true, pActWarmup, false);
    if (ret != 0)
        return ret;

    // Configure pump laser property
	CPropertyAction* pActPumpLaser = new CPropertyAction(this, &SpectraPhysicsInsightDS::OnPumpLaser);
    ret = CreateStringProperty("Pump Laser", g_Off, false, pActPumpLaser, false);
    if (ret != 0)
        return ret;
    ret = AddAllowedValue("Pump Laser", g_Off);
    if (ret != 0)
        return ret;
    ret = AddAllowedValue("Pump Laser", g_On);
    if (ret != 0)
        return ret;
    


    initialized_ = true;
    return DEVICE_OK;
}

int SpectraPhysicsInsightDS::Shutdown()
{
    if (initialized_)
    {
        SetOpen(false);

        initialized_ = false;
    }
    return DEVICE_OK;
}

bool SpectraPhysicsInsightDS::Busy()
{
    return false;
}

int SpectraPhysicsInsightDS::SetOpen(bool open = true)
{
    if (open)
        return ExecuteCommand("SHUT 1");
    else
        return ExecuteCommand("SHUT 0");
}

int SpectraPhysicsInsightDS::GetOpen(bool& open)
{
    // Bit 2 identifies the main shutter, 1 means it is open
    return StatusBit(2, open);
}

int SpectraPhysicsInsightDS::Fire(double deltaT)
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

int SpectraPhysicsInsightDS::OnPort(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(port_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
			// revert
			pProp->Set(port_.c_str());
			return ERR_PORT_CHANGE_FORBIDDEN;
		}
		pProp->Get(port_);
	}

	return DEVICE_OK;
}

int SpectraPhysicsInsightDS::OnWavelength(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
        std::string wavelength;
        int ret = ExecuteCommand("READ:WAV?", wavelength);
        if (ret != 0)
            return ret;
		pProp->Set(wavelength.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
        std::string cmd;
        pProp->Get(cmd);
        cmd = "WAV " + cmd;
        // TODO Consider the WAV? command for reading the result?
        return ExecuteCommand(cmd);
	}

	return DEVICE_OK;
}

int SpectraPhysicsInsightDS::OnWarmup(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
        std::string warmupPct;
        int ret = ExecuteCommand("READ:PCTW?", warmupPct);
        if (ret != 0)
            return ret;
		pProp->Set(warmupPct.c_str());
	}
	return DEVICE_OK;
}

int SpectraPhysicsInsightDS::OnPumpLaser(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
        std::string warmupPct;
        int ret = ExecuteCommand("READ:PCTW?", warmupPct);
        if (ret != 0)
            return ret;
		pProp->Set(warmupPct.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
        std::string cmd;
        pProp->Get(cmd);
        if (cmd == g_On)
            return ExecuteCommand("ON");
        else
            return ExecuteCommand("OFF");
	}

	return DEVICE_OK;
}

/**
 * This function not only sends the command but also reads back an answer from the device
 */
int SpectraPhysicsInsightDS::ExecuteCommand(const std::string& cmd, std::string& answer)
{
   // send command
	int ret = ExecuteCommand(cmd);
	if (ret != DEVICE_OK)
		return ret;

	// Get answer from the device.  It will always end with a line feed
	return GetSerialAnswer(port_.c_str(), "\n", answer);
}

/**
 * This function sends the command to the Spectra InSight
 */
int SpectraPhysicsInsightDS::ExecuteCommand(const std::string& cmd)
{
    // Clean COM port
    int ret = PurgeComPort(port_.c_str());
    if (ret != DEVICE_OK)
        return ret;

	// Send command
    return SendSerialCommand(port_.c_str(), cmd.c_str(), "\r");
}

int SpectraPhysicsInsightDS::StatusBit(unsigned int bitNumber, bool& bit)
{
    // Returns an integer value that corresponds to a 32-bit binary number
    //
    // TODO It is apparently normal for this command to return an incorrect status for
    // 1 second after SetOpen(true)
    //
    // TODO We could be misinterpreting the return of the command - ReadFromComPort could be more appropriate
    std::string status_int;
    int ret = ExecuteCommand("*STB?", status_int);
    if (ret != 0)
        return ret;

    unsigned int status_byte = stoul(status_int);
    unsigned int bit_mask = 1u << bitNumber;
    bit = (status_byte & bit_mask) != 0;
    return DEVICE_OK;
}
