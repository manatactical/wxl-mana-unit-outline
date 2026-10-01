// wxl-unit-outline: extension entry points.
// Copyright (C) 2026 WarcraftXL
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "Outline.hpp"

#include "wxl/PluginApi.h"
#include "wxl/EventScript.hpp"

#include <windows.h>

#include <new>
#include <string>

// The two exports the core resolves by name. WXL_Query must have no side effects (it describes the
// module before anything runs); WXL_Load receives the core's service table and is where the module is
// built. The event base class has no table until Bind is called, so the instance is constructed here,
// from WXL_Load, rather than at static-initialisation time.
namespace
{
    const WXL_Api* g_api = nullptr;
    wxl::scripts::outline::Outline* g_module = nullptr;

    void __cdecl DrawPanel(void*)
    {
        if (g_api && g_module) g_module->DrawPanel(*g_api);
    }

    std::string ModuleDirectory(HMODULE module)
    {
        char path[MAX_PATH] = {};
        const DWORD n = GetModuleFileNameA(module, path, MAX_PATH);
        std::string dir(path, n);
        const size_t slash = dir.find_last_of("\\/");
        return slash == std::string::npos ? std::string() : dir.substr(0, slash + 1);
    }

    HMODULE ThisModule()
    {
        HMODULE module = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&ThisModule), &module);
        return module;
    }
}

extern "C" __declspec(dllexport) const WXL_PluginInfo* __cdecl WXL_Query()
{
    static const WXL_PluginInfo info = {
        sizeof(WXL_PluginInfo),
        WXL_API_VERSION,
        "wxl-unit-outline",
        1,
        WXL_CLIENT_BUILD,
    };
    return &info;
}

extern "C" __declspec(dllexport) int __cdecl WXL_Load(const WXL_Api* api)
{
    if (!api) return 0;
    g_api = api;

    wxl::ext::EventScript::Bind(api);

    g_module = new (std::nothrow) wxl::scripts::outline::Outline();
    if (!g_module)
    {
        if (api->Log) api->Log(WXL_LOG_ERROR, "wxl-unit-outline", "module allocation failed");
        return 0;
    }
    g_module->SetApi(api);
    g_module->LoadConfig(ModuleDirectory(ThisModule()) + "wxl-unit-outline.ini");

    if (api->UiAddPanel) api->UiAddPanel("Unit Outline", &DrawPanel, nullptr);

    if (api->Log) api->Log(WXL_LOG_INFO, "wxl-unit-outline", "unit outline ready");
    return 1;
}
