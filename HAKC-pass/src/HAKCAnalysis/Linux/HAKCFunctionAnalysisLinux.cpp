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

    void HAKCFunctionAnalysisLinux::UpdateHAKCFunctionParameters_Arch(CallInst *CallI, hakc_compartment_id_t TargetID,
                                                                      hakc_transfer_def_t &HAKCTransferFunction) {
        auto *CompartmentIDValue = getTransformer().GetHAKCCompartmentValue(TargetID);
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Setting ";
            CallI->getArgOperand(HAKCTransferFunction->GetCompartmentIdIdx())->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " to be ";
            CompartmentIDValue->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }
        CallI->setOperand(HAKCTransferFunction->GetCompartmentIdIdx(), CompartmentIDValue);

        if (HAKCTransferFunction->HasDivisionIdx()) {
            auto *F = CallI->getFunction();
            ConstantInt *color;
            if (isOutsideTransferFunc(F)) {
                auto transferTargetName = F->getName().substr(OUTSIDE_TRANSFER_PREFIX.size());
                auto *TransferTarget = getModule().getFunction(transferTargetName);
                color = getLinuxModuleAnalysis().getFunctionColor(TransferTarget);
            } else {
                color = getLinuxModuleAnalysis().getFunctionColor(F);
            }

            if (!color) {
                CommonHAKCAnalysis::getWriter() << "Could not find DivisionID for function " << F->getName()
                                                << "\n";
                throw std::exception();
            }
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Setting argument " << HAKCTransferFunction->GetDivisionIdx()
                                                << " to be ";
                color->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            CallI->setOperand(HAKCTransferFunction->GetDivisionIdx(), color);
        }
    }

    StringRef HAKCFunctionAnalysisLinux::SafePointerName = "hakc_safe_ptr";
} // hakc
