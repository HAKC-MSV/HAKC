//
// Created by de29664 on 11/12/24.
//

#ifndef HAKC_HAKCYAML_H
#define HAKC_HAKCYAML_H

#include <vector>
#include <string>

#include "HAKCFunctionDefinition/HAKCTransferFunction.h"

typedef std::string HAKCYAMLStringType;

template <typename Ty>
using HAKCYAMLSequence = std::vector<Ty>;

typedef HAKCYAMLSequence<HAKCYAMLStringType> HAKCYAMLStringSequenceType;

namespace hakc {

    enum HAKCAllocationTypeEnum {
        InvalidAllocationType,
        SimpleArgumentSize,
        SimpleStaticSize,
        StaticPlusArgument,
        MultiplyTwoArguments,
        ArgumentGEP
    };

    struct HAKCYAMLAllocationType {
        HAKCYAMLStringType FunctionName;
        HAKCAllocationTypeEnum AllocationType;
        HAKCYAMLStringSequenceType Arguments;
    };

    struct HAKCYAMLTransferType {
        HAKCYAMLStringType FunctionName;
        unsigned PointerIdx;
        unsigned SizeIdx;
        unsigned CompartmentIdx;
        unsigned DivisionIdx;

        HAKCYAMLTransferType() : FunctionName(), PointerIdx(-1), SizeIdx(-1), CompartmentIdx(-1), DivisionIdx(-1) {}
        bool IsValid() const { return !FunctionName.empty(); }
    };

    struct HAKCYamlConfig {
        HAKCYAMLStringType Arch;
        HAKCYAMLStringType Platform;
        HAKCYAMLStringType Database;
        HAKCYAMLStringType CodeValidationFunction;
        HAKCYAMLStringType DataValidationFunction;
        HAKCYAMLStringSequenceType NoTransferFunctions;
        HAKCYAMLStringSequenceType SeparateNamespacePaths;
        HAKCYAMLStringSequenceType HAKCSourcePaths;
        HAKCYAMLStringSequenceType SafeTransitionFunctions;
        HAKCYAMLStringSequenceType IgnoredTypes;
        HAKCYAMLStringSequenceType IgnoredGlobals;
        HAKCYAMLStringSequenceType TransferFunctions;
        HAKCYAMLStringSequenceType PassDebugSymbols;
        bool OutputAllDebugInfo;

        HAKCYAMLSequence<HAKCYAMLTransferType> CompartmentTransferFunctions;
        HAKCYAMLStringSequenceType CompartmentalizationSupportFunctions;
        HAKCYAMLSequence<HAKCYAMLAllocationType> KernelAllocationSizeMap;
        HAKCYAMLTransferType DefaultCompartmentTransfer;
        HAKCYAMLTransferType PerCPUCompartmentTransfer;
    };
} // hakc

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLAllocationType);
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLTransferType);

template <>
struct yaml::ScalarEnumerationTraits<hakc::HAKCAllocationTypeEnum> {
    static void enumeration(IO &io, hakc::HAKCAllocationTypeEnum &value) {
        io.enumCase(value, "SimpleArgumentSize", hakc::SimpleArgumentSize);
        io.enumCase(value, "SimpleStaticSize", hakc::SimpleStaticSize);
        io.enumCase(value, "StaticPlusArgument", hakc::StaticPlusArgument);
        io.enumCase(value, "MultiplyTwoArguments", hakc::MultiplyTwoArguments);
        io.enumCase(value, "ArgumentGEP", hakc::ArgumentGEP);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLAllocationType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLAllocationType &AllocationType) {
        io.mapRequired("name", AllocationType.FunctionName);
        io.mapRequired("type", AllocationType.AllocationType);
        io.mapRequired("args", AllocationType.Arguments);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLTransferType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLTransferType &TransferType) {
        io.mapRequired("name", TransferType.FunctionName);
        io.mapRequired("ptr-idx", TransferType.PointerIdx);

        io.mapOptional("compartment-idx", TransferType.CompartmentIdx, (unsigned)-1);
        io.mapOptional("division-idx", TransferType.DivisionIdx, (unsigned)-1);
        io.mapOptional("size-idx", TransferType.SizeIdx, (unsigned)-1);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYamlConfig> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlConfig &YamlConfig) {
        io.mapRequired("Arch", YamlConfig.Arch);
        io.mapRequired("Platform", YamlConfig.Platform);
        io.mapRequired("Database", YamlConfig.Database);
        io.mapRequired("CodeValidationFunction", YamlConfig.CodeValidationFunction);
        io.mapRequired("DataValidationFunction", YamlConfig.DataValidationFunction);
        io.mapRequired("DefaultCompartmentTransferFunction", YamlConfig.DefaultCompartmentTransfer);

        io.mapOptional("CompartmentTransferFunctions", YamlConfig.CompartmentTransferFunctions);
        io.mapOptional("CompartmentalizationSupportFunctions", YamlConfig.CompartmentalizationSupportFunctions);
        io.mapOptional("NoTransferFunctions", YamlConfig.NoTransferFunctions);
        io.mapOptional("SeparateNamespacePathList", YamlConfig.SeparateNamespacePaths);
        io.mapOptional("HAKCSourcePathList", YamlConfig.HAKCSourcePaths);
        io.mapOptional("SafeTransitionFunctions", YamlConfig.SafeTransitionFunctions);
        io.mapOptional("IgnoredTypeSet", YamlConfig.IgnoredTypes);
        io.mapOptional("IgnoredGlobals", YamlConfig.IgnoredGlobals);
        io.mapOptional("KernelAllocationSizeMap", YamlConfig.KernelAllocationSizeMap);
        io.mapOptional("OutputDebugInfo", YamlConfig.OutputAllDebugInfo, false);
        io.mapOptional("DebugOutputSymbols", YamlConfig.PassDebugSymbols);
        io.mapOptional("PerCPUCompartmentTransferFunction", YamlConfig.PerCPUCompartmentTransfer, hakc::MISSING_TRANSFER);
    }
};

#endif //HAKC_HAKCYAML_H
