//
// Created by de29664 on 3/29/23.
//

#ifndef HAKC_HAKCFUNCTIONTRANSFORMATIONARMV8_H
#define HAKC_HAKCFUNCTIONTRANSFORMATIONARMV8_H

#include "HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "HAKCAnalysis/Linux/HAKCFunctionAnalysisLinux.h"
#include "HAKCModuleAnalysisLinuxArmV9.h"

namespace hakc {

    class HAKCFunctionAnalysisLinuxArmV9 : public HAKCFunctionAnalysisLinux {
    public:
        HAKCFunctionAnalysisLinuxArmV9(Function *F, HAKCModuleAnalysisLinuxArmV9 *ModAnalysis);

    protected:
        HAKCModuleAnalysisLinuxArmV9 *ModAnalysis;

        HAKCModuleAnalysis &getModuleAnalysis() override;

        HAKCModuleAnalysisLinux &getLinuxModuleAnalysis() override;

    };

} // hakc

#endif //HAKC_HAKCFUNCTIONTRANSFORMATIONARMV8_H
