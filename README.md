# JiggleBuddy

A tiny (~6 KB) Windows tray app that keeps your PC awake and your status active.
It's one C file with no runtime dependencies: no CRT, .NET or installer.

## What it does

- Sits in the system tray as a dot: **green** means on, **grey** means off.
- When you've been idle for the interval (default **60 s**), it nudges the mouse
  1 px and back, then puts the cursor exactly where it was. You won't see it move.
- While enabled, it also tells Windows not to sleep or turn off the display.
- It does nothing while you're actively using the mouse or keyboard.

## Usage

| Action                | Result                                         |
|-----------------------|------------------------------------------------|
| Left-click tray icon  | Toggle on/off                                  |
| Right-click tray icon | Menu: 1 / 15 / 30 / 60 seconds, On / Off, Exit |
| `jigglebuddy.exe 30`  | Start with a custom idle interval in seconds   |

The interval is how long you have to be idle before it jiggles. Hover the tray
icon to see the current state and interval.

Only one copy runs at a time. To start it at login, put a shortcut in
`shell:startup` (press Win+R and type `shell:startup`).

## Download

Grab `jigglebuddy.exe` from the latest release, or from the artifacts of any successful **Build** workflow run.

## Build

**MinGW-w64** (Linux, macOS or MSYS2):

```bash
make
```

**MSVC**: run `build.bat` from a "Developer Command Prompt for VS".
