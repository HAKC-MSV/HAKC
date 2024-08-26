//
// Created by de29664 on 8/7/23.
//

#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_socket.h"
#include "HAKC-defs.h"

namespace hakc {
    CustomTransfer_socket::CustomTransfer_socket(Module &M, unsigned CompartmentStorageSizeInBits) :
            SingleFunctionCustomTransfer(M, CompartmentStorageSizeInBits, "struct.socket", "hakc_transfer_socket",
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
