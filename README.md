# MyCross

A tiny Windows crosshair overlay tool.

## Features

- Always-on-top crosshair overlay.
- Click-through window (`WS_EX_TRANSPARENT`) so it does not block mouse operations.
- Optional screen coordinate launch.
- Press `Esc` to quit.

## Build

### Option 1: use script

```bat
build.bat
```

### Option 2: build manually

```bat
g++ -std=c++17 -O2 -municode -mwindows crosshair.cpp -o crosshair.exe
```

## Run

```bat
crosshair.exe
```

Starts at screen center.

```bat
crosshair.exe 960 540
```

Starts at absolute screen coordinate `(960, 540)`.

```bat
crosshair.exe --x=960 --y=540
```

Same as above using named args.

```bat
crosshair.exe --help
```

Show usage help.

## Notes

- Platform: Windows only.
- The app uses a layered popup window with color-key transparency.
