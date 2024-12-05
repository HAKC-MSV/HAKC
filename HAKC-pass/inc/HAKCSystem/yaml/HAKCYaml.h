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

    struct HAKCYAMLFileType {
        HAKCYAMLStringType PathRoot;
        HAKCYAMLStringSequenceType Files;

        HAKCYAMLFileType() : PathRoot(), Files() {}
    };

    struct HAKCYAMLStructType {
        HAKCYAMLStringType StructType;
        HAKCYAMLStringSequenceType StructSubType;

        HAKCYAMLStructType() : StructType(), StructSubType() {}
    };


    struct HAKCYAMLFunctionDefinitionType {
        HAKCYAMLStringType FunctionName;
        unsigned PointerIdx;
        unsigned SizeIdx;
        unsigned CompartmentIdx;
        unsigned DivisionIdx;
        unsigned IsCodeIdx;

        HAKCYAMLFunctionDefinitionType()
                : FunctionName(), PointerIdx(HAKCTransferFunction::MissingIdx),
                  SizeIdx(HAKCTransferFunction::MissingIdx), CompartmentIdx(HAKCTransferFunction::MissingIdx),
                  DivisionIdx(HAKCTransferFunction::MissingIdx), IsCodeIdx(HAKCTransferFunction::MissingIdx) {}

        bool IsValid() const { return !FunctionName.empty(); }

        Function *GetFunction(Module &M) {
            if (!IsValid()) {
                return nullptr;
            }
            unsigned BitCount = 64;
            SmallVector<Type *, HAKCTransferFunction::MaxArgIndex> ArgTypes = {
                PointerType::get(M.getContext(), 0)
            };
            if (PointerIdx != HAKCTransferFunction::MissingIdx) {
                ArgTypes[PointerIdx] = IntegerType::get(M.getContext(), BitCount);
            }
            if (SizeIdx != HAKCTransferFunction::MissingIdx) {
                ArgTypes[SizeIdx] = IntegerType::get(M.getContext(), BitCount);
            }
            if (CompartmentIdx != HAKCTransferFunction::MissingIdx) {
                ArgTypes[CompartmentIdx] = IntegerType::get(M.getContext(), BitCount);
            }
            if (DivisionIdx != HAKCTransferFunction::MissingIdx) {
                ArgTypes[DivisionIdx] = IntegerType::get(M.getContext(), BitCount);
            }
            if (IsCodeIdx != HAKCTransferFunction::MissingIdx) {
                ArgTypes[IsCodeIdx] = IntegerType::get(M.getContext(), 1);
            }

            auto *FuncType = FunctionType::get(PointerType::get(M.getContext(), 0), ArgTypes, false);
            return dyn_cast<Function>(M.getOrInsertFunction(FunctionName, FuncType).getCallee());
        }
    };

    struct HAKCYAMLTransferType : public HAKCYAMLFunctionDefinitionType {
        HAKCYAMLTransferType() : HAKCYAMLFunctionDefinitionType() {}
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
        HAKCYAMLStringSequenceType SafeTransitionFunctions;
        HAKCYAMLStringSequenceType IgnoredGlobals;
        HAKCYAMLStringSequenceType TransferFunctions;
        HAKCYAMLStringSequenceType PassDebugSymbols;
        HAKCYAMLStringSequenceType SeparateNamespacePathsList;
        HAKCYAMLStringSequenceType HAKCSourcePathsList; 
        HAKCYAMLStringSequenceType IgnoredTypesList; 
        bool OutputAllDebugInfo;

        HAKCYAMLSequence <HAKCYAMLCustomTransferType> CustomTransferFunctions;
        HAKCYAMLSequence <HAKCYAMLFunctionDefinitionType> CompartmentalizationSupportFunctions;
        HAKCYAMLSequence <HAKCYAMLAllocationType> AllocationFunctions;
        HAKCYAMLSequence <HAKCYAMLFileType> SeparateNamespacePaths;
        HAKCYAMLSequence <HAKCYAMLFileType> HAKCSourcePaths;
        HAKCYAMLSequence <HAKCYAMLStructType> IgnoredTypes;
        HAKCYAMLTransferType DefaultCompartmentTransfer;
        HAKCYAMLFunctionDefinitionType SignWithDivision;
        HAKCYAMLTransferType PerCPUCompartmentTransfer;
    };
} // hakc

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLAllocationType);
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLTransferType);
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLCustomTransferType);
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFunctionDefinitionType);
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFileType);
LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLStructType);

inline void ValidateHAKCDefinition(hakc::HAKCYAMLFunctionDefinitionType &Definition) {
#define FieldCheck(Def, Field) if (Def.Field != hakc::HAKCTransferFunction::MissingIdx && Def.Field > hakc::HAKCTransferFunction::MaxArgIndex) { errs() << "Invalid Index Value for " << #Field << " : " << Def.Field << "\n"; throw std::exception(); }
    FieldCheck(Definition, CompartmentIdx);
    FieldCheck(Definition, DivisionIdx);
    FieldCheck(Definition, PointerIdx);
    FieldCheck(Definition, IsCodeIdx);
    FieldCheck(Definition, SizeIdx);
#undef FieldCheck
}


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
        io.mapOptional("is-code-idx", FunctionDefinition.IsCodeIdx);
        ValidateHAKCDefinition(FunctionDefinition);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLStructType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLStructType &Struct) {
        io.mapRequired("type", Struct.StructType);
        io.mapRequired("subtypes", Struct.StructSubType);

    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLFileType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLFileType &File) {
        io.mapRequired("path_root", File.PathRoot);
        io.mapRequired("file_names", File.Files);
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
        io.mapOptional("is-code-idx", TransferType.IsCodeIdx);
        ValidateHAKCDefinition(TransferType);
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
        io.mapOptional("is-code-idx", CustomTransfer.IsCodeIdx);
        ValidateHAKCDefinition(CustomTransfer);
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
        io.mapRequired("SignWithDivisionFunction", YamlConfig.SignWithDivision);

        io.mapOptional("CompartmentalizationSupportFunctions", YamlConfig.CompartmentalizationSupportFunctions);
        io.mapOptional("NoTransferFunctions", YamlConfig.NoTransferFunctions);
        io.mapOptional("SeparateNamespacePathList", YamlConfig.SeparateNamespacePaths);
        io.mapOptional("HAKCSourcePathList", YamlConfig.HAKCSourcePaths);
        io.mapOptional("SafeTransitionFunctions", YamlConfig.SafeTransitionFunctions);
        io.mapOptional("IgnoredTypes", YamlConfig.IgnoredTypes);
        io.mapOptional("IgnoredGlobals", YamlConfig.IgnoredGlobals);
        io.mapOptional("AllocationFunctions", YamlConfig.AllocationFunctions);
        io.mapOptional("OutputDebugInfo", YamlConfig.OutputAllDebugInfo, false);
        io.mapOptional("DebugOutputSymbols", YamlConfig.PassDebugSymbols);
        io.mapOptional("PerCPUCompartmentTransferFunction", YamlConfig.PerCPUCompartmentTransfer);
        io.mapOptional("CustomTransferFunctions", YamlConfig.CustomTransferFunctions);
    }
};

#endif //HAKC_HAKCYAML_H
