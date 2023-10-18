/*
    uDMX Matlab commander.
    Control stuff on the DMX512 bus from Matlab.

    I basically put every function that I needed together to a single file.

    See docs on github.

*/

// Comment this line out if you don't want to see stray mexPrintf()'s in your commnand window.
//#define VERBOSE

// Matlab-specific stuff
#include "mex.h"
#include "matrix.h"

// Good old C-specific stuff
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// Windows-specific stuff
#include <windows.h>

// libusbK
#include <usb.h>
#include "libusbk.h"

// uDMX-specific stuff
#include "uDMX_cmds.h"

KUSB_DRIVER_API Usb;


// This is my device, your might be different.
#define UDMX_VENDOR_ID (UINT)0x16c0
#define UDMX_PRODUCT_ID (UINT)0x05dc


// This function is called by the LstK_Enumerate function for each
// device until it returns FALSE.
static BOOL KUSB_API ShowDevicesCB(KLST_HANDLE DeviceList,
                                   KLST_DEVINFO_HANDLE deviceInfo,
                                   PVOID MyContext)
{
    // print some information about the device.
    mexPrintf("%04X:%04X (%s): %s - %s\n",
           deviceInfo->Common.Vid,
           deviceInfo->Common.Pid,
           deviceInfo->Common.InstanceID,
           deviceInfo->DeviceDesc,
           deviceInfo->Mfg);

    // If this function returns FALSE then enumeration ceases.
    return TRUE;
}




/*
    This is a multiple entry-point function. when you call this, the first argument is the function's name.
*/

void mexFunction( int nlhs, mxArray *plhs[],
                  int nrhs, const mxArray *prhs[])
{
    /*
        Local variables
    */
    char stringBuffer[128]; // This is for the function name. 128 bytes are generous.


    /*
        Sanity checks on the first input argument.
        Everything else is done within the corresponding parts.
    */

    // Do we at least have 1 input argument?.
	if (nrhs < 1) {
		mexErrMsgTxt("dmx.mex::This function needs at least one input argument!\n");
	}

	// Is the first input argument a string?
	if (!mxIsChar(prhs[0])) {
		mexErrMsgTxt("dmx.mex::The first argument is a function name string. Check the documentation on what is available.\n");
	}


    // Process the string
    if (mxGetString(prhs[0], stringBuffer, sizeof(stringBuffer) - 1)) {
		mexErrMsgTxt("dmx.mex::The funcion name is suspiciosly too long. Check the documentation.\n");
	}
    /*
        This bit is based on the API examples of libusbK.
        https://github.com/mcuee/libusbk/tree/master/libusbK/examples
    */

    KLST_HANDLE deviceList = NULL;
    KLST_DEVINFO_HANDLE deviceInfo = NULL;
    KUSB_HANDLE handle = NULL;
    DWORD errorCode = ERROR_SUCCESS;
    ULONG count = 0;

    /*
        Initialize a new LstK (device list) handle.
        The list is polulated with all usb devices libusbK can access.
    */
    if (!LstK_Init(&deviceList, 0))
    {
        errorCode = GetLastError();
        mexPrintf("Error code: %d.\n", errorCode);
        mexErrMsgTxt("dmx.mex::An error occured getting the device list.");
    }


    #ifdef VERBOSE
    /*
        dmx('mextest')

        I just leave this here for future reference,
    */
    if(!strcmp(stringBuffer, "mextest"))
    {
        mexPrintf("Test function is working without crashing.\n");
    }
    #endif

    /*
        dmx('list')

        Prints all the devices that use libusbK
    */
    if(!strcmp(stringBuffer, "list"))
    {
        // Get the number of devices contained in the device list.
        LstK_Count(deviceList, &count);
        if (!count)
        {
            // Always free the device list if LstK_Init returns TRUE
            LstK_Free(deviceList);

            mexErrMsgTxt("dmx.mex::No USB device that uses libusbK was detected.");
        }
        else
        {
            mexPrintf("\nFound the following devices that use the libusbK driver:\n");
            // Print devices to console.

            LstK_Enumerate(deviceList, ShowDevicesCB, NULL);

            mexPrintf("\n");
        }


    }



    /*
        dmx('devicetest')

        This one opens and closes the device.
    */

    if(!strcmp(stringBuffer, "devicetest"))
    {
        bool isPresent = FALSE;

        // Open the device, fail if cannot
        if (!LstK_FindByVidPid(deviceList, UDMX_VENDOR_ID, UDMX_PRODUCT_ID, &deviceInfo))
            mexErrMsgTxt("dmx.mex::Could not find the uDMX device.\n");

        // If we didn't die before, then load the driver API
        LibK_LoadDriverAPI(&Usb, deviceInfo->DriverID);

        // Open device.
        if(!Usb.Init(&handle, deviceInfo))
        {
            errorCode = GetLastError();
            mexPrintf("dmx.mex::Error code: %d", errorCode);
            mexErrMsgTxt("dmx.mex::Failed to open device.\n");
        }

        #ifdef VERBOSE
        mexPrintf("dmx.mex::Device opened, all good.\n");
        #endif


        // All done, clean up.
        Usb.Free(handle);
        LstK_Free(deviceList);


        plhs[0] = mxCreateLogicalScalar(isPresent);
    }




    /*
        dmx('commtest')

        This one opens the device, sends a few bytes, then closes the device.
    */

    if(!strcmp(stringBuffer, "commtest"))
    {
        bool isPresent = FALSE;

        // Open the device, fail if cannot
        if (!LstK_FindByVidPid(deviceList, UDMX_VENDOR_ID, UDMX_PRODUCT_ID, &deviceInfo))
            mexErrMsgTxt("dmx.mex::Could not find the uDMX device.\n");

        // If we didn't die before, then load the driver API
        LibK_LoadDriverAPI(&Usb, deviceInfo->DriverID);

        // Open device.
        if(!Usb.Init(&handle, deviceInfo))
        {
            errorCode = GetLastError();
            mexPrintf("dmx.mex::Error code: %d", errorCode);
            mexErrMsgTxt("dmx.mex::Failed to open device.\n");
        }

        #ifdef VERBOSE
        mexPrintf("dmx.mex::Device opened.\n");
        #endif

       /* Send a simple USB request to the uDMX device.
          I got these from:
          https://github.com/mirdej/udmx/blob/master/firmware/main.c, line 432, usbFunctionSetup()
          https://github.com/mirdej/udmx/blob/master/common/uDMX_cmds.h
          https://github.com/mirdej/udmx/blob/master/commandline/uDMX.c

            There is a uchar (or uint8) data[8] array to receive the arugment.
            -Byte 0 is either cmd_setSingleChannel (1) or cmd_setChannelRange (2) or (I don't care about this, but I leave this here for the future) cmd_StartBootloader (248)

            When data[1] is cmd_setSingleChannel:
            -data[5..8] is the channel number (0-511) (wIndex) [btw, this is some epic bitbanging, cool]
            -data[2] is the channel data (0-255) (wValue)
            -data[3] is to be set to 0, otherwise err_BadValue


            When data[1] is cmd_setChannelRange:
            -data[4] is start channel (0-511)
            -data[5] is number of channels (0-511) (wLength)
                There are sanity checks here, so the channels need to be strictly monotonically increasing

                    After the sanity checks, the usb (legacy libusb-win32) set to usb_ChannelRange (2)
                    This calls usbFunctionWrite() (line 533), which accepts the uchar data array pointer, and the number of bytes to be transmitted.
                    There are sanity checks in this one too.

            ..and then, presumably, blast the data raw without any consideration for anything




            The error codes are stored in reply[0], can can be
            -0: All OK
            -1: err_BadChannel (1)
            -2: err_BadValue (2)

            -----------------------------------------------------------------------------------------------------------------------------------------------------
            this is from uDMX.c, line 156:
            nBytes = usb_control_msg(handle,
                                USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
								cmd_SetChannelRange,
                                argc-2,
                                channel,
                                buf,
                                argc-2,
                                5000);

            From libusb-win32's documentation (https://sourceforge.net/p/libusb-win32/wiki/Documentation/)
            int usb_control_msg(usb_dev_handle *dev,
                                int requesttype,
                                int request,
                                int value,
                                int index,
                                char *bytes,
                                int size,
                                int timeout);
                *dev handle is      handle
                int requestype is   USB_TYPE_VENDOR | USB_RECIP_DEVICE |  USB_ENDPOINT_OUT (note bitwise or here)
                int request is      cmd_setChannelRange (decimal 2)
                int value is        index (or rather, indices, the channel numbers to be set)
                int index is        channel (base channel address)
                char *bytes         buf, the buffer where the channel data is
                int size            the number of channels to be set (but for here, it's the length of the buffer)
                int tmeout          usb request timeout in millisecons, this is libusb-win32 specific.




            So to change channel 100, 101, 102, 103; (4 channels, base address is 100)
            uchar channel_data[5] = [255, 0, 255, 0, 0];
            Arguments:
                -opcode (decimal 2)
                -no_of_channels
                -start_address
                -the data to be send
                -Length of data buffer, which corresponds to the number of channels

            In here, we need to send data to the device. the first three arguments are processed by usbFunctionSetup(),
            ..and the last two arguments are processed by usbFunctionWrite()

            usb_send_somehow(cmd_setChannelRange, 4, 100, 4);

            usb_send_somehow(channel_data)



       */

        // Some test data.
        UCHAR opcode = cmd_SetChannelRange;
        USHORT no_of_channels = 5;
        USHORT start_address = 99; // Address 100 onwards! (off by 1 error)
        UCHAR data_to_be_sent[] = {10, 255, 255, 0, 0}; // DIM, R, G, B, STROBE


        /*
            All these could probably go in a function for portability.
            Source: https://github.com/mcuee/libusbk/blob/master/libusbK/examples/examples.c, line 244
            Source: https://github.com/mcuee/libusbk/blob/master/libusbK/includes/libusbk.h, line 120
        */

        DWORD transferred = 0;
	    BOOL success;
	    WINUSB_SETUP_PACKET Pkt;
	    KUSB_SETUP_PACKET* defPkt = (KUSB_SETUP_PACKET*)&Pkt;

        memset(&Pkt, 0, sizeof(Pkt));
        defPkt->BmRequest.Dir	= 0; // This should be BMREQUEST_DIR_HOST_TO_DEVICE
        defPkt->BmRequest.Type	= 2; // This should be BMREQUEST_TYPE_VENDOR
        defPkt->Request			= (UCHAR) cmd_SetChannelRange;
        defPkt->Value			= (UINT) no_of_channels;
        defPkt->Index			= start_address;
        defPkt->Length			= no_of_channels;


        mexPrintf("dmx.mex::Sending data to the device.\n");
        /*
            BOOL __stdcall UsbK_ControlTransfer(KUSB_HANDLE InterfaceHandle,
                                                WINUSB_SETUP_PACKET SetupPacket,
                                                PUCHAR Buffer,
                                                UINT BufferLength,
                                                PUINT LengthTransferred,
                                                LPOVERLAPPED Overlapped)

        */

        success = UsbK_ControlTransfer(handle, Pkt, data_to_be_sent, no_of_channels, &transferred, NULL);

        mexPrintf("dmx.mex::Transferred %d Bytes.\n", transferred);

        // All done, clean up.
        mexPrintf("dmx.mex::Cleaning up..\n");
        Usb.Free(handle);
        LstK_Free(deviceList);




        plhs[0] = mxCreateLogicalScalar(!success); // fail. :)
    }


    /*
        dmx('inputtest')

        This one checks the input arguments:
        -'addresses' are the addresses to be changed in the DMX frame (1-512)
        -'data_values' are the bytes that are to be assinged to the addresses (0-255)

        Each input is a vector. The addresses must be strictly monotonically increasing.
        The number of addresses must match with the number of data values.

        I just used this bit for developing the sanity chesks for dmx('send', ...).
        Just in casem I keep this here.

    */

    if(!strcmp(stringBuffer, "inputtest"))
    {
        // Create some sanity checks on the input arguments.

        if(nrhs != 3)
            mexErrMsgTxt("dmx.mex::This function needs exactly three arguments.\n");

        // Check if the inputs are numeric arrays.
        if(!mxIsNumeric(prhs[1]))
            mexErrMsgTxt("dmx.mex::Addresses must be numbers.\n");

        if(!mxIsNumeric(prhs[2]))
            mexErrMsgTxt("dmx.mex::Data values must be numbers.\n");

        // Check dimensions of the address array
        if(mxGetNumberOfDimensions(prhs[1]) > 2)
            mexErrMsgTxt("dmx.mex::Addresses must be packed into a vector.\n");

        // Check dimensions of the data array
        if(mxGetNumberOfDimensions(prhs[2]) > 2)
            mexErrMsgTxt("dmx.mex::Data values must be packed into a vector.\n");

        // Get the dimensions of each array
        mwSize no_of_elements_address, no_of_elements_data;
        no_of_elements_address = mxGetNumberOfElements(prhs[1]);
        no_of_elements_data = mxGetNumberOfElements(prhs[2]);

        if(no_of_elements_address != no_of_elements_data)
            mexErrMsgTxt("dmx.mex::The address and data array do not have the same number of elements.\n");

        if(no_of_elements_address > 512)
            mexErrMsgTxt("dmx.mex::You only can have 512 elements in a DMX512 frame.\n");


        // Check if the input arrays are empty.
        if(mxIsEmpty(prhs[1]))
            mexErrMsgTxt("dmx.mex::Addresses must not be empty.");

        if(mxIsEmpty(prhs[2]))
            mexErrMsgTxt("dmx.mex::Data values must not be empty.");

        // Are we getting vectors?
        size_t addresses_no_of_rows = mxGetM(prhs[1]);
        size_t addresses_no_of_columns = mxGetN(prhs[1]);
        size_t data_values_no_of_rows = mxGetM(prhs[2]);
        size_t data_values_no_of_columns = mxGetN(prhs[2]);
        #ifdef VERBOSE
        mexPrintf("dmx.mex::Addresses have %d rows and %d columns,\nData values have %d rows and %d columns.\n", addresses_no_of_rows, addresses_no_of_columns, data_values_no_of_rows, data_values_no_of_columns);
        #endif

        if(addresses_no_of_rows != 1 && addresses_no_of_columns != 1)
            mexErrMsgTxt("dmx.mex::The addresses must be in a vector.\n");

        if(data_values_no_of_rows != 1 && data_values_no_of_columns != 1)
            mexErrMsgTxt("dmx.mex::The data valaues must be in a vector.\n");


        USHORT addresses_converted[512];
        UCHAR data_values_converted[512];

        mxDouble *addresses_input_pointer = mxGetData(prhs[1]);
        mxDouble *data_values_input_pointer = mxGetData(prhs[2]);

        // Is the list of addresses in strictly monotonically increasing order?
        for(unsigned int i = 1; i<no_of_elements_address; i++)
        {
            if(addresses_input_pointer[i] - addresses_input_pointer[i-1] != 1)
                mexErrMsgTxt("dmx.mex::The addresses must increase one by one.\n");

        }

        //Copy the arrays over while casting them to the required format.
        for(unsigned int i = 0; i<no_of_elements_address; i++)
        {
            // In this loop, we fetch the input data, and cast it before saving.
            #ifdef VERBOSE
            mexPrintf("%d: Addr: %d; Data: %d.\n", i, (USHORT) addresses_input_pointer[i], (UCHAR) data_values_input_pointer[i]);
            #endif
            addresses_converted[i] = (USHORT) addresses_input_pointer[i] -1; // off-by-one error: the dongle expects [0-511], reality expects [1-512].
            data_values_converted[i] = (UCHAR) data_values_input_pointer[i];
        }


        // Add the offset.
        USHORT start_address = addresses_converted[0];
        USHORT end_address = addresses_converted[no_of_elements_address - 1];
        USHORT no_of_channels = end_address - start_address;


        #ifdef VERBOSE
        mexPrintf("dmx.mex::All sanity checks passed, showing converted address range: %03d - %03d = %d\n", end_address, start_address, no_of_channels);
        #endif

        // Check the work: dmx('inputtest', [100, 101, 102, 103, 104, 105], [255, 255; 255, 255; 0, 0]);


    }



    /*
        dmx('send')

        This one checks the input arguments:
        -'addresses' are the addresses to be changed in the DMX frame (1-512)
        -'data_values' are the bytes that are to be assinged to the addresses (0-255)

        Each input is a vector. The addresses must be strictly monotonically increasing.
        The number of addresses must match with the number of data values.
    */

    if(!strcmp(stringBuffer, "send"))
    {
        /*
            The Sanity check and data preparation stuff
        */

        if(nrhs != 3)
            mexErrMsgTxt("dmx.mex::This function needs exactly three arguments.\n");

        // Check if the inputs are numeric arrays.
        if(!mxIsNumeric(prhs[1]))
            mexErrMsgTxt("dmx.mex::Addresses must be numbers.\n");

        if(!mxIsNumeric(prhs[2]))
            mexErrMsgTxt("dmx.mex::Data values must be numbers.\n");

        // Check dimensions of the address array
        if(mxGetNumberOfDimensions(prhs[1]) > 2)
            mexErrMsgTxt("dmx.mex::Addresses must be packed into a vector.\n");

        // Check dimensions of the data array
        if(mxGetNumberOfDimensions(prhs[2]) > 2)
            mexErrMsgTxt("dmx.mex::Data values must be packed into a vector.\n");

        // Get the dimensions of each array
        mwSize no_of_elements_address, no_of_elements_data;
        no_of_elements_address = mxGetNumberOfElements(prhs[1]);
        no_of_elements_data = mxGetNumberOfElements(prhs[2]);

        if(no_of_elements_address != no_of_elements_data)
            mexErrMsgTxt("dmx.mex::The address and data array do not have the same number of elements.\n");

        if(no_of_elements_address > 512)
            mexErrMsgTxt("dmx.mex::You only can have 512 elements in a DMX512 frame.\n");


        // Check if the input arrays are empty.
        if(mxIsEmpty(prhs[1]))
            mexErrMsgTxt("dmx.mex::Addresses must not be empty.");

        if(mxIsEmpty(prhs[2]))
            mexErrMsgTxt("dmx.mex::Data values must not be empty.");

        // Are we getting vectors?
        size_t addresses_no_of_rows = mxGetM(prhs[1]);
        size_t addresses_no_of_columns = mxGetN(prhs[1]);
        size_t data_values_no_of_rows = mxGetM(prhs[2]);
        size_t data_values_no_of_columns = mxGetN(prhs[2]);
        #ifdef VERBOSE
        mexPrintf("dmx.mex::Addresses have %d rows and %d columns,\nData values have %d rows and %d columns.\n", addresses_no_of_rows, addresses_no_of_columns, data_values_no_of_rows, data_values_no_of_columns);
        #endif

        if(addresses_no_of_rows != 1 && addresses_no_of_columns != 1)
            mexErrMsgTxt("dmx.mex::The addresses must be in a vector.\n");

        if(data_values_no_of_rows != 1 && data_values_no_of_columns != 1)
            mexErrMsgTxt("dmx.mex::The data valaues must be in a vector.\n");


        USHORT addresses_converted[512];
        UCHAR data_values_converted[512];

        mxDouble *addresses_input_pointer = mxGetData(prhs[1]);
        mxDouble *data_values_input_pointer = mxGetData(prhs[2]);

        // Is the list of addresses in strictly monotonically increasing order?
        for(unsigned int i = 1; i<no_of_elements_address; i++)
        {
            if(addresses_input_pointer[i] - addresses_input_pointer[i-1] != 1)
                mexErrMsgTxt("dmx.mex::The addresses must increase one by one.\n");

        }

        //Copy the arrays over while casting them to the required format.
        for(unsigned int i = 0; i<no_of_elements_address; i++)
        {
            // In this loop, we fetch the input data, and cast it before saving.
            #ifdef VERBOSE
            mexPrintf("%d: Addr: %d; Data: %d.\n", i, (USHORT) addresses_input_pointer[i], (UCHAR) data_values_input_pointer[i]);
            #endif
            addresses_converted[i] = (USHORT) addresses_input_pointer[i] -1; // off-by-one error: the dongle expects [0-511], reality expects [1-512].
            data_values_converted[i] = (UCHAR) data_values_input_pointer[i];
        }


        USHORT start_address;
        USHORT end_address;
        USHORT no_of_channels;

        // Add the offset.
        if(no_of_elements_address > 1)
        {
            start_address = addresses_converted[0];
            end_address = addresses_converted[no_of_elements_address - 1];
            no_of_channels = end_address - start_address + 1;
        }
        else
        {
            // Special case: only 1 channel argument:
            start_address = addresses_converted[0];
            no_of_channels = 1;
        }


        #ifdef VERBOSE
        mexPrintf("dmx.mex::All sanity checks passed, showing converted address range: %03d - %03d = %d\n", end_address, start_address, no_of_channels);
        #endif

        // Check the work: dmx('inputtest', [100, 101, 102, 103, 104, 105], [255, 255; 255, 255; 0, 0]);

        /*
            The USB transfer stuff
        */


        bool isPresent = FALSE;

        // Open the device, fail if cannot
        if (!LstK_FindByVidPid(deviceList, UDMX_VENDOR_ID, UDMX_PRODUCT_ID, &deviceInfo))
            mexErrMsgTxt("dmx.mex::Could not find the uDMX device.\n");

        // If we didn't die before, then load the driver API
        LibK_LoadDriverAPI(&Usb, deviceInfo->DriverID);

        // Open device.
        if(!Usb.Init(&handle, deviceInfo))
        {
            errorCode = GetLastError();
            mexPrintf("dmx.mex::Error code: %d", errorCode);
            mexErrMsgTxt("dmx.mex::Failed to open device.\n");
        }

        DWORD transferred = 0;
	    BOOL success;
	    WINUSB_SETUP_PACKET Pkt;
	    KUSB_SETUP_PACKET* defPkt = (KUSB_SETUP_PACKET*)&Pkt;

        memset(&Pkt, 0, sizeof(Pkt));
        defPkt->BmRequest.Dir	= 0; // This should be BMREQUEST_DIR_HOST_TO_DEVICE
        defPkt->BmRequest.Type	= 2; // This should be BMREQUEST_TYPE_VENDOR
        defPkt->Request			= (UCHAR) cmd_SetChannelRange;
        defPkt->Value			= (UINT) no_of_channels;
        defPkt->Index			= start_address;
        defPkt->Length			= no_of_channels;


        success = UsbK_ControlTransfer(handle, Pkt, data_values_converted, no_of_channels, &transferred, NULL);
        #ifdef VERBOSE
        mexPrintf("dmx.mex::UsbK_ControlTransfer():\n\tPkt.Value (no_of_channels): %d,\n\tPkt.Index (start_address): %d,\n\tPkt.Length (no_of_channels): %d\n", Pkt.Value, Pkt.Index, Pkt.Length);
        mexPrintf("dmx.mex::Transferred %d Bytes.\n", transferred);
        mexPrintf("dmx.mex::Cleaning up..\n");
        #endif
        // All done, clean up.
        Usb.Free(handle);
        LstK_Free(deviceList);

        plhs[0] = mxCreateLogicalScalar(!success); // fail. :)


    }





}
