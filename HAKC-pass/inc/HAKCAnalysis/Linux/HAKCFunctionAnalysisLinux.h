//
// Created by de29664 on 3/31/23.
//

#ifndef HAKC_HAKCFUNCTIONANALYSISLINUX_H
#define HAKC_HAKCFUNCTIONANALYSISLINUX_H

#include "HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "HAKCModuleAnalysisLinux.h"

namespace hakc {

    class HAKCFunctionAnalysisLinux : public HAKCFunctionAnalysis {
    public:
        HAKCFunctionAnalysisLinux(Function *F);

    protected:
        std::string getHAKCFunctionSectionName() override;

        ConstantInt *getColor();

        bool pointerShouldBeChecked(Value *ptr) override;

        virtual HAKCModuleAnalysisLinux &getLinuxModuleAnalysis() = 0;

        bool isSafeTransitionFunction(Function *F) override;
    };

} // hakc

#endif //HAKC_HAKCFUNCTIONANALYSISLINUX_H
