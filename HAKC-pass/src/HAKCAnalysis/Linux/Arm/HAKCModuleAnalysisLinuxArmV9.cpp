//
// Created by de29664 on 3/29/23.
//

#include "HAKCAnalysis/Linux/Arm/HAKCFunctionAnalysisLinuxArmV9.h"
#include "HAKCTransformers/Linux/Arm/HAKCTransformerLinuxArmV9.h"
#include "HAKCAnalysis/Linux/Arm/HAKCModuleAnalysisLinuxArmV9.h"


namespace hakc {
    HAKCModuleAnalysisLinuxArmV9::HAKCModuleAnalysisLinuxArmV9(Module &M) :
            HAKCModuleAnalysisLinuxArm(M) {}

    HAKCFunctionAnalysis *
    HAKCModuleAnalysisLinuxArmV9::GetFunctionTransformation(Function *F, HAKCCompartmentalizationPolicy &Policy) {
        return new HAKCFunctionAnalysisLinuxArmV9(F, Policy, this);
    }

    std::shared_ptr<HAKCTransformer>
    HAKCModuleAnalysisLinuxArmV9::CreateTransformer(HAKCCompartmentalizationPolicy &Policy) {
        return std::make_shared<HAKCTransformerLinuxArmV9>(Policy, *this);
    }

    std::set<StringRef> HAKCModuleAnalysisLinuxArmV9::GetHAKCSourcePaths() {
        StringRef extras[] = {
                /* current paths for ARMv9 HAKC */
                "arch/arm64/kernel/hakc/armv9/hakc_pac_mte.c",
                /* legacy paths for ARMv9 HAKC */
                "arch/arm64/kernel/hakc/armv9/hakc.c"
        };

        /* legacy + current paths for common HAKC code */
        auto Paths = HAKCModuleAnalysisLinux::GetHAKCSourcePaths();
        return AddToSet(Paths, extras);
    }
} // hakc
