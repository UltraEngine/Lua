#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
    #include <windows.h>
#endif

//Include Lua library
#include "lua.h"
#include "lauxlib.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

static int SetWindowZenMode(lua_State* L)
{
    if (!lua_islightuserdata(L, 1)) return luaL_argerror(L, 1, "Expected HWND");
    HWND hwnd = (HWND)lua_touserdata(L, 1);
    if (hwnd == NULL) return luaL_argerror(L, 1, "HWND is NULL");

    const auto style = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME;
    auto currentstyle = GetWindowLong(hwnd, GWL_STYLE);
    int state = luaL_checkinteger(L, 2);
    if (state == 1)
    {
        SetWindowLong(hwnd, GWL_STYLE, currentstyle & ~(style));
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 1920, 1080, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    else
    {
        SetWindowLong(hwnd, GWL_STYLE, currentstyle | style);
    }
    return 0;
}

__declspec(dllexport) int luaopen_LuaZenMode(lua_State* L) {
    lua_pushcfunction(L, SetWindowZenMode);
    return 1;
}