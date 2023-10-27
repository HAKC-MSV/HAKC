//
// Created by de29664 on 4/4/23.
//

#ifndef HAKC_HAKCFUNCTIONANALYSISLINUXX86_H
#define HAKC_HAKCFUNCTIONANALYSISLINUXX86_H

#include "HAKCAnalysis/Linux/HAKCFunctionAnalysisLinux.h"
#include "HAKCModuleAnalysisLinuxX86.h"

namespace hakc {

    class HAKCFunctionAnalysisLinuxX86 : public HAKCFunctionAnalysisLinux {
    public:
        HAKCFunctionAnalysisLinuxX86(Function *F, HAKCModuleAnalysisLinuxX86 *ModAnalysis);

    protected:
        HAKCModuleAnalysisLinuxX86 *ModAnalysis;

        HAKCModuleAnalysis &getModuleAnalysis() override;

        HAKCModuleAnalysisLinux &getLinuxModuleAnalysis() override;
    };

} // hakc

#endif //HAKC_HAKCFUNCTIONANALYSISLINUXX86_H
