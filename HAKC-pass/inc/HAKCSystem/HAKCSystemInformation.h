//
// Created by de29664 on 11/7/24.
//

#ifndef HAKC_HAKCSYSTEMINFORMATION_H
#define HAKC_HAKCSYSTEMINFORMATION_H

#include <set>
#include <map>
#include <string>
#include "llvm/Support/YAMLTraits.h"
#include "llvm/IR/Module.h"
#include "HAKCSystem/HAKCAllocationSize.h"

using namespace llvm;

typedef std::vector<std::string> HAKCYamlSequence;

namespace hakc {
    struct HAKCYamlConfig {
        std::string Arch;
        std::string Platform;
        std::string Database;
        HAKCYamlSequence NoTransferFunctions;
        HAKCYamlSequence SeparateNamespacePaths;
        HAKCYamlSequence HAKCSourcePaths;
        HAKCYamlSequence SafeTransitionFunctions;
        HAKCYamlSequence IgnoredTypes;
        HAKCYamlSequence IgnoredGlobals;
        std::vector<HAKCAllocationType> KernelAllocationSizeMap;
    };

    class HAKCSystemInformation {
    public:
        explicit HAKCSystemInformation(Module &M);

        friend void operator<<(HAKCSystemInformation &HAKCSystemInfo, HAKCYamlConfig &YamlConfig);

    protected:
        Module &M;
        std::string Arch;
        std::string Platform;
        std::string Database;
        std::set<Function*> NoTransferFunctions;
        std::set<std::string> SeparateNamespacePaths;
        std::set<std::string> HAKCSourcePaths;
        std::set<Function*> SafeTransitionFunctions;
        std::set<Type*> IgnoredTypes;
        std::set<GlobalValue*> IgnoredGlobals;
        std::map<Function*, std::unique_ptr<HAKCAllocationSize>> AllocationSizeMap;
    };

} // hakc

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCAllocationType);

template <>
struct yaml::ScalarEnumerationTraits<hakc::HAKCAllocationTypeEnum> {
    static void enumeration(IO &io, hakc::HAKCAllocationTypeEnum &value) {
        io.enumCase(value, hakc::HAKCSingleArgumentSize::YAMLString(), hakc::SimpleArgumentSize);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCAllocationType> {
    static void mapping(yaml::IO &io, hakc::HAKCAllocationType &AllocationType) {
        io.mapRequired("name", AllocationType.FunctionName);
        io.mapRequired("type", AllocationType.AllocationType);
        io.mapRequired("args", AllocationType.Arguments);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYamlConfig> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlConfig &SystemInfo) {
        io.mapRequired("Arch", SystemInfo.Arch);
        io.mapRequired("Platform", SystemInfo.Platform);
        io.mapRequired("Database", SystemInfo.Database);
        io.mapOptional("NoTransferFunctions", SystemInfo.NoTransferFunctions);
        io.mapOptional("SeparateNamespacePaths", SystemInfo.SeparateNamespacePaths);
        io.mapOptional("HAKCSourcePaths", SystemInfo.HAKCSourcePaths);
        io.mapOptional("SafeTransitionFunctions", SystemInfo.SafeTransitionFunctions);
        io.mapOptional("IgnoredTypes", SystemInfo.IgnoredTypes);
        io.mapOptional("IgnoredGlobals", SystemInfo.IgnoredGlobals);
        io.mapOptional("KernelAllocationSizeMap", SystemInfo.KernelAllocationSizeMap);
    }
};

#endif //HAKC_HAKCSYSTEMINFORMATION_H
