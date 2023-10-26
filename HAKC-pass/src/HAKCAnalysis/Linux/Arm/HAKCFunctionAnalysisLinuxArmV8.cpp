//
// Created by de29664 on 4/4/23.
//

#include "HAKCAnalysis/Linux/Arm/HAKCFunctionAnalysisLinuxArmV8.h"

namespace hakc {
    HAKCFunctionAnalysisLinuxArmV8::HAKCFunctionAnalysisLinuxArmV8(Function *F,
                                                                   HAKCModuleAnalysisLinuxArmV8 *ModAnalysis) :
            HAKCFunctionAnalysisLinux(F),
            ModAnalysis(ModAnalysis) {

    }

    HAKCModuleAnalysis &HAKCFunctionAnalysisLinuxArmV8::getModuleAnalysis() {
        return *ModAnalysis;
    }

    HAKCModuleAnalysisLinux &HAKCFunctionAnalysisLinuxArmV8::getLinuxModuleAnalysis() {
        return *ModAnalysis;
    }
} // hakc