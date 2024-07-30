//
// Created by ja18738 on 9/29/23.
//

#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_us_data.h"
#include "HAKC-defs.h"

namespace hakc {
    CustomTransfer_us_data::CustomTransfer_us_data(Module &M, unsigned CompartmentStorageSizeInBits) :
            SingleFunctionCustomTransfer(M, CompartmentStorageSizeInBits, "struct.us_data", "hakc_transfer_us_data",
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
