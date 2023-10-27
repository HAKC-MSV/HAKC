//
// Created by de29664 on 8/7/23.
//

#ifndef HAKC_CUSTOMTRANSFER_FILE_H
#define HAKC_CUSTOMTRANSFER_FILE_H

#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "HAKCFunctionDefinition/SingleFunctionCustomTransfer.h"

namespace hakc {

    class CustomTransfer_file : public SingleFunctionCustomTransfer {
    public:
        CustomTransfer_file(Module &M, unsigned CompartmentStorageSizeInBits);
    };

} // hakc

#endif //HAKC_CUSTOMTRANSFER_FILE_H
