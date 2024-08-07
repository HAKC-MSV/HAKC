//
// Created by de29664 on 4/4/23.
//

#ifndef HAKC_HAKCMODULEANALYSISARMV8_H
#define HAKC_HAKCMODULEANALYSISARMV8_H

#include "HAKCModuleAnalysisLinuxArm.h"

namespace hakc {

    class HAKCModuleAnalysisLinuxArmV8 : public HAKCModuleAnalysisLinuxArm {
    public:
        HAKCModuleAnalysisLinuxArmV8(Module &M);

        std::set<StringRef> GetHAKCSourcePaths() override;

    protected:
        HAKCFunctionAnalysis *GetFunctionTransformation(Function *F) override;

        std::shared_ptr<HAKCTransformer> CreateTransformer(HAKCCompartmentalizationPolicy &Policy) override;
    };

} // hakc

#endif //HAKC_HAKCMODULEANALYSISARMV8_H
