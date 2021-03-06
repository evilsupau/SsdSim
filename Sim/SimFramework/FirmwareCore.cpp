#include "FirmwareCore.h"

#include <windows.h>

// TODO: fix this declaration. Cannot add to header file because of causing JSON build error
HMODULE _DllInstance;
HMODULE _NewDllInstance;

typedef void(__stdcall *fInitialize)(NandHal* nandHal, BufferHal* bufferHal, CustomProtocolHal* CustomProtocolHal);
typedef void(__stdcall *fSetIpcName)(const std::string& ipcName);
typedef void(__stdcall *fExecute)();
typedef void(__stdcall *fExecuteCallback)(std::function<bool(std::string)> callback);
typedef void(__stdcall *fShutdown)();

FirmwareCore::FirmwareCore() : _Execute(nullptr), _NewExecute(nullptr), _NandHal(nullptr), _BufferHal(nullptr), _CustomProtocolHal(nullptr)
{
    _DllInstance = NULL;
    _NewDllInstance = NULL;
}

bool FirmwareCore::SetExecute(std::string Filename)
{
    _NewDllInstance = LoadLibrary(Filename.c_str());

    if (!_NewDllInstance)
    {
        return false;
    }

    auto initialize = (fInitialize)GetProcAddress(_NewDllInstance, "Initialize");
    if (initialize)
    {
        initialize(_NandHal, _BufferHal, _CustomProtocolHal);
    }

    auto execute = (fExecute)GetProcAddress(_NewDllInstance, "Execute");
    if (!execute)
    {
        FreeLibrary(_NewDllInstance);
        return false;
    }

    auto setExecuteCallback = (fExecuteCallback)GetProcAddress(_NewDllInstance, "SetExecuteCallback");
    if (setExecuteCallback)
    {
        std::function<bool(std::string)> func = std::bind(&FirmwareCore::SetExecute, this, std::placeholders::_1);
        setExecuteCallback(func);
    }

    _NewExecute = execute;
    return true;
}

void FirmwareCore::Run()
{
    if (_NewExecute)
    {
        SwapExecute();
    }

    if (_Execute)
    {
        _Execute();
    }
}

void FirmwareCore::Unload()
{
    if (_DllInstance)
    {
        auto shutdown = (fShutdown)GetProcAddress(_DllInstance, "Shutdown");
        if (shutdown)
        {
            shutdown();
        }
        FreeLibrary(_DllInstance);
    }

    if (_NewDllInstance)
    {
        FreeLibrary(_NewDllInstance);
    }
}

void FirmwareCore::SwapExecute()
{
    if (_DllInstance)
    {
        auto shutdown = (fShutdown)GetProcAddress(_DllInstance, "Shutdown");
        if (shutdown)
        {
            shutdown();
        }

        // Wait for the current code completely
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        FreeLibrary(_DllInstance);
    }

    _Execute = _NewExecute;
    _NewExecute = nullptr;

    _DllInstance = _NewDllInstance;
    _NewDllInstance = NULL;
}

void FirmwareCore::SetHalComponents(NandHal* nandHal, BufferHal* bufferHal, CustomProtocolHal* CustomProtocolHal)
{
    _NandHal = nandHal;
    _BufferHal = bufferHal;
    _CustomProtocolHal = CustomProtocolHal;
}
