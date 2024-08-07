//
// Created by de29664 on 4/4/23.
//

#include "HAKCTransformers/Linux/Arm/HAKCTransformerLinuxArmV8.h"

namespace hakc {
    HAKCTransformerLinuxArmV8::HAKCTransformerLinuxArmV8(HAKCCompartmentalizationPolicy &Policy,
                                                         HAKCModuleAnalysisLinuxArmV8 &ModAnalysis)
            : HAKCTransformerLinux(Policy, ModAnalysis) {

    }
} // hakc
