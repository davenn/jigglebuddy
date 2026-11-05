@echo off
rem Build with MSVC from a "Developer Command Prompt for VS".
cl /nologo /O1 /GS- /W3 jigglebuddy.c /link /SUBSYSTEM:WINDOWS /ENTRY:entry /NODEFAULTLIB ^
   user32.lib kernel32.lib shell32.lib gdi32.lib /OUT:jigglebuddy.exe
