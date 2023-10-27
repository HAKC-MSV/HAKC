//
// Created by ja18738 on 9/29/23.
//

#ifndef HAKC_CUSTOMTRANSFER_SCSI_CMND_H
#define HAKC_CUSTOMTRANSFER_SCSI_CMND_H

#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "HAKCFunctionDefinition/SingleFunctionCustomTransfer.h"

namespace hakc {

    class CustomTransfer_scsi_cmnd : public SingleFunctionCustomTransfer {
    public:
        CustomTransfer_scsi_cmnd(Module &M, unsigned CompartmentStorageSizeInBits);
    };

} // hakc

#endif //HAKC_CUSTOMTRANSFER_SCSI_CMND_H
