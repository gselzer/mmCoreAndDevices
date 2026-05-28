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

#include <chrono>
#include <cstring>

const char* g_DeviceName = "Spectra-Physics Insight DS+";
const char* g_On = "On";
const char* g_Off = "Off";
const char* g_Yes = "Yes";
const char* g_No = "No";

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
    watchdogDisabled_(false),
    initialized_(false),
    watchdogThread_(new WatchdogThread(*this))
{
    InitializeDefaultErrorMessages();
    SetErrorText(ERR_PORT_CHANGE_FORBIDDEN, "Cannot change port after initialization");
    // This is really more of a shouldn't than a can't...
    SetErrorText(ERR_WATCHDOG_CHANGE_FORBIDDEN, "Cannot toggle watchdog timer after initialization");

    // COM port property
	CPropertyAction* pActPort = new CPropertyAction (this, &SpectraPhysicsInsightDS::OnPort);
	CreateStringProperty(MM::g_Keyword_Port, "Undefined", false, pActPort, true);

    // Watchdog disable property - shouldn't be used very often
    std::string watchdogProp = "DISABLE WATCHDOG TIMER";
	CPropertyAction* pActWatchdog = new CPropertyAction (this, &SpectraPhysicsInsightDS::OnWatchdog);
	CreateStringProperty(watchdogProp.c_str(), g_No, false, pActWatchdog, true);
    AddAllowedValue(watchdogProp.c_str(), g_No);
    AddAllowedValue(watchdogProp.c_str(), g_Yes);
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
    int ret{};

    // Clean COM port
    ret = PurgeComPort(port_.c_str());
    if (ret != DEVICE_OK)
        return ret;

    // Sanity check the device
    std::string id{};
    ret = ExecuteCommand("*IDN?", id);
    if (ret != 0)
        return ret;
    if (id.rfind("Spectra-Physics, InSight DeepSee", 0) != 0) {
        // *Waves hand* this is not the device you're looking for...
        return DEVICE_NOT_CONNECTED;
    }

    if (watchdogDisabled_) {
        // Disable watchdog by setting timeout to 0 seconds
		ret = ExecuteCommand("TIM:WATC 0");
		if (ret != 0)
			return ret;
    }
    else {
		// Setup watchdog timer to 3 seconds (timeout recommended by manual)
		ret = ExecuteCommand("TIM:WATC 3");
		if (ret != 0)
			return ret;
        // Then start the watchdog thread
        watchdogThread_->Start();
    }


    // Configure wavelength property
    std::string wave_min{}, wave_max{};
    ret = ExecuteCommand("WAV:min?", wave_min);
    if (ret != 0)
        return ret;
    ret = ExecuteCommand("WAV:max?", wave_max);
    if (ret != 0)
        return ret;
	CPropertyAction* pActWavelength = new CPropertyAction(this, &SpectraPhysicsInsightDS::OnWavelength);
    ret = CreateIntegerProperty("Wavelength", 800, false, pActWavelength);
    if (ret != 0)
        return ret;
    ret = SetPropertyLimits("Wavelength", stoi(wave_min), stoi(wave_max));
    if (ret != 0)
        return ret;

    // Configure pump laser property
    // TODO Should this be a property? What happens when you set it to OFF? Maybe ask Jenu
	CPropertyAction* pActPumpLaser = new CPropertyAction(this, &SpectraPhysicsInsightDS::OnPumpLaser);
    ret = CreateStringProperty("Pump Laser", g_Off, false, pActPumpLaser);
    if (ret != 0)
        return ret;
    ret = AddAllowedValue("Pump Laser", g_Off);
    if (ret != 0)
        return ret;
    ret = AddAllowedValue("Pump Laser", g_On);
    if (ret != 0)
        return ret;

    // Configure humidity property (read-only)
	CPropertyAction* pActHumidity = new CPropertyAction(this, &SpectraPhysicsInsightDS::OnHumidity);
    ret = CreateIntegerProperty("RelativeHumidity", 0, true, pActHumidity);
    if (ret != 0)
        return ret;

    // Configure warmup percentage property (read-only)
	CPropertyAction* pActWarmup = new CPropertyAction(this, &SpectraPhysicsInsightDS::OnWarmup);
    ret = CreateIntegerProperty("Warmup Percentage", 0, true, pActWarmup);
    if (ret != 0)
        return ret;

    // Configure diode1 current property (read-only)
	CPropertyAction* pActDiode1Current = new CPropertyAction(this, &SpectraPhysicsInsightDS::OnDiode1Current);
    ret = CreateFloatProperty("Diode 1 Current", 0, true, pActDiode1Current);
    if (ret != 0)
        return ret;

    // Configure diode2 current property (read-only)
	CPropertyAction* pActDiode2Current = new CPropertyAction(this, &SpectraPhysicsInsightDS::OnDiode2Current);
    ret = CreateFloatProperty("Diode 2 Current", 0, true, pActDiode2Current);
    if (ret != 0)
        return ret;

    // Configure diode1 temperature property (read-only)
	CPropertyAction* pActDiode1Temp = new CPropertyAction(this, &SpectraPhysicsInsightDS::OnDiode1Temp);
    ret = CreateFloatProperty("Diode 1 Temperature (Celsius)", 0, true, pActDiode1Temp);
    if (ret != 0)
        return ret;

    // Configure diode2 temperature property (read-only)
	CPropertyAction* pActDiode2Temp = new CPropertyAction(this, &SpectraPhysicsInsightDS::OnDiode2Temp);
    ret = CreateFloatProperty("Diode 2 Temperature (Celsius)", 0, true, pActDiode2Temp);
    if (ret != 0)
        return ret;

    // Configure output power property (read-only)
	CPropertyAction* pActPower = new CPropertyAction(this, &SpectraPhysicsInsightDS::OnPower);
    ret = CreateFloatProperty("Laser Power (Watts)", 0, true, pActPower);
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
        if (watchdogThread_)
        {
            watchdogThread_->Stop();
            delete watchdogThread_;
            watchdogThread_ = nullptr;
        }
        initialized_ = false;
    }
    return DEVICE_OK;
}

bool SpectraPhysicsInsightDS::Busy()
{
    if (lastCommand_ == "ON")
    {
        int state{};
        int ret = LaserState(state);
        if (ret != 0)
            return ret;
        return state == 50;
       
        //std::chrono::duration elapsed = std::chrono::steady_clock::now() - lastCommandTime_;
        //if (elapsed < std::chrono::seconds(1)) {
        //    return true;
        //}
    }
    else if (lastCommand_.rfind("WAV ", 0) == 0) {
        int state{};
        int ret = LaserState(state);
        if (ret != 0)
            return ret;
        return state == 25;
    }
    else if (lastCommand_ == "OFF")
    {
        
    }
    return false;
}

int SpectraPhysicsInsightDS::SetOpen(bool open)
{
    // TODO: Record the last sent command and last sent time. If the last sent command is open,
    // then Busy() needs to return true for 1 second. If the last sent command is close, then
    // Busy() needs to return True until STB reports the shutter is closed (0).
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

int SpectraPhysicsInsightDS::OnWatchdog(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(watchdogDisabled_ ? g_Yes : g_No);
	}
	else if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
			// revert
			pProp->Set(watchdogDisabled_ ? g_Yes : g_No);
			return ERR_WATCHDOG_CHANGE_FORBIDDEN;
		}
        std::string disabledStr;
		pProp->Get(disabledStr);
        watchdogDisabled_ = (disabledStr == g_Yes);
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
        // TODO Busy should return True() until READ:WAV? is the same as what we just set here
        // Consider also checking the STB command state bits for the READY value (25)
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

int SpectraPhysicsInsightDS::OnHumidity(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
        std::string humidityPct;
        int ret = ExecuteCommand("READ:HUM?", humidityPct);
        if (ret != 0)
            return ret;
		pProp->Set(humidityPct.c_str());
	}
	return DEVICE_OK;
}

int SpectraPhysicsInsightDS::OnDiode1Current(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
        std::string diode1Current;
        int ret = ExecuteCommand("READ:PLAS:DIOD1:CURR?", diode1Current);
        if (ret != 0)
            return ret;
		pProp->Set(diode1Current.c_str());
	}
	return DEVICE_OK;
}

int SpectraPhysicsInsightDS::OnDiode2Current(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
        std::string diode2Current;
        int ret = ExecuteCommand("READ:PLAS:DIOD2:CURR?", diode2Current);
        if (ret != 0)
            return ret;
		pProp->Set(diode2Current.c_str());
	}
	return DEVICE_OK;
}

int SpectraPhysicsInsightDS::OnDiode1Temp(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
        std::string diode1Temperature;
        int ret = ExecuteCommand("READ:PLAS:DIOD1:TEMP?", diode1Temperature);
        if (ret != 0)
            return ret;
		pProp->Set(diode1Temperature.c_str());
	}
	return DEVICE_OK;
}

int SpectraPhysicsInsightDS::OnDiode2Temp(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
        std::string diode2Temperature;
        int ret = ExecuteCommand("READ:PLAS:DIOD2:TEMP?", diode2Temperature);
        if (ret != 0)
            return ret;
		pProp->Set(diode2Temperature.c_str());
	}
	return DEVICE_OK;
}

int SpectraPhysicsInsightDS::OnPower(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
        std::string power;
        int ret = ExecuteCommand("READ:POW?", power);
        if (ret != 0)
            return ret;
		pProp->Set(power.c_str());
	}
	return DEVICE_OK;
}


int SpectraPhysicsInsightDS::OnPumpLaser(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		// Bit 0 identifies energized state, 1 means energized
        bool isEnergized;
		int ret = StatusBit(0, isEnergized);
        if (ret != 0)
            return ret;
        pProp->Set(isEnergized ? g_On : g_Off);
	}
	else if (eAct == MM::AfterSet)
	{
        std::string cmd;
        pProp->Get(cmd);
        if (cmd == g_On)
        {
            std::string warmupPct{};
            int ret{};
			ret = ExecuteCommand("READ:PCTW?", warmupPct);
			if (ret != 0)
				return ret;
			if (stoi(warmupPct) < 100)
				return ERR_PUMP_LASER_NOT_WARM;
			// TODO Consider also checking the STB command state bits for the RUN value (50)
            return ExecuteCommand("ON");
		}
        else
        {
            return ExecuteCommand("OFF");
        }
	}

	return DEVICE_OK;
}

/**
 * This function not only sends the command but also reads back an answer from the device
 */
int SpectraPhysicsInsightDS::ExecuteCommand(const std::string& cmd, std::string& answer)
{
    // Grab the lock...
    std::lock_guard<std::mutex> guard(serialMutex_);
    // ...then send the command...
	int ret = SendCommand(cmd);
	if (ret != DEVICE_OK)
		return ret;

	// ...then get answer from the device.
    // It will always end with a line feed.
	return GetSerialAnswer(port_.c_str(), "\n", answer);
}

/**
 * This function sends the command to the Spectra InSight
 */
int SpectraPhysicsInsightDS::ExecuteCommand(const std::string& cmd)
{
    // Grab the lock...
    std::lock_guard<std::mutex> guard(serialMutex_);
    // ...then send the command
    return SendCommand(cmd);
}

/**
 * This function actually the command to the Spectra InSight.
 * It should not be called directly - call ExecuteCommand instead!
 */
int SpectraPhysicsInsightDS::SendCommand(const std::string& cmd)
{
	// Send command
    int ret = SendSerialCommand(port_.c_str(), cmd.c_str(), "\r");
    if (ret != 0)
        return ret;
    // Update state for Busy checks
    lastCommand_ = cmd;
	lastCommandTime_ = std::chrono::steady_clock::now();

    return DEVICE_OK;
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

int SpectraPhysicsInsightDS::LaserState(int& state)
{
    std::string status_int;
    int ret = ExecuteCommand("*STB?", status_int);
    if (ret != 0)
        return ret;

    unsigned int status_byte = stoul(status_int);
    unsigned int bit_mask = 0x007F0000;
    state = (status_byte & bit_mask) >> 16;
    return DEVICE_OK;
}

WatchdogThread::WatchdogThread(SpectraPhysicsInsightDS& device) :
    device_(device),
    stop_(false),
    interval_(std::chrono::seconds(1))
{
}

WatchdogThread::~WatchdogThread() {
    Stop();
    wait();
}

void WatchdogThread::Start()
{
    stop_ = false;
    activate();
}

void WatchdogThread::Stop()
{
    {
        std::unique_lock<std::mutex> lock(stopMutex_);
        stop_ = true;
    }
    timerCV_.notify_all();
}

int WatchdogThread::svc() {
    while (!stop_)
    {
		std::unique_lock<std::mutex> lock(stopMutex_);
        timerCV_.wait_for(lock, interval_, [this] {return stop_;  });
        if (stop_)
            break;
        // Just need to do something that sends a command
        int state;
        device_.LaserState(state);

    }

    return 0;
}
