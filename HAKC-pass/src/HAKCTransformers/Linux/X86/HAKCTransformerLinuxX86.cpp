//
// Created by de29664 on 4/4/23.
//

#include "HAKCAnalysis/Linux/X86/HAKCModuleAnalysisLinuxX86.h"
#include "HAKCTransformers/Linux/X86/HAKCTransformerLinuxX86.h"

namespace hakc {
    HAKCTransformerLinuxX86::HAKCTransformerLinuxX86(HAKCCompartmentalizationPolicy &Policy,
                                                     HAKCModuleAnalysisLinuxX86 &ModAnalysis) : HAKCTransformerLinux(
            Policy, ModAnalysis) {

    }

} // hakc
