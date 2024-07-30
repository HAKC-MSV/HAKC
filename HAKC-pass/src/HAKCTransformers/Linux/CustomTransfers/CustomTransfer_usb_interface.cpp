//
// Created by ja18738 on 9/29/23.
//

#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_usb_interface.h"
#include "HAKC-defs.h"

namespace hakc {
    CustomTransfer_usb_interface::CustomTransfer_usb_interface(Module &M, unsigned CompartmentStorageSizeInBits) :
            SingleFunctionCustomTransfer(M, CompartmentStorageSizeInBits, "struct.usb_interface",
                                         "hakc_transfer_usb_interface",
                                         nullptr,
                                         {
                                                 nullptr,
                                                 IntegerType::get(M.getContext(),
                                                                  COMPARTMENT_ID_BIT_LENGTH),
                                                 IntegerType::get(
                                                         M.getContext(), CLIQUE_COLOR_BIT_LENGTH)
                                         }, 0, 1, 2) {

    }
} // hakc
