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

        static StringRef SafePointerName;

    protected:
        std::string getHAKCFunctionSectionName() override;

        ConstantInt *getColor();

        virtual HAKCModuleAnalysisLinux &getLinuxModuleAnalysis() = 0;

        bool isSafeTransitionFunction(Function *F) override;

        std::set<StringRef> GetSafePointerFunctionNames() override;

        void UpdateHAKCFunctionParameters_Arch(CallInst *CallI, hakc_compartment_id_t TargetID, hakc_transfer_def_t
        &HAKCTransferFunction) override;
    };

} // hakc

#endif //HAKC_HAKCFUNCTIONANALYSISLINUX_H
