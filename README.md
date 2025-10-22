# Jarvis \(E-Bike central management system\)

## Introduction

Built for:
- Far-driver motor controller \(南京远驱控制器\)
- AntBMS \(蚂蚁保护版\)

Features:
- Reads telemetry data from motor controller & BMS system
- Anti-theft & BLE unlocking
- SoftAP with a web server for easy management
- GPS tracking
- IoT \(4G networking\) for remote management

This project is based on the ESP32S3 board. Although it should also work on other boards with some tweaks.
The system is actively used in my e-bike customization. I only recommend using this as a add-on and keep the
other systems such as the original odometer and anti-theft system as-is. You can hook this in parallel so that
it keeps the original hardware of the bike functional while provided added features \(just-in-case something
with the DIY solution fails\).

## Project structure

```
├── CMakeLists.txt
├── pytest_hello_world.py      Python script used for automated testing
├── main
│   ├── CMakeLists.txt
│   └── hello_world_main.c
└── README.md                  This is the file you are currently reading
```

## Building

After cloning the repo, also clone the submodules:

```
git submodule update --init --recursive
```

Inside of the frontend \(web/\) folder:
- Install the dependencies
- Build the frontend
- Execute `npx svelteesp32 -e espidf -s ../web/dist -o ../main/includes/svelteesp32.h --etag=true` to generate the header file


```
cd web
pnpm install
pnpm run build
npx svelteesp32 -e espidf -s ../web/dist -o ../main/includes/svelteesp32.h --etag=true
```

## Examples

## Technical support and feedback

Feel free to contact me at wjw_03@outlook.com for any issues or inquires. Please set the subject with the name
of this project and that it's from GitHub. I will try to get back to you ASAP.

Please also feel free to post issues and PRs.

## Copyright notice

For to use "as-is" for any non-commercial purposes. I do not concent to sharing the code to be used
for commercial purposes without contact me first.