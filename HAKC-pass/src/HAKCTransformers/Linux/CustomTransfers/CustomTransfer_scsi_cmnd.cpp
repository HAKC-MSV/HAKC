//
// Created by ja18738 on 9/29/23.
//

#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_scsi_cmnd.h"
#include "HAKC-defs.h"

namespace hakc {
    CustomTransfer_scsi_cmnd::CustomTransfer_scsi_cmnd(Module &M, unsigned CompartmentStorageSizeInBits) :
            SingleFunctionCustomTransfer(M, CompartmentStorageSizeInBits, "struct.scsi_cmnd", "hakc_transfer_scsi_cmnd",
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
