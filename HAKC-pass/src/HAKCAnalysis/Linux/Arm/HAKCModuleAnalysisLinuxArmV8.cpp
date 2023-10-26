//
// Created by de29664 on 4/4/23.
//

#include "HAKCAnalysis/Linux/Arm/HAKCFunctionAnalysisLinuxArmV8.h"
#include "HAKCTransformers/Linux/Arm/HAKCTransformerLinuxArmV8.h"
#include "HAKCAnalysis/Linux/Arm/HAKCModuleAnalysisLinuxArmV8.h"


namespace hakc {
    HAKCModuleAnalysisLinuxArmV8::HAKCModuleAnalysisLinuxArmV8(Module &M) :
            HAKCModuleAnalysisLinuxArm(M) {}

    HAKCFunctionAnalysis *HAKCModuleAnalysisLinuxArmV8::GetFunctionTransformation(Function *F) {
        return new HAKCFunctionAnalysisLinuxArmV8(F, this);
    }

    std::shared_ptr<HAKCTransformer> HAKCModuleAnalysisLinuxArmV8::CreateTransformer() {
        return std::make_shared<HAKCTransformerLinuxArmV8>(M, this);
    }

    std::set<StringRef> HAKCModuleAnalysisLinuxArmV8::GetHAKCSourcePaths() {
        StringRef extras[] = {
                /* current paths for ARMv8 HAKC */
                "arch/arm64/kernel/hakc/armv8/hakc_neon.c",
                /* legacy paths for ARMv8 HAKC */
                "arch/arm64/kernel/hakc/armv8/hakc.c",
                "arch/arm64/kernel/hakc/armv8/hakc_btree.c",
                "arch/arm64/kernel/hakc/armv8/hakc_memory.c",
        };

        /* legacy + current paths for common HAKC code */
        auto Paths = HAKCModuleAnalysisLinux::GetHAKCSourcePaths();
        return AddToSet(Paths, extras);
    }
} // hakc
