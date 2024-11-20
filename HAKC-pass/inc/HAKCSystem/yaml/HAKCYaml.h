//
// Created by de29664 on 11/12/24.
//

#ifndef HAKC_HAKCYAML_H
#define HAKC_HAKCYAML_H

#include <vector>
#include <string>

#include "HAKCFunctionDefinition/HAKCTransferFunction.h"

typedef std::string HAKCYAMLStringType;

template<typename Ty>
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

        HAKCYAMLAllocationType() : FunctionName(), AllocationType(InvalidAllocationType), Arguments() {}
    };

    struct HAKCYAMLFunctionDefinitionType {
        HAKCYAMLStringType FunctionName;
        unsigned PointerIdx;
        unsigned SizeIdx;
        unsigned CompartmentIdx;
        unsigned DivisionIdx;

        HAKCYAMLFunctionDefinitionType()
                : FunctionName(), PointerIdx(HAKCTransferFunction::MissingIdx),
                  SizeIdx(HAKCTransferFunction::MissingIdx), CompartmentIdx(HAKCTransferFunction::MissingIdx),
                  DivisionIdx(HAKCTransferFunction::MissingIdx) {}

        bool IsValid() const { return !FunctionName.empty(); }
    };

    struct HAKCYAMLTransferType : public HAKCYAMLFunctionDefinitionType {
        HAKCYAMLTransferType() : HAKCYAMLFunctionDefinitionType() {}

        Function *GetFunction(Module &M) {
            if (!IsValid()) {
                return nullptr;
            }
            unsigned BitCount = 64;
            SmallVector<Type *> ArgTypes = {
                    PointerType::get(M.getContext(), 0)
            };
            if (PointerIdx != HAKCTransferFunction::MissingIdx) {
                ArgTypes.push_back(IntegerType::get(M.getContext(), BitCount));
            }
            if (SizeIdx != HAKCTransferFunction::MissingIdx) {
                ArgTypes.push_back(IntegerType::get(M.getContext(), BitCount));
            }
            if (CompartmentIdx != HAKCTransferFunction::MissingIdx) {
                ArgTypes.push_back(IntegerType::get(M.getContext(), BitCount));
            }
            if (DivisionIdx != HAKCTransferFunction::MissingIdx) {
                ArgTypes.push_back(IntegerType::get(M.getContext(), BitCount));
            }

            auto *FuncType = FunctionType::get(PointerType::get(M.getContext(), 0), ArgTypes, false);
            return dyn_cast<Function>(M.getOrInsertFunction(FunctionName, FuncType).getCallee());
        }
    };

    struct HAKCYAMLCustomTransferType : public HAKCYAMLTransferType {
        HAKCYAMLStringType TypeName;

        HAKCYAMLCustomTransferType() : HAKCYAMLTransferType(), TypeName() {}
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

        HAKCYAMLSequence <HAKCYAMLCustomTransferType> CustomTransferFunctionList;
        HAKCYAMLSequence <HAKCYAMLFunctionDefinitionType> CompartmentalizationSupportFunctions;
        HAKCYAMLSequence <HAKCYAMLAllocationType> KernelAllocationSizeMap;
        HAKCYAMLTransferType DefaultCompartmentTransfer;
        HAKCYAMLTransferType PerCPUCompartmentTransfer;
    };
} // hakc

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLAllocationType);
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLTransferType);
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLCustomTransferType);
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFunctionDefinitionType);

template<>
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
struct yaml::MappingTraits<hakc::HAKCYAMLFunctionDefinitionType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLFunctionDefinitionType &FunctionDefinition) {
        io.mapRequired("name", FunctionDefinition.FunctionName);
        io.mapOptional("ptr-idx", FunctionDefinition.PointerIdx);
        io.mapOptional("compartment-idx", FunctionDefinition.CompartmentIdx);
        io.mapOptional("division-idx", FunctionDefinition.DivisionIdx);
        io.mapOptional("size-idx", FunctionDefinition.SizeIdx);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLTransferType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLTransferType &TransferType) {
        io.mapRequired("name", TransferType.FunctionName);
        io.mapRequired("ptr-idx", TransferType.PointerIdx);
        io.mapRequired("compartment-idx", TransferType.CompartmentIdx);
        io.mapRequired("division-idx", TransferType.DivisionIdx);
        io.mapOptional("size-idx", TransferType.SizeIdx);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLCustomTransferType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLCustomTransferType &CustomTransfer) {
        io.mapRequired("name", CustomTransfer.FunctionName);
        io.mapRequired("ptr-idx", CustomTransfer.PointerIdx);
        io.mapRequired("compartment-idx", CustomTransfer.CompartmentIdx);
        io.mapRequired("division-idx", CustomTransfer.DivisionIdx);
        io.mapRequired("type", CustomTransfer.TypeName);
        io.mapOptional("size-idx", CustomTransfer.SizeIdx);
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
        io.mapOptional("PerCPUCompartmentTransferFunction", YamlConfig.PerCPUCompartmentTransfer);
        io.mapOptional("CustomTransferFunctions", YamlConfig.CustomTransferFunctionList);
    }
};

#endif //HAKC_HAKCYAML_H
