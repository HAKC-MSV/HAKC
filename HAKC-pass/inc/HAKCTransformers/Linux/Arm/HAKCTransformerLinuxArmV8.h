//
// Created by de29664 on 4/4/23.
//

#ifndef HAKC_HAKCTRANSFORMERLINUXARMV8_H
#define HAKC_HAKCTRANSFORMERLINUXARMV8_H

#include "HAKCTransformers/Linux/HAKCTransformerLinux.h"
#include "HAKCAnalysis/Linux/Arm/HAKCModuleAnalysisLinuxArmV8.h"

namespace hakc {

    class HAKCTransformerLinuxArmV8 : public HAKCTransformerLinux {
    public:
        HAKCTransformerLinuxArmV8(Module &Module, HAKCModuleAnalysisLinuxArmV8 *ModAnalysis);
    };

} // hakc

#endif //HAKC_HAKCTRANSFORMERLINUXARMV8_H
