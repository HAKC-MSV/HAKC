//
// Created by ja18738 on 9/29/23.
//

#ifndef HAKC_CUSTOMTRANSFER_USB_DEVICE_H
#define HAKC_CUSTOMTRANSFER_USB_DEVICE_H

#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "HAKCFunctionDefinition/SingleFunctionCustomTransfer.h"

namespace hakc {

    class CustomTransfer_usb_device : public SingleFunctionCustomTransfer {
    public:
        CustomTransfer_usb_device(Module &M, unsigned CompartmentStorageSizeInBits);
    };

} // hakc

#endif //HAKC_CUSTOMTRANSFER_USB_DEVICE_H

