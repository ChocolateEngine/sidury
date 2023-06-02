@echo off

@REM Add CapnProto to the system path so it can find the plugins
set "CapnProtoPath=%cd%\..\..\..\..\chocolate\thirdparty\capnproto\build\c++\src\capnp\Release"
set "PATH=%PATH%;%CapnProtoPath%"

capnp.exe compile -oc++ "sidury.capnp"

pause