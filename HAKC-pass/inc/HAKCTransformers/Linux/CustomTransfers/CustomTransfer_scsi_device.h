//
// Created by ja18738 on 10/10/23.
//

#ifndef HAKC_CUSTOMTRANSFER_SCSI_DEVICE_H
#define HAKC_CUSTOMTRANSFER_SCSI_DEVICE_H

#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "HAKCFunctionDefinition/SingleFunctionCustomTransfer.h"

namespace hakc {

    class CustomTransfer_scsi_device : public SingleFunctionCustomTransfer {
    public:
        CustomTransfer_scsi_device(Module &M, unsigned CompartmentStorageSizeInBits);
    };

} // hakc

#endif //HAKC_CUSTOMTRANSFER_SCSI_DEVICE_H
