//
// Created by de29664 on 11/7/24.
//

#include "HAKCSystem/HAKCSystemInformation.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "HAKCSystem/yaml/HAKCYaml.h"

namespace hakc {
    HAKCSystemInformation::HAKCSystemInformation(CommonHAKCAnalysis &CommonAnalysis) : CommonAnalysis(CommonAnalysis),
        TypeIdentifier(CommonAnalysis), DebugOutput(false), PassMode(InvalidPassModeType), Arch(), Platform(),
        DatabasePath(), SourcePath(), BuildPath(), DagAnalysisRootPath(), IncludePathsList(),
        NoTransferFunctionList(), CompartmentTransferFunctionList(), CodeValidationFunction(nullptr),
        DataValidationFunction(nullptr), SignWithDivisionFunction(nullptr), DefaultCompartmentTransfer(nullptr),
        PerCPUCompartmentTransfer(nullptr), CompartmentalizationSupportFunctionList(), SymbolsToOutputDebugInfo(),
        SeparateNamespacePathList(), HAKCSourcePathList(), SafeTransitionFunctionList(), IgnoredTypeSet(),
        IgnoredGlobalList(), AllocationFunctionList(), CustomTransferList() {
    }

    void operator<<(HAKCSystemInformation &HAKCSystemInfo, HAKCYamlConfig &YamlConfig) {
        HAKCSystemInfo.Arch = YamlConfig.Arch;
        HAKCSystemInfo.Platform = YamlConfig.Platform;
        HAKCSystemInfo.DatabasePath = YamlConfig.DatabasePath;
        HAKCSystemInfo.SourcePath = YamlConfig.SourcePath;
        HAKCSystemInfo.BuildPath = YamlConfig.BuildPath;
        HAKCSystemInfo.DagAnalysisRootPath = YamlConfig.DagAnalysisRootPath;
        HAKCSystemInfo.PassMode = YamlConfig.PassMode;
        HAKCSystemInfo.DebugOutput = YamlConfig.OutputAllDebugInfo;

        for (auto &FunctionName: YamlConfig.NoTransferFunctions) {
            auto *F = HAKCSystemInfo.GetModule().getFunction(FunctionName);
            if (F) {
                HAKCSystemInfo.NoTransferFunctionList.push_back(F);
            }
        }

        for (auto &SymbolName: YamlConfig.PassDebugSymbols) {
            auto *F = HAKCSystemInfo.GetModule().getFunction(SymbolName);
            if (F) {
                HAKCSystemInfo.SymbolsToOutputDebugInfo.push_back(F);
            } else {
                auto *Global = HAKCSystemInfo.GetModule().getGlobalVariable(SymbolName);
                if (Global) {
                    HAKCSystemInfo.SymbolsToOutputDebugInfo.push_back(Global);
                }
            }
        }

        auto CodeValidation = HAKCSystemInfo.GetModule().getOrInsertFunction(YamlConfig.CodeValidationFunction,
                                                                             CommonHAKCAnalysis::GetCodeAuthenticationFunctionType(
                                                                                 HAKCSystemInfo.GetModule()));
        HAKCSystemInfo.CodeValidationFunction = dyn_cast<Function>(CodeValidation.getCallee());
        auto DataValidation = HAKCSystemInfo.GetModule().getOrInsertFunction(YamlConfig.DataValidationFunction,
                                                                             CommonHAKCAnalysis::GetDataAuthenticationFunctionType(
                                                                                 HAKCSystemInfo.GetModule()));
        HAKCSystemInfo.DataValidationFunction = dyn_cast<Function>(DataValidation.getCallee());


        HAKCSystemInfo.IncludePathsList.append(YamlConfig.IncludePathsList.begin(),
                                               YamlConfig.IncludePathsList.end());

        for (auto &FileType: YamlConfig.SeparateNamespacePaths) {
            auto PathRoot = FileType.PathRoot;
            CommonHAKCAnalysis::getWriter() << "PathRoot: " << PathRoot << "\n";
            for (auto &FileName: FileType.Files) {
                CommonHAKCAnalysis::getWriter() << "\tFile: " << PathRoot << FileName << "\n";
                auto File = PathRoot + FileName;
                YamlConfig.SeparateNamespacePathsList.push_back(File);
            }
        }
        HAKCSystemInfo.SeparateNamespacePathList.append(YamlConfig.SeparateNamespacePathsList.begin(),
                                                        YamlConfig.SeparateNamespacePathsList.end());

        for (auto &FileType: YamlConfig.HAKCSourcePaths) {
            auto PathRoot = FileType.PathRoot;
            CommonHAKCAnalysis::getWriter() << "PathRoot: " << PathRoot << "\n";
            for (auto &FileName: FileType.Files) {
                CommonHAKCAnalysis::getWriter() << "\tFile: " << PathRoot << FileName << "\n";
                auto File = PathRoot + FileName;
                YamlConfig.HAKCSourcePathsList.push_back(File);
            }
        }
        HAKCSystemInfo.HAKCSourcePathList.append(YamlConfig.HAKCSourcePathsList.begin(),
                                                 YamlConfig.HAKCSourcePathsList.end());

        for (auto &FunctionName: YamlConfig.SafeTransitionFunctions) {
            auto *F = HAKCSystemInfo.GetModule().getFunction(FunctionName);
            if (F) {
                HAKCSystemInfo.SafeTransitionFunctionList.push_back(F);
            }
        }

        auto *DefaultTransferFunc = dyn_cast<Function>(
            HAKCSystemInfo.GetModule().getOrInsertFunction(YamlConfig.DefaultCompartmentTransfer.FunctionName,
                                                           CommonHAKCAnalysis::GetTransferFunctionType(
                                                               HAKCSystemInfo.GetModule())).getCallee());
        HAKCSystemInfo.DefaultCompartmentTransfer = std::make_shared<HAKCTransferFunction>(DefaultTransferFunc,
            YamlConfig.DefaultCompartmentTransfer.PointerIdx,
            YamlConfig.DefaultCompartmentTransfer.CompartmentIdx,
            YamlConfig.DefaultCompartmentTransfer.DivisionIdx,
            YamlConfig.DefaultCompartmentTransfer.SizeIdx);
        if (YamlConfig.PerCPUCompartmentTransfer.IsValid()) {
            auto *PerCPUTransferFunc = dyn_cast<Function>(
                HAKCSystemInfo.GetModule().getOrInsertFunction(YamlConfig.PerCPUCompartmentTransfer.FunctionName,
                                                               CommonHAKCAnalysis::GetTransferFunctionType(
                                                                   HAKCSystemInfo.GetModule())).getCallee());
            HAKCSystemInfo.PerCPUCompartmentTransfer = std::make_shared<HAKCTransferFunction>(PerCPUTransferFunc,
                YamlConfig.PerCPUCompartmentTransfer.PointerIdx,
                YamlConfig.PerCPUCompartmentTransfer.CompartmentIdx,
                YamlConfig.PerCPUCompartmentTransfer.DivisionIdx,
                YamlConfig.PerCPUCompartmentTransfer.SizeIdx);
        } else {
            HAKCSystemInfo.PerCPUCompartmentTransfer = HAKCSystemInfo.DefaultCompartmentTransfer;
        }

        for (auto &SupportFunctionDefinition: YamlConfig.CompartmentalizationSupportFunctions) {
            auto *F = SupportFunctionDefinition.GetFunction(HAKCSystemInfo.GetModule());
            if (F) {
                HAKCSystemInfo.CompartmentalizationSupportFunctionList.push_back(F);
            }
        }

        HAKCSystemInfo.SignWithDivisionFunction = YamlConfig.SignWithDivision.GetFunction(HAKCSystemInfo.GetModule());

        for (auto &StructType: YamlConfig.IgnoredTypes) {
            auto StructTypeName = StructType.StructType;
            for (auto &StructSubTypeName: StructType.StructSubType) {
                auto StructName = StructTypeName + StructSubTypeName;
                auto *Ty = StructType::getTypeByName(HAKCSystemInfo.GetModule().getContext(), StructName);
                if (Ty) {
                    HAKCSystemInfo.IgnoredTypeSet.insert(Ty);
                }
            }
        }

        for (auto &GlobalName: YamlConfig.IgnoredGlobals) {
            auto *GV = HAKCSystemInfo.GetModule().getGlobalVariable(GlobalName, true);
            if (GV) {
                HAKCSystemInfo.IgnoredGlobalList.push_back(GV);
            }
        }

        for (const auto &AllocationDefinition: YamlConfig.AllocationFunctions) {
            auto Allocation = HAKCAllocationSize::FromYaml(AllocationDefinition, HAKCSystemInfo.GetModule());
            if (Allocation) {
                HAKCSystemInfo.AllocationFunctionList.push_back(Allocation);
            }
        }

        SmallVector<HAKCTypeP> Types;
        // ProcessDebugInfo must happen before creating custom transfers
        HAKCSystemInfo.TypeIdentifier.ProcessDebugInfo();
        HAKCSystemInfo.TypeIdentifier.GetHAKCTypes(Types);
        for (auto &CustomTransferDefinition: YamlConfig.CustomTransferFunctions) {
            for (auto &HAKCTy: Types) {
                if (CustomTransferDefinition.TypeName == HAKCTy) {
                    auto *F = CustomTransferDefinition.GetFunction(HAKCSystemInfo.GetModule());
                    auto CustomTransfer = std::make_shared<HAKCCustomTransfer>(F, HAKCTy,
                                                                               CustomTransferDefinition.PointerIdx,
                                                                               CustomTransferDefinition.CompartmentIdx,
                                                                               CustomTransferDefinition.DivisionIdx,
                                                                               CustomTransferDefinition.SizeIdx);
                    HAKCSystemInfo.CustomTransferList.push_back(CustomTransfer);
                }
            }
        }
    }

    bool HAKCSystemInformation::OutputDebugInfo() const {
        return DebugOutput;
    }

    bool HAKCSystemInformation::OutputDebugInfo(GlobalValue *GV) const {
        auto Search = [GV](GlobalValue *Symbol) {
            return Symbol == GV;
        };

        return OutputDebugInfo() || llvm::any_of(SymbolsToOutputDebugInfo, Search);
    }

    Module &HAKCSystemInformation::GetModule() {
        return CommonAnalysis.GetModule();
    }

    hakc::HAKCPassModeTypeEnum HAKCSystemInformation::GetPassMode() const {
        return PassMode;
    }

    StringRef HAKCSystemInformation::GetDatabasePath() const{
        return DatabasePath; 
    }
    
    StringRef HAKCSystemInformation::GetSourcePath() const{
        return SourcePath; 
    }

    StringRef HAKCSystemInformation::GetBuildPath() const{
        return BuildPath; 
    }

    StringRef HAKCSystemInformation::GetDagAnalysisRootPath() const {
        return DagAnalysisRootPath;
    }

    Function *HAKCSystemInformation::CodeValidation() const {
        return CodeValidationFunction;
    }



    Function *HAKCSystemInformation::DataValidation() const {
        return DataValidationFunction;
    }

    Function *HAKCSystemInformation::SignWithDivision() const {
        return SignWithDivisionFunction;
    }

    HAKCTypeIdentifier &HAKCSystemInformation::GetTypeIdentifier() {
        return TypeIdentifier;
    }

    hakc::hakc_transfer_def_t HAKCSystemInformation::CompartmentTransfer(bool PerCPU) const {
        if (PerCPU) {
            return PerCPUCompartmentTransfer;
        } else {
            return DefaultCompartmentTransfer;
        }
    }

    bool HAKCSystemInformation::OutputDebugInfo(StringRef SymbolName) const {
        auto Search = [SymbolName](GlobalValue *Symbol) {
            return Symbol->getName() == SymbolName;
        };

        return OutputDebugInfo() || llvm::any_of(SymbolsToOutputDebugInfo, Search);
    }

    iterator_range<FunctionList::iterator> HAKCSystemInformation::NoTransferFunctions() {
        return make_range(NoTransferFunctionList.begin(), NoTransferFunctionList.end());
    }

    iterator_range<HAKCTransferList::iterator> HAKCSystemInformation::CompartmentTransferFunctions() {
        return make_range(CompartmentTransferFunctionList.begin(), CompartmentTransferFunctionList.end());
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

    iterator_range<HAKCGlobalVariableList::iterator> HAKCSystemInformation::IgnoredGlobals() {
        return make_range(IgnoredGlobalList.begin(), IgnoredGlobalList.end());
    }

    iterator_range<HAKCStringList::iterator> HAKCSystemInformation::SeparateNamespacePaths() {
        return make_range(SeparateNamespacePathList.begin(), SeparateNamespacePathList.end());
    }

    iterator_range<HAKCStringList::iterator> HAKCSystemInformation::HAKCSourcePaths() {
        return make_range(HAKCSourcePathList.begin(), HAKCSourcePathList.end());
    }

    iterator_range<HAKCCustomTransferList::iterator> HAKCSystemInformation::HAKCCustomTransfers() {
        return make_range(CustomTransferList.begin(), CustomTransferList.end());
    }

    iterator_range<HAKCCustomAllocationList::iterator> HAKCSystemInformation::AllocationFunctions() {
        return make_range(AllocationFunctionList.begin(), AllocationFunctionList.end());
    }
} // hakc
