//
// Created by de29664 on 6/22/23.
//

#ifndef HAKC_CUSTOMTRANSFER_SK_BUFF_H
#define HAKC_CUSTOMTRANSFER_SK_BUFF_H

#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "HAKCFunctionDefinition/SingleFunctionCustomTransfer.h"

namespace hakc {

    class CustomTransfer_sk_buff : public SingleFunctionCustomTransfer {
    public:
        CustomTransfer_sk_buff(Module &M, unsigned CompartmentStorageSizeInBits);
    };

} // hakc

#endif //HAKC_CUSTOMTRANSFER_SK_BUFF_H
