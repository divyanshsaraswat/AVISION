@echo off
echo Args passed to run_build_core: %* > debug_args.txt
echo Calling build.bat core >> debug_args.txt
call build.bat core > build_log.txt 2>&1
exit
