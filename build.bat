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
"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /Fe:%BLD%\injector.exe injector\main.cpp /link /SUBSYSTEM:CONSOLE advapi32.lib
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

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_cheat_tester.obj el_native\features\cheat_tester.cpp /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo cheat_tester.cpp COMPILE FAILED
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

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_quest.obj el_native\features\quest.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo quest.cpp COMPILE FAILED
    exit /b 1
)

"%VCDIR%\bin\Hostx64\x64\cl.exe" /nologo /O2 /EHsc /c /Fo%BLD%\el_tracer.obj el_native\features\tracer.cpp /I minhook\include /I third_party\imgui
if %ERRORLEVEL% neq 0 (
    echo tracer.cpp COMPILE FAILED
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
    %BLD%\el_hook.obj %BLD%\el_render.obj %BLD%\el_resolve.obj %BLD%\el_feature.obj %BLD%\el_game_speed.obj %BLD%\el_currency.obj %BLD%\el_resource_dump.obj ^
    %BLD%\el_damage.obj %BLD%\el_netlog.obj %BLD%\el_gacha.obj %BLD%\el_cheat_tester.obj %BLD%\el_monster_dump.obj %BLD%\el_battle_shop.obj %BLD%\el_energy_attack_speed.obj %BLD%\el_battle_result.obj %BLD%\el_anticheat.obj %BLD%\el_quest.obj %BLD%\el_tracer.obj ^
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
