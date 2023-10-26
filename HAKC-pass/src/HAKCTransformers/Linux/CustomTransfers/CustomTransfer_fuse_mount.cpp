//
// Created by ja18738 on 9/15/23.
//

#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_fuse_mount.h"

namespace hakc {
    CustomTransfer_fuse_mount::CustomTransfer_fuse_mount(Module &M, unsigned CompartmentStorageSizeInBits) :
            SingleFunctionCustomTransfer(M, CompartmentStorageSizeInBits, "struct.fuse_mount", "hakc_transfer_fuse_mount",
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
