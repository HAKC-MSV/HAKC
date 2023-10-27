//
// Created by de29664 on 6/22/23.
//

#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_sk_buff.h"

namespace hakc {
    CustomTransfer_sk_buff::CustomTransfer_sk_buff(Module &M, unsigned CompartmentStorageSizeInBits) :
            SingleFunctionCustomTransfer(M, CompartmentStorageSizeInBits, "struct.sk_buff", "hakc_transfer_skb",
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