# uDMX Matlab Commander (for Windows)

Sometimes, you just need to set up a simple experiment and have to control some lights using Matlab. Sometimes, the lights are 'intelligent' lights, and have [DMX512](https://en.wikipedia.org/wiki/DMX512) interfaces. There are [these cheap (16c0:05e4) dongles](https://anyma.ch/research/udmx/) that can be used to control DMX512 devices, but it is suprisingly difficult to get these to work. This implementation uses the [libusbK](https://libusbk.sourceforge.net/UsbK3/examples_8h.html) driver to communicate with the device.

Note that this code is for the ones that are advertised to be compatible with the [FreeStyler](https://www.freestylerdmx.be/) DMX software: sometimes it's referred to as the [anyma uDMX USB_DMX interface](https://anyma.ch/research/udmx/), or the [ILLUTZMINATOR001 - uDMX - VOTI Artnet node](https://www.illutzmination.de/udmx.html?&L=1). On popular webshops, you could also find it as 'Low-cost USB-DMX cable' that comes with a CD and a note to follow the instructions carefully. With good reason. :)

## Setting up

* If you want to keep it simple and don't want to compile, [download the binary in the release section](https://github.com/ha5dzs/udmx-matlab-commander/releases).

* Install the **included** ILLUZMINATION uDMX driver. In the device manager, you should see the uDMX device under 'libusbK Usb Devices'.

* Add the `.mexw64` file, along with the dll files to your Matlab path.

* you now have access to the `dmx()` function, see below how to use it.

## Usage

### `dmx('send', addresses, data_values)`
```Matlab
% DMX is a function that allows you to send DMX512 frames using a uDMX dongle.
% Input arguments are:
%     -'send', which must be here and set to this value. This is the entry point for the mex function.
%     -addresses is a vector of increasing integers that defines the DMX channel range.
%     -data_values is a vector which contains the channel data.
% Returns:
%     - 0 if everything went well, 1 otherwise.
% IMPORTANT:
%     -Everything must be integers. If you use different variable types (i.e. Double), the function will convert them to integers.
%     -The addresses must be strictly monotonically increasing by 1, in a continuous interval: 25:40 is fine, but 23:30 and 34:40 concatenated together is not.
%     -While there are sanity checks in the code, make sure you know what you are doing first. If not, then the function may crash, which in turn crashes Matlab, and your data within it.
```

### Examples

Let's say you have a cheap DMX RGB light. The instructions are:
Channel | Value range | Function
--------|:-------:|---------
Base address `+0` | `0-255` | Total Dimming
Base address `+1` | `0-255` | Red
Base address `+2` | `0-255` | Green
Base address `+3` | `0-255` | Blue
Base address `+4` | `0-255` | Strobe
Base address `+5` | `0-50` | No function
Base address `+5` | `51-100` | Preset colours
Base address `+5` | `101-150` | Silly effect 1
Base address `+5` | `151-200` | Silly effect 2
Base address `+5` | `201-255` | Sound-activated mode
Base address `+6` | `0-255` | Silly effect change speed (slow to fast)

There should be a display and some buttons on the light, where you can set the base address. Let's set it to `100`. So `100` becomes the dimming channel, `101` is red, and so on.

You can specify a single channel, or multiple channels.

* At first, it's probably best to specify all the channels and fill the data with zeros, just in case there was some previous information stored in the light's memory:
`dmx('send', [100:106], zeros(1, 7))`

* Set full brightness, green only.
`dmx('send', [100, 101, 102], [255, 0, 255])`

* Now the light is behaving how it should, why not add some red to the green to make it yellow?
`dmx('send', 101, 255)`

* ...and finally, we reconfigure the entire light to produce some soothing effect in the office:
`dmx('send', [100:106], [64, 0, 0, 0, 0, 160, 80])`

Since there are a lot of devices that use the DMX512 standard, you need to know what device you are connecting to. If you don't understand what channels and what values correspond to which functions, you could present a danger to health or equipment.
### Additional diagnostic functions

If something doesn't work, these functions allow you to check whether your uDMX device is detectable and/or a connection can be established.

* `dmx('list')` prints the available devices to the command window. This is useful if you want to verify if the driver is loaded correctly.

* `dmx('devicetest')` attempts to open and close connection to the device. The default USB VID/PID is `16c0:05dc`. This is set with two `#define`s in lines 38-39 of `dmx.c`, change it and recompile if your device is different.

### How does it work?

The code does a bunch of sanity checks on the inputs. It gets a list of the USB devices that use libusbk/winusb (`LstK_Init(&deviceList, 0)`), selects the correct one by vid/pid (`LstK_FindByVidPid(deviceList, UDMX_VENDOR_ID, UDMX_PRODUCT_ID, &deviceInfo)`), loads the driver API (`LibK_LoadDriverAPI(&Usb, deviceInfo->DriverID)`), then opens the selected device (`Usb.Init(&handle, deviceInfo)`). Then it takes the previously-sanity-checked-and-appropriately-converted input arguments, and transfers all this information to the device (`UsbK_ControlTransfer(handle, Pkt, data_to_be_sent, no_of_channels, &transferred, NULL)`) from the host computer as a vendor-type request. The [firmware](https://github.com/mirdej/udmx/blob/master/firmware/main.c) on the usb device's Atmel microcontroller updates its buffer and updates the DMX frames accordingly.

Once all the transfer is finished, it frees the USB device (`Usb.Free(handle)`) and lets go of the device list (`LstK_Free(deviceList)`) as well. For good measure, the code also returns a boolean to indicate if the transfer was successful (0) or not (1).

#### What is different from other implementations?

This code is designed to be natively run on the computer with Matlab. I also migrated from [libusb-win32](https://sourceforge.net/projects/libusb-win32/) to the more recent [libusbK](https://github.com/mcuee/libusbk). The [illuzmination implementation](https://www.illutzmination.de/udmxdriver.html?&L=1) failed to run on my machines, presumably because it doesn't include any 64-bit code. Not having found the source code for the `uDMX.dll`, I decided to write my own. (...and reverse-engineer some code, understand libusb-win32, and libusbK, and find out a bunch of stuff about the USB standard I never thought I needed. I am not sure if I did this correctly, but it works, I can control my lights, and it only takes 6-7 millisconds to send instructions.)

#### What is implemented?

The microcontroller's code has three features, only one of them used in this code:

* `cmd_SetChannelRange` (`0x02`):
This allows you to set the values of one or many consecutive channels.

The following are *NOT* implemented, and probably won't be:

* `cmd_SetSingleChannel` (`0x01`):
This one allows you to set the value of a single channel. `cmd_SetChannelRange` can do everything this can do. There is very little extra communication overhead here, so there is no real reason for this to be implemented.

* `cmd_StartBootloader` (`0x0F8`):
This one is for the firmware update over USB. Since [nobody really touched this in the past decade or so](https://github.com/mirdej/udmx/blob/master/firmware/main.c), I don't think it's a good idea to risk bricking devices by allowing the upload of outdated or corrupt firmware. If you are desperate for a new firmware, disassemble the device, and upload it using a USBasp programmer.

## Compiling

Since this is a windows-only project, it's probably best if you used a recent version of the Microsoft Visual C++ compilers.
I have not been able to be successful with MinGW64, even if I installed the Windows SDK. But I didn't really try very hard, I am using Microsoft compilers for other projects too.

```Matlab
 clc; mex -R2018a dmx.c -llibusbK
```
