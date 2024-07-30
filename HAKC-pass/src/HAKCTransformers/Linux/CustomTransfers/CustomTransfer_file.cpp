//
// Created by de29664 on 8/7/23.
//

#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_file.h"
#include "HAKC-defs.h"

namespace hakc {
    CustomTransfer_file::CustomTransfer_file(Module &M, unsigned CompartmentStorageSizeInBits) :
            SingleFunctionCustomTransfer(M, CompartmentStorageSizeInBits, "struct.file", "hakc_transfer_file_struct",
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
