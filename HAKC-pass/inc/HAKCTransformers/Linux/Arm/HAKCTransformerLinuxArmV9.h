//
// Created by de29664 on 4/4/23.
//

#ifndef HAKC_HAKCTRANSFORMERLINUXARMV9_H
#define HAKC_HAKCTRANSFORMERLINUXARMV9_H

#include "HAKCTransformers/Linux/HAKCTransformerLinux.h"
#include "HAKCAnalysis/Linux/Arm/HAKCModuleAnalysisLinuxArmV9.h"

namespace hakc {

    class HAKCTransformerLinuxArmV9 : public HAKCTransformerLinux {
    public:
        HAKCTransformerLinuxArmV9(Module &Module, HAKCModuleAnalysisLinuxArmV9 *ModAnalysis);
    };

} // hakc

#endif //HAKC_HAKCTRANSFORMERLINUXARMV9_H
