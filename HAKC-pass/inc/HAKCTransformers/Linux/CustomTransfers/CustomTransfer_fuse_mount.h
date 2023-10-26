//
// Created by ja18738 on 9/15/23.
//

#ifndef HAKC_CUSTOMTRANSFER_FUSE_MOUNT_H
#define HAKC_CUSTOMTRANSFER_FUSE_MOUNT_H

#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "HAKCFunctionDefinition/SingleFunctionCustomTransfer.h"

namespace hakc {

    class CustomTransfer_fuse_mount : public SingleFunctionCustomTransfer {
    public:
        CustomTransfer_fuse_mount(Module &M, unsigned CompartmentStorageSizeInBits);
    };

} // hakc

#endif //HAKC_CUSTOMTRANSFER_FUSE_MOUNT_H
