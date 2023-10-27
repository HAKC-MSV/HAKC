//
// Created by de29664 on 4/4/23.
//

#include "HAKCAnalysis/Linux/X86/HAKCFunctionAnalysisLinuxX86.h"

namespace hakc {
    HAKCFunctionAnalysisLinuxX86::HAKCFunctionAnalysisLinuxX86(Function *F, HAKCModuleAnalysisLinuxX86 *ModAnalysis)
            : HAKCFunctionAnalysisLinux(F),
              ModAnalysis(ModAnalysis) {

    }

    HAKCModuleAnalysis &HAKCFunctionAnalysisLinuxX86::getModuleAnalysis() {
        return *ModAnalysis;
    }

    HAKCModuleAnalysisLinux &HAKCFunctionAnalysisLinuxX86::getLinuxModuleAnalysis() {
        return *ModAnalysis;
    }
} // hakc