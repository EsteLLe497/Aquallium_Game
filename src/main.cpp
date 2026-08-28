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
#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    try
    {
        D3D11App app(instance);
        return app.Run();
    }
    catch (const std::exception& exception)
    {
        const std::string message = exception.what();
        MessageBoxA(nullptr, message.c_str(), "Aquarium Lighting Prototype Error", MB_OK | MB_ICONERROR);
        return 1;
    }
}
