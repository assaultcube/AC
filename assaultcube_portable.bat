@echo off
%~d0
cd %~p0
start bin_win32\ac_client.exe --home=profile --init %1 %2 %3 %4 %5
