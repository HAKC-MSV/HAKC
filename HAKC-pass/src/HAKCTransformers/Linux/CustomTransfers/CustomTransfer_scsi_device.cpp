//
// Created by ja18738 on 10/10/23.
//

#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_scsi_device.h"
#include "HAKC-defs.h"

namespace hakc {
    CustomTransfer_scsi_device::CustomTransfer_scsi_device(Module &M, unsigned CompartmentStorageSizeInBits) :
            SingleFunctionCustomTransfer(M, CompartmentStorageSizeInBits, "struct.scsi_device",
                                         "hakc_transfer_scsi_device",
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
