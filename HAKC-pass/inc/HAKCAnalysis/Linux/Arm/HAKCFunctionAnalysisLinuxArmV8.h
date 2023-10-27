//
// Created by de29664 on 4/4/23.
//

#ifndef HAKC_HAKCFUNCTIONANALYSISARMV8_H
#define HAKC_HAKCFUNCTIONANALYSISARMV8_H

#include "HAKCAnalysis/Linux/HAKCFunctionAnalysisLinux.h"
#include "HAKCModuleAnalysisLinuxArmV8.h"

namespace hakc {

    class HAKCFunctionAnalysisLinuxArmV8 : public HAKCFunctionAnalysisLinux {
    public:
        HAKCFunctionAnalysisLinuxArmV8(Function *F, HAKCModuleAnalysisLinuxArmV8 *ModAnalysis);

    protected:
        HAKCModuleAnalysisLinuxArmV8 *ModAnalysis;

        HAKCModuleAnalysis &getModuleAnalysis() override;

        HAKCModuleAnalysisLinux &getLinuxModuleAnalysis() override;
    };

} // hakc

#endif //HAKC_HAKCFUNCTIONANALYSISARMV8_H
