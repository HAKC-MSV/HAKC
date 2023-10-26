//
// Created by ja18738 on 9/29/23.
//

#ifndef HAKC_CUSTOMTRANSFER_US_DATA_H
#define HAKC_CUSTOMTRANSFER_US_DATA_H

#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "HAKCFunctionDefinition/SingleFunctionCustomTransfer.h"

namespace hakc {

    class CustomTransfer_us_data : public SingleFunctionCustomTransfer {
    public:
        CustomTransfer_us_data(Module &M, unsigned CompartmentStorageSizeInBits);
    };

} // hakc

#endif //HAKC_CUSTOMTRANSFER_US_DATA_H

