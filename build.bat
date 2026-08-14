@echo off
setlocal enabledelayedexpansion

set VCDIR=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207
set SDKINC=C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0
set SDKLIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0
set UCRTINC=C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt
set UM_INC=C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um
set UCRT_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64
set UM_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64

set INCLUDE=%VCDIR%\include;%SDKINC%\ucrt;%SDKINC%\um;%SDKINC%\shared;%INCLUDE%
set LIB=%VCDIR%\lib\x64;%UCRT_LIB%;%UM_LIB%;%LIB%

set BLD=D:\Temp\opencode\el_build
if not exist "%BLD%" mkdir "%BLD%"

echo === Building injector.exe ===
"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /Fe:%BLD%\injector.exe injector\main.cpp injector\launcher.cpp /link /SUBSYSTEM:CONSOLE advapi32.lib user32.lib shell32.lib
if %ERRORLEVEL% neq 0 (
    echo INJECTOR BUILD FAILED
    exit /b 1
)

echo === Building el_native.dll ===
"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_hook.obj el_native\hook.cpp /I minhook\include /I minhook\src /I minhook\src\hde
if %ERRORLEVEL% neq 0 (
    echo hook.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_resolve.obj el_native\il2cpp_resolve.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo il2cpp_resolve.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_render.obj el_native\render.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo render.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_feature.obj el_native\feature.cpp /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo feature.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_resource_dump.obj el_native\features\resource_dump.cpp /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo resource_dump.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_game_speed.obj el_native\features\game_speed.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo game_speed.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_currency.obj el_native\features\currency.cpp /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo currency.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_damage.obj el_native\features\damage.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo damage.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_netlog.obj el_native\features\netlog.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo netlog.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_gacha.obj el_native\features\gacha.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo gacha.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_monster_dump.obj el_native\features\monster_dump.cpp /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo monster_dump.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_battle_shop.obj el_native\features\battle_shop.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo battle_shop.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_energy_attack_speed.obj el_native\features\energy_attack_speed.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo energy_attack_speed.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_battle_result.obj el_native\features\battle_result.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo battle_result.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_anticheat.obj el_native\features\anticheat.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo anticheat.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_tracer.obj el_native\features\tracer.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo tracer.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_battle_combat.obj el_native\features\battle_combat.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo battle_combat.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_compatibility_patch.obj el_native\compatibility_patch.cpp /I minhook\include /I minhook\src /I minhook\src\hde
if %ERRORLEVEL% neq 0 (
    echo compatibility_patch.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_multichest_delegate_guard.obj el_native\multichest_delegate_guard.cpp
if %ERRORLEVEL% neq 0 (
    echo multichest_delegate_guard.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_automation.obj el_native\automation.cpp
if %ERRORLEVEL% neq 0 (
    echo automation.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_automation_feature.obj el_native\features\automation_feature.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo automation_feature.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_main_thread_dispatcher.obj el_native\main_thread_dispatcher.cpp
if %ERRORLEVEL% neq 0 (
    echo main_thread_dispatcher.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_dev_menu.obj el_native\features\dev_menu.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo dev_menu.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_combat_runtime.obj el_native\combat_runtime.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo combat_runtime.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_combat_runtime_adapter.obj el_native\combat_runtime_adapter.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo combat_runtime_adapter.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_hook_registry.obj el_native\hook_registry.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo hook_registry.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_profile_store.obj el_native\profile_store.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo profile_store.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_config_registry.obj el_native\config_registry.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo config_registry.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_profile_ui.obj el_native\profile_ui.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo profile_ui.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_launcher.obj injector\launcher.cpp
if %ERRORLEVEL% neq 0 (
    echo launcher.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_combat_runtime_feature.obj el_native\features\combat_runtime_feature.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo combat_runtime_feature.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_relationship.obj el_native\features\relationship.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo relationship.cpp COMPILE FAILED
    exit /b 1
)

if %ERRORLEVEL% neq 0 (
    echo roulette_trace.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_proxy_log_overlay.obj el_native\features\proxy_log_overlay.cpp /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo proxy_log_overlay.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_action_tracer.obj el_native\features\action_tracer.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo action_tracer.cpp COMPILE FAILED
    exit /b 1
)

echo === Building ImGui ===
set IMGUI=third_party\imgui
"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\imgui.obj %IMGUI%\imgui.cpp /I %IMGUI%
if %ERRORLEVEL% neq 0 ( echo imgui.cpp FAILED & exit /b 1 )
"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\imgui_draw.obj %IMGUI%\imgui_draw.cpp /I %IMGUI%
if %ERRORLEVEL% neq 0 ( echo imgui_draw.cpp FAILED & exit /b 1 )
"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\imgui_tables.obj %IMGUI%\imgui_tables.cpp /I %IMGUI%
if %ERRORLEVEL% neq 0 ( echo imgui_tables.cpp FAILED & exit /b 1 )
"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\imgui_widgets.obj %IMGUI%\imgui_widgets.cpp /I %IMGUI%
if %ERRORLEVEL% neq 0 ( echo imgui_widgets.cpp FAILED & exit /b 1 )
"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\imgui_impl_dx11.obj %IMGUI%\imgui_impl_dx11.cpp /I %IMGUI%
if %ERRORLEVEL% neq 0 ( echo imgui_impl_dx11.cpp FAILED & exit /b 1 )
"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\imgui_impl_win32.obj %IMGUI%\imgui_impl_win32.cpp /I %IMGUI%
if %ERRORLEVEL% neq 0 ( echo imgui_impl_win32.cpp FAILED & exit /b 1 )

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /Fe:%BLD%\el_native.dll el_native\dllmain.cpp ^
    minhook\src\buffer.c minhook\src\hook.c minhook\src\trampoline.c minhook\src\hde\hde64.c ^
    %BLD%\el_hook.obj %BLD%\el_compatibility_patch.obj %BLD%\el_multichest_delegate_guard.obj %BLD%\el_automation.obj %BLD%\el_automation_feature.obj %BLD%\el_main_thread_dispatcher.obj %BLD%\el_dev_menu.obj %BLD%\el_render.obj %BLD%\el_resolve.obj %BLD%\el_feature.obj %BLD%\el_game_speed.obj %BLD%\el_currency.obj %BLD%\el_resource_dump.obj ^
    %BLD%\imgui.obj %BLD%\imgui_draw.obj %BLD%\imgui_tables.obj %BLD%\imgui_widgets.obj ^
    %BLD%\imgui_impl_dx11.obj %BLD%\imgui_impl_win32.obj ^
    /I minhook\include /I minhook\src /I minhook\src\hde /I %IMGUI% /link /DLL /SUBSYSTEM:WINDOWS d3d11.lib dxgi.lib
if %ERRORLEVEL% neq 0 (
    echo DLL LINK FAILED
    exit /b 1
)

echo === Build complete ===
echo   %BLD%\injector.exe
echo   %BLD%\el_native.dll

rem Deploy the two runtime files to the game directory after every successful build.
rem Override EL_GAME_DIR when using another installation; missing directories only
rem skip deployment so CI/builds on another machine still succeed.
if not defined EL_GAME_DIR set "EL_GAME_DIR=D:\SteamLibrary\steamapps\common\Everlusting Life"
if exist "%EL_GAME_DIR%" (
    echo === Deploying to game ===
    copy /Y "%BLD%\injector.exe" "%EL_GAME_DIR%\injector.exe" >nul
    if %ERRORLEVEL% neq 0 (
        echo injector.exe DEPLOY FAILED
        exit /b 1
    )
    copy /Y "%BLD%\el_native.dll" "%EL_GAME_DIR%\el_native.dll" >nul
    if %ERRORLEVEL% neq 0 (
        echo el_native.dll DEPLOY FAILED
        exit /b 1
    )
    echo   %EL_GAME_DIR%\injector.exe
    echo   %EL_GAME_DIR%\el_native.dll
    echo === Deploying proxy to game ===
    copy /Y el_proxy.py "%EL_GAME_DIR%\el_proxy.py" >nul
    if %ERRORLEVEL% neq 0 (
        echo el_proxy.py DEPLOY FAILED
        exit /b 1
    )
    copy /Y el_proxy.json "%EL_GAME_DIR%\el_proxy.json" >nul
    if %ERRORLEVEL% neq 0 (
        echo el_proxy.json DEPLOY FAILED
        exit /b 1
    )
    copy /Y adultchess.crt "%EL_GAME_DIR%\adultchess.crt" >nul
    if %ERRORLEVEL% neq 0 (
        echo adultchess.crt DEPLOY FAILED
        exit /b 1
    )
    copy /Y adultchess.key "%EL_GAME_DIR%\adultchess.key" >nul
    if %ERRORLEVEL% neq 0 (
        echo adultchess.key DEPLOY FAILED
        exit /b 1
    )
    if exist el_proxy.log copy /Y el_proxy.log "%EL_GAME_DIR%\el_proxy.log" >nul
    echo   %EL_GAME_DIR%\el_proxy.py / .json / certs
) else (
    echo GAME_DIR not found; deployment skipped: %EL_GAME_DIR%
)

echo === Building runtime/profile tests ===
"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /EHsc /std:c++14 tests\combat_runtime_profiles_tests.cpp ^
    el_native\combat_runtime.cpp el_native\combat_runtime_adapter.cpp el_native\automation.cpp el_native\main_thread_dispatcher.cpp el_native\multichest_delegate_guard.cpp el_native\hook_registry.cpp el_native\profile_store.cpp el_native\config_registry.cpp injector\launcher.cpp ^
    /I el_native /I injector /I third_party\imgui /Fe:%BLD%\combat_runtime_profiles_tests.exe
if %ERRORLEVEL% neq 0 (
    echo runtime/profile tests BUILD FAILED
    exit /b 1
)
echo   %BLD%\combat_runtime_profiles_tests.exe

echo === Building proxy log parser tests ===
"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /EHsc /std:c++14 tests\proxy_log_parse_tests.cpp ^
    /I el_native /Fe:%BLD%\proxy_log_parse_tests.exe
if %ERRORLEVEL% neq 0 (
    echo proxy log parser tests BUILD FAILED
    exit /b 1
)
echo   %BLD%\proxy_log_parse_tests.exe

echo === Building action trace tests ===
"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /EHsc /std:c++14 tests\action_trace_tests.cpp ^
    /I el_native /Fe:%BLD%\action_trace_tests.exe
if %ERRORLEVEL% neq 0 (
    echo action trace tests BUILD FAILED
    exit /b 1
)
echo   %BLD%\action_trace_tests.exe

