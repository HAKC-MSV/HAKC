//
// Created by de29664 on 3/31/23.
//

#include "HAKCAnalysis/Linux/HAKCFunctionAnalysisLinux.h"

namespace hakc {
    HAKCFunctionAnalysisLinux::HAKCFunctionAnalysisLinux(Function *F, HAKCCompartmentalizationPolicy &Policy) :
            HAKCFunctionAnalysis(F, Policy, CommonHAKCAnalysis::getHAKCDebugName() == F->getName()) {

    }

    std::string HAKCFunctionAnalysisLinux::getHAKCFunctionSectionName(HAKCCompartmentalizationPolicy &Policy) {
        std::string sectionName = HAKC_SECTION_PREFIX.str();
        auto *Color = getDivision(Policy);

        sectionName += HAKCModuleAnalysisLinux::getColorStringFromValue(Color);
        if (getFunction().getSection().empty()) {
            sectionName += ".text";
        } else {
            sectionName += getFunction().getSection().str();
        }
        return sectionName;
    }

    HAKC_Division_ID HAKCFunctionAnalysisLinux::getDivision(HAKCCompartmentalizationPolicy &Policy) {
        return Policy.GetDivisionID(CurrentFunction);
    }

    std::set<StringRef> HAKCFunctionAnalysisLinux::GetSafePointerFunctionNames() {
        return {
                HAKCFunctionAnalysisLinux::SafePointerName,
        };
    }

    bool HAKCFunctionAnalysisLinux::isSafeTransitionFunction(Function *F) {
        auto isSafe = CommonHAKCAnalysis::isSafeTransitionFunction(F);
        std::set<StringRef> ExcludedFunctions = {
                "__lse_atomic_",
                "get_pid_ns",
                "get_user_ns",
                "static_branch_",
        };
        for (auto ExcludedName: ExcludedFunctions) {
            if (F->getName().contains(ExcludedName)) {
                isSafe = true;
                break;
            }
        }

        return isSafe;
    }

    void
    HAKCFunctionAnalysisLinux::UpdateHAKCFunctionParameters_Arch(CallInst *CallI, HAKCCompartment &TargetCompartment,
                                                                 hakc_transfer_def_t &HAKCTransferFunction,
                                                                 HAKCCompartmentalizationPolicy &Policy) {
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Setting "
                                            << *CallI->getArgOperand(HAKCTransferFunction->GetCompartmentIdIdx())
                                            << " to be " << *TargetCompartment.GetCompartmentID() << "\n";
        }
        CallI->setOperand(HAKCTransferFunction->GetCompartmentIdIdx(), TargetCompartment.GetCompartmentID());

        if (HAKCTransferFunction->HasDivisionIdx()) {
            auto *F = CallI->getFunction();
            HAKC_Division_ID Division;
            if (isOutsideTransferFunc(F)) {
                auto transferTargetName = F->getName().substr(OUTSIDE_TRANSFER_PREFIX.size());
                auto *TransferTarget = F->getParent()->getFunction(transferTargetName);
                Division = Policy.GetDivisionID(TransferTarget);
            } else {
                Division = Policy.GetDivisionID(F);
            }

            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Setting argument " << HAKCTransferFunction->GetDivisionIdx()
                                                << " to be " << *Division << "\n";
            }
            CallI->setOperand(HAKCTransferFunction->GetDivisionIdx(), Division);
        }
    }

    StringRef HAKCFunctionAnalysisLinux::SafePointerName = "hakc_safe_ptr";
} // hakc
