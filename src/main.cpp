/*==================================================================================================

   [main.cpp]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/05/12
----------------------------------------------------------------------------------------------------

===================================================================================================*/
#include "main.h"
#include "D3D11App.h"

#include <exception>
#include <string>
#include <stdexcept>
#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    // MCI's mpegvideo driver needs STA on the calling (game/UI) thread.
    const HRESULT comResult=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int result=1;
    try
    {
        if(FAILED(comResult))throw std::runtime_error("COM STA initialization failed: "+std::to_string(comResult));
        D3D11App app(instance);
        result=app.Run();
    }
    catch (const std::exception& exception)
    {
        const std::string message = exception.what();
        MessageBoxA(nullptr, message.c_str(), "Aquarium Lighting Prototype Error", MB_OK | MB_ICONERROR);
    }
    if(SUCCEEDED(comResult))CoUninitialize();
    return result;
}
