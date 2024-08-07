//
// Created by de29664 on 3/29/23.
//

#ifndef HAKC_HAKCMODULETRANFORMATIONARMV9_H
#define HAKC_HAKCMODULETRANFORMATIONARMV9_H

#include "HAKCModuleAnalysisLinuxArm.h"

namespace hakc {

    class HAKCModuleAnalysisLinuxArmV9 : public HAKCModuleAnalysisLinuxArm {
    public:
        HAKCModuleAnalysisLinuxArmV9(Module &M);

        std::set<StringRef> GetHAKCSourcePaths() override;

    protected:
        HAKCFunctionAnalysis *GetFunctionTransformation(Function *F) override;

        std::shared_ptr<HAKCTransformer> CreateTransformer(HAKCCompartmentalizationPolicy &Policy) override;
    };

} // hakc

#endif //HAKC_HAKCMODULETRANFORMATIONARMV9_H
