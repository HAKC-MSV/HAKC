//
// Created by de29664 on 3/31/23.
//

#include "HAKCAnalysis/Linux/HAKCFunctionAnalysisLinux.h"

namespace hakc {
    HAKCFunctionAnalysisLinux::HAKCFunctionAnalysisLinux(Function *F) :
            HAKCFunctionAnalysis(F, CommonHAKCAnalysis::getHAKCDebugName() == F->getName()) {

    }

    std::string HAKCFunctionAnalysisLinux::getHAKCFunctionSectionName() {
        std::string sectionName = HAKC_SECTION_PREFIX.str();
        auto *Color = getColor();

        sectionName += HAKCModuleAnalysisLinux::getColorStringFromValue(Color);
        if (getFunction().getSection().empty()) {
            sectionName += ".text";
        } else {
            sectionName += getFunction().getSection().str();
        }
        return sectionName;
    }

    ConstantInt *HAKCFunctionAnalysisLinux::getColor() {
        auto Symbol = getTransformer().getSystemInformation().findSymbol(CurrentFunction);
        if (Symbol) {
            return getTransformer().getInt64(Symbol->getCompartment()->getColor());
        }
        return getTransformer().getInt64(getLinuxModuleAnalysis().GetMajoritySymbolColor());
    }

    bool HAKCFunctionAnalysisLinux::pointerShouldBeChecked(Value *ptr) {
        if (auto *call = dyn_cast<CallInst>(ptr)) {
            if (call->getCalledFunction() &&
                call->getCalledFunction()->getName() == "hakc_safe_ptr") {
                return false;
            }
        }
        return HAKCFunctionAnalysis::pointerShouldBeChecked(ptr);
    }

    bool HAKCFunctionAnalysisLinux::isSafeTransitionFunction(Function *F) {
        auto isSafe = CommonHAKCAnalysis::isSafeTransitionFunction(F);
        return isSafe || F->getName().contains("__lse_atomic_") || F->getName().contains("get_pid_ns") || F->getName().contains("get_user_ns") || F->getName().contains("static_branch_");
    }
} // hakc
