//
// Created by de29664 on 11/7/24.
//

#include "HAKCSystem/HAKCSystemInformation.h"

namespace hakc {

    HAKCSystemInformation::HAKCSystemInformation(Module &M) : M(M), Arch(), Platform(), NoTransferFunctions(),
                                                              SeparateNamespacePaths(), HAKCSourcePaths(),
                                                              SafeTransitionFunctions(), IgnoredTypes(),
                                                              IgnoredGlobals(), AllocationSizeMap() {

    }

    void operator<<(HAKCSystemInformation &HAKCSystemInfo, HAKCYamlConfig &YamlConfig) {
        HAKCSystemInfo.Arch = YamlConfig.Arch;
        HAKCSystemInfo.Platform = YamlConfig.Platform;

        for (auto &FunctionName: YamlConfig.NoTransferFunctions) {
            auto *F = HAKCSystemInfo.M.getFunction(FunctionName);
            if (F) {
                HAKCSystemInfo.NoTransferFunctions.insert(F);
            }
        }

        HAKCSystemInfo.SeparateNamespacePaths.insert(YamlConfig.SeparateNamespacePaths.begin(),
                                                     YamlConfig.SeparateNamespacePaths.end());
        HAKCSystemInfo.HAKCSourcePaths.insert(YamlConfig.HAKCSourcePaths.begin(), YamlConfig.HAKCSourcePaths.end());

        for (auto &FunctionName : YamlConfig.SafeTransitionFunctions) {
            auto *F = HAKCSystemInfo.M.getFunction(FunctionName);
            if(F) {
                HAKCSystemInfo.SafeTransitionFunctions.insert(F);
            }
        }

        for(auto &TypeName : YamlConfig.IgnoredTypes) {
            auto *Ty = StructType::getTypeByName(HAKCSystemInfo.M.getContext(), TypeName);
            if(Ty) {
                HAKCSystemInfo.IgnoredTypes.insert(Ty);
            }
        }

        for(auto &GlobalName : YamlConfig.IgnoredGlobals) {
            auto *GV = HAKCSystemInfo.M.getGlobalVariable(GlobalName, true);
            if(GV) {
                HAKCSystemInfo.IgnoredGlobals.insert(GV);
            }
        }

        for(auto &AllocationDefinition : YamlConfig.KernelAllocationSizeMap) {
            auto Allocation = HAKCAllocationSize::FromYaml(AllocationDefinition, HAKCSystemInfo.M);
            if(Allocation) {
                HAKCSystemInfo.AllocationSizeMap[Allocation->GetAllocationFunction()] = std::move(Allocation);
            }
        }
    }
} // hakc
