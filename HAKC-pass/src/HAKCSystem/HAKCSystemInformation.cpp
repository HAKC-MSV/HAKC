//
// Created by de29664 on 11/7/24.
//

#include "HAKCSystem/HAKCSystemInformation.h"

namespace hakc {

    HAKCSystemInformation::HAKCSystemInformation(Module &M) : M(M), Arch(), Platform(), Database(),
                                                              NoTransferFunctionList(),
                                                              CompartmentTransferFunctionList(),
                                                              CompartmentalizationValidationFunctionList(),
                                                              CompartmentalizationSupportFunctionList(),
                                                              SeparateNamespacePaths(), HAKCSourcePaths(),
                                                              SafeTransitionFunctionList(), IgnoredTypeSet(),
                                                              IgnoredGlobalList(), AllocationSizeMap() {

    }

    void operator<<(HAKCSystemInformation &HAKCSystemInfo, HAKCYamlConfig &YamlConfig) {
        HAKCSystemInfo.Arch = YamlConfig.Arch;
        HAKCSystemInfo.Platform = YamlConfig.Platform;
        HAKCSystemInfo.Database = YamlConfig.Database;

        for (auto &FunctionName: YamlConfig.NoTransferFunctions) {
            auto *F = HAKCSystemInfo.M.getFunction(FunctionName);
            if (F) {
                HAKCSystemInfo.NoTransferFunctionList.push_back(F);
            }
        }

        HAKCSystemInfo.SeparateNamespacePaths.insert(YamlConfig.SeparateNamespacePaths.begin(),
                                                     YamlConfig.SeparateNamespacePaths.end());
        HAKCSystemInfo.HAKCSourcePaths.insert(YamlConfig.HAKCSourcePaths.begin(), YamlConfig.HAKCSourcePaths.end());

        for (auto &FunctionName: YamlConfig.SafeTransitionFunctions) {
            auto *F = HAKCSystemInfo.M.getFunction(FunctionName);
            if (F) {
                HAKCSystemInfo.SafeTransitionFunctionList.push_back(F);
            }
        }

        for (auto &TransferEntry: YamlConfig.CompartmentTransferFunctions) {
            auto *F = HAKCSystemInfo.M.getFunction(TransferEntry.FunctionName);
            if (F) {
                auto Transfer = std::make_shared<HAKCTransferFunction>(F, TransferEntry.PointerIdx,
                                                                       TransferEntry.CompartmentIdx,
                                                                       TransferEntry.DivisionIdx,
                                                                       TransferEntry.SizeIdx);
                HAKCSystemInfo.CompartmentTransferFunctionList.push_back(Transfer);
            }
        }

        for (auto &FunctionName: YamlConfig.CompartmentalizationValidationFunctions) {
            auto *F = HAKCSystemInfo.M.getFunction(FunctionName);
            if (F) {
                auto Validation = std::make_shared<HAKCFunctionDefinition>(F);
                HAKCSystemInfo.CompartmentalizationValidationFunctionList.push_back(Validation);
            }
        }

        for (auto &FunctionName: YamlConfig.CompartmentalizationSupportFunctions) {
            auto *F = HAKCSystemInfo.M.getFunction(FunctionName);
            if (F) {
                HAKCSystemInfo.CompartmentalizationSupportFunctionList.push_back(F);
            }
        }

        for (auto &TypeName: YamlConfig.IgnoredTypes) {
            auto *Ty = StructType::getTypeByName(HAKCSystemInfo.M.getContext(), TypeName);
            if (Ty) {
                HAKCSystemInfo.IgnoredTypeSet.insert(Ty);
            }
        }

        for (auto &GlobalName: YamlConfig.IgnoredGlobals) {
            auto *GV = HAKCSystemInfo.M.getGlobalVariable(GlobalName, true);
            if (GV) {
                HAKCSystemInfo.IgnoredGlobalList.push_back(GV);
            }
        }

        for (const auto &AllocationDefinition: YamlConfig.KernelAllocationSizeMap) {
            auto Allocation = HAKCAllocationSize::FromYaml(AllocationDefinition, HAKCSystemInfo.M);
            if (Allocation) {
                HAKCSystemInfo.AllocationSizeMap[Allocation->GetAllocationFunction()] = std::move(Allocation);
            }
        }
    }

    iterator_range<FunctionList::iterator> HAKCSystemInformation::GetNoTransferFunctions() {
        return make_range(NoTransferFunctionList.begin(), NoTransferFunctionList.end());
    }

    iterator_range<HAKCTransferList::iterator> HAKCSystemInformation::CompartmentTransferFunctions() {
        return make_range(CompartmentTransferFunctionList.begin(), CompartmentTransferFunctionList.end());
    }

    iterator_range<HAKCFunctionList::iterator> HAKCSystemInformation::CompartmentalizationValidationFunctions() {
        return make_range(CompartmentalizationValidationFunctionList.begin(),
                          CompartmentalizationValidationFunctionList.end());
    }

    iterator_range<FunctionList::iterator> HAKCSystemInformation::CompartmentalizationSupportFunctions() {
        return make_range(CompartmentalizationSupportFunctionList.begin(),
                          CompartmentalizationSupportFunctionList.end());
    }

    iterator_range<FunctionList::iterator> HAKCSystemInformation::SafeTransitionFunctions() {
        return make_range(SafeTransitionFunctionList.begin(), SafeTransitionFunctionList.end());
    }

    iterator_range<HAKCTypeSet::iterator> HAKCSystemInformation::IgnoredTypes() {
        return make_range(IgnoredTypeSet.begin(), IgnoredTypeSet.end());
    }

    iterator_range<HAKCGlobalList::iterator> HAKCSystemInformation::IgnoredGlobals() {
        return make_range(IgnoredGlobalList.begin(), IgnoredGlobalList.end());
    }
} // hakc
