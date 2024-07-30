//
// Created by ge31287 on 04/21/24.
//

#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_file_ops.h"
#include "HAKC-defs.h"

namespace hakc {
    CustomTransfer_file_ops::CustomTransfer_file_ops(Module &M, unsigned CompartmentStorageSizeInBits) :
            SingleFunctionCustomTransfer(M, CompartmentStorageSizeInBits, "struct.file_operations",
                                         "hakc_transfer_file_ops",
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
