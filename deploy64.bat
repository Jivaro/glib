msbuild /p:Platform=x86_64 /p:Configuration=Debug %1.hsproj
copy /B /Y ..\..\__build__\x86_64\Debug\lib\%1-2.0.lib ..\..\..\frida-ire\zed\ext\sdk\x64-Debug\lib\
copy /B /Y ..\..\__build__\x86_64\Debug\lib\%1-2.0.pdb ..\..\..\frida-ire\zed\ext\sdk\x64-Debug\lib\