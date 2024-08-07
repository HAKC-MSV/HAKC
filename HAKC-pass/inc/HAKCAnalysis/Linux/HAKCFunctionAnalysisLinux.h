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
        explicit HAKCFunctionAnalysisLinux(Function *F, HAKCCompartmentalizationPolicy &Policy);

        static StringRef SafePointerName;

    protected:
        std::string getHAKCFunctionSectionName(HAKCCompartmentalizationPolicy &Policy) override;

        HAKC_Division_ID getDivision(HAKCCompartmentalizationPolicy &Policy);

        virtual HAKCModuleAnalysisLinux &getLinuxModuleAnalysis() = 0;

        bool isSafeTransitionFunction(Function *F) override;

        std::set<StringRef> GetSafePointerFunctionNames() override;

        void UpdateHAKCFunctionParameters_Arch(CallInst *CallI, HAKCCompartment &TargetCompartment,
                                               hakc_transfer_def_t &HAKCTransferFunction,
                                               HAKCCompartmentalizationPolicy &Policy) override;
    };

} // hakc

#endif //HAKC_HAKCFUNCTIONANALYSISLINUX_H
