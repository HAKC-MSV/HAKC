//
// Created by de29664 on 3/29/23.
//

#include "HAKCAnalysis/Linux/Arm/HAKCFunctionAnalysisLinuxArmV9.h"

namespace hakc {
    HAKCFunctionAnalysisLinuxArmV9::HAKCFunctionAnalysisLinuxArmV9(Function *F,
                                                                   HAKCModuleAnalysisLinuxArmV9 *ModAnalysis) :
            HAKCFunctionAnalysisLinux(F),
            ModAnalysis(ModAnalysis) {

    }

    HAKCModuleAnalysis &HAKCFunctionAnalysisLinuxArmV9::getModuleAnalysis() {
        return *ModAnalysis;
    }

    HAKCModuleAnalysisLinux &HAKCFunctionAnalysisLinuxArmV9::getLinuxModuleAnalysis() {
        return *ModAnalysis;
    }
} // hakc