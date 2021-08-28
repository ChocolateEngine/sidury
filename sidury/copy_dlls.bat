@REM call ../chocolate/thirdparty/thirdparty.bat

set "engine_bin_dir=..\chocolate\bin\win"
set "game_bin_dir=."

robocopy %engine_bin_dir% %game_bin_dir% *.dll

