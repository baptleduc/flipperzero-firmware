# LoRa Satellite Balloon Tracker App
## Connection Instructions

### LoRa Grove-Wio-E5 Module
To connect the Grove-Wio-E5 module to the Flipper zero, simply connect the pins as follows: 

Pinout: Grove cable on Flipper GPIO
* `Red` on `3V3` (9)
* `Black` on `GND` (11)
* `White` on `TX` (13)
* `Yellow` on `RX` (14)

## Build Instructions

There are two build modes: **Debug** (default) and **Release**.

```sh
./fbt COMPACT=1 APPSRC={app_id}      # Debug mode
./fbt COMPACT=1 DEBUG=0 APPSRC={app_id}  # Release mode
```

The `{app_id}` corresponds to the `app_id` specified in the `application.fam` file. 
This project contains two apps: the main application and a test suite.

To build the main app in **Release mode**, use:

```sh
./fbt COMPACT=1 DEBUG=0 APPSRC=lora_app
```

## Deploying the App

To upload and launch the compiled app via USB, run:

```sh
./fbt DEBUG=0 launch APPSRC=lora_app
```

To **build** and **upload** all FAP apps in one command:

```sh
./fbt DEBUG=0 fap_deploy  # Release mode
```

## Debugging

To view log traces from the device's CLI, use:

```sh
./fbt cli
```
