//
// Created by ja18738 on 9/29/23.
//

#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_usb_device.h"
#include "HAKC-defs.h"

namespace hakc {
    CustomTransfer_usb_device::CustomTransfer_usb_device(Module &M, unsigned CompartmentStorageSizeInBits) :
            SingleFunctionCustomTransfer(M, CompartmentStorageSizeInBits, "struct.usb_device",
                                         "hakc_transfer_usb_device",
                                         nullptr,
                                         {
                                                 nullptr,
                                                 IntegerType::get(M.getContext(),
                                                                  COMPARTMENT_ID_BIT_LENGTH),
                                                 IntegerType::get(
                                                         M.getContext(), DIVISION_ID_BIT_LENGTH)
                                         }, 0, 1, 2) {

    }
} // hakc
