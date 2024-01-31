@echo off

set "flatc=%cd%\..\..\..\..\chocolate\thirdparty\flatbuffers\build\Release\flatc.exe"
"%flatc%" -c "sidury.fbs"

pause