/*
* FreeplayTech UHID gamepad driver
* HID descriptor header
*
* Note:
* Whole HID descriptor is defined, even without specific things like ADCs, Dpad or misc buttons enabled.
* It is done that way because some emulator grab inputs from Event device instead of Joystick device.
* Changing anything here can shift/change reported buttons/axis "index" numbers and possibly mess with some emulators input configuration. 
*/

//partially based on https://github.com/NicoHood/HID/blob/master/src/MultiReport/Gamepad.cpp
//reference : https://elixir.bootlin.com/linux/latest/source/include/linux/hid.h

#pragma once

unsigned char hid_descriptor[] = {
    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0x09, 0x05, // USAGE (Gamepad)
    0xA1, 0x01, // COLLECTION (Application)
        //Digital
        0x05, 0x09, // USAGE_PAGE (Button)
        0x19, 0x01, // USAGE_MINIMUM (Button 1)
        0x29, uhid_buttons_count, // USAGE_MAXIMUM, uhid_buttons_count defined in driver_config.h
        0x15, 0x00, // LOGICAL_MINIMUM (0)
        0x25, 0x01, // LOGICAL_MAXIMUM (1)
        0x75, 0x01, // REPORT_SIZE (1)
        0x95, 0x10, // REPORT_COUNT (16)
        0x81, 0x02, // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

        //Dpad (Hat0X-Y)
        /*
        Important note:
        Multiple ways can be used to declare a Dpad.
        As of 2021-12-25, HID_GD_UP (0x90), HID_GD_DOWN (0x91), HID_GD_RIGHT (0x92), HID_GD_LEFT (0x93) way is broken.
        For reference: 
        If HID descriptor has DPAD UP/DOWN/LEFT/RIGHT HID usages and each of usage size is 1 bit,
        then only the first one will generate input event, the rest ofthe HID usages will be assigned to hat direction only.
        https://patchwork.kernel.org/project/linux-input/patch/20201101193504.679934-1-lzye@google.com/
        */
        0x05, 0x01, // USAGE_PAGE (Generic Desktop)
        0x09, 0x39, // USAGE (Hat switch)
        //0x09, 0x39, // USAGE (Hat switch)
        0x15, 0x01, // LOGICAL_MINIMUM (1)
        0x25, 0x08, // LOGICAL_MAXIMUM (8)
        0x75, 0x04, // REPORT_SIZE (4)
        0x95, 0x02, // REPORT_COUNT (2), needs one, 2 here to avoid padding 4bits
        0x81, 0x02, // INPUT (Data,Var,Abs)

        //4x 16bits axis
        0x05, 0x01, // USAGE_PAGE (Generic Desktop)
        0xa1, 0x00, // COLLECTION (Physical)
            0x09, 0x30, // USAGE (X)
            0x09, 0x31, // USAGE (Y)
            0x09, 0x33, // USAGE (Rx)
            0x09, 0x34, // USAGE (Ry)
            0x15, 0x00, // LOGICAL_MINIMUM (0)
            0x26, 0xFF, 0xFF, // LOGICAL_MAXIMUM (0xFFFF)
            0x75, 0x10, // REPORT_SIZE (16)
            0x95, 0x04, // REPORT_COUNT (4)
            0x81, 0x02, // INPUT (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0xc0, // END_COLLECTION

        //Misc digital input when ADC0,1,2,3 not used
        0x09, 0x08, // USAGE (Multiaxis)
        0xA1, 0x01, // COLLECTION (Application)
            0x05, 0x09, // USAGE_PAGE (Button)
            0x19, 0x01, // USAGE_MINIMUM (Button 1)
            0x29, uhid_buttons_misc_count, // USAGE_MAXIMUM, uhid_buttons_misc_count defined in driver_config.h
            0x15, 0x00, // LOGICAL_MINIMUM (0)
            0x25, 0x01, // LOGICAL_MAXIMUM (1)
            0x75, 0x01, // REPORT_SIZE (1)
            0x95, 0x08, // REPORT_COUNT (8)
            0x81, 0x02, // INPUT (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0xc0, // END_COLLECTION
    0xC0, // END_COLLECTION
};
