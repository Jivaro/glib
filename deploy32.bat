msbuild /p:Platform=x86 /p:Configuration=Debug %1.hsproj
copy /B /Y ..\..\__build__\x86\Debug\lib\%1-2.0.lib ..\..\..\frida-ire\zed\ext\sdk\Win32-Debug\lib\
copy /B /Y ..\..\__build__\x86\Debug\lib\%1-2.0.pdb ..\..\..\frida-ire\zed\ext\sdk\Win32-Debug\lib\