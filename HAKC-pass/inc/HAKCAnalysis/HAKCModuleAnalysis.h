//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCMODULEANALYSIS_H
#define HAKC_HAKCMODULEANALYSIS_H

#include <bits/stdc++.h>
#include <execinfo.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <tuple>

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Regex.h"

#include "CommonHAKCAnalysis.h"
#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"
// #include "HAKCAnalysis/HAKCFunctionAnalysis.h"
// #include "HAKCFunctionDefinition/HAKCCustomTransfer.h"
// #include "HAKCSystemInformation.h"


namespace hakc {

#define HAKC_TRANSFER(Name, CompartmentIDIdx, ColorIdx) RegisterHAKCTransfer(std::make_shared<hakc::HAKCTransferFunction>(Name, (unsigned) 0, CompartmentIDIdx, ColorIdx))

#define HAKC_TRANSFER_NO_COLOR(Name, CompartmentIDIdx) RegisterHAKCTransfer(std::make_shared<hakc::HAKCTransferFunction>(Name, (unsigned) 0, CompartmentIDIdx))

#define HAKC_FUNCTION(Name) RegisterNonTransferHAKCFunction(std::make_shared<hakc::HAKCFunctionDefinition>(Name))

#define HAKC_CUSTOM_TRANSFER(CustomTransfer, Args...) RegisterCustomTransfer(std::make_shared<CustomTransfer>(Args)

    class HAKCFunctionAnalysis;

    class HAKCModuleAnalysis : public CommonHAKCAnalysis {
    protected:
        bool moduleModified;
        std::set<int64_t> used_compartments;
        Module &M;
        bool MajorityColorSet;
        sym_color_t MajorityColor;
        Module &getModule();

        ConstantInt *getSymbolColor(GlobalValue *GV);

        ConstantInt *GetColorValue(sym_color_t Color);

        GlobalValue *ExtractGlobalFromKernelParam(GlobalVariable *GV);

        void emitModParamGetCtx(GlobalValue *kernparam);

        bool functionEscapes(Function *F);

        void registerUsedCompartment(int64_t compartment);

        std::string getGlobalHAKCSectionName(GlobalVariable *GV);

        void updateCallParameters(std::map<Function *, std::set<CallInst *>> calls_map);

        void CompartmentalizeFunction(Function *F);

        HAKCFunctionAnalysis *GetFunctionTransformation(Function *F);

        bool isModuleCompartmentalized();

        void GetAnalysisFunctions();

        std::shared_ptr<HAKCTransformer> CreateTransformer();

        void compartmentalizeModule();

        void removeSignatures();

        void addTransferFunctions();

        void RegisterCustomTransfer(hakc_custom_transfer_def_t CustomTransfer);

        void RegisterHAKCTransfer(hakc_transfer_def_t Transfer);

        void RegisterNonTransferHAKCFunction(hakc_function_def_t HAKCFunction);

        bool FunctionNeedsAnalysis(Function *F);

        std::set<Function *> AnalysisFunctions;

    private:
        std::shared_ptr<HAKCTransformer> transformer;

        std::vector<hakc_custom_transfer_def_t> CustomTransfers;
        std::set<hakc_transfer_def_t> Transfers;
        std::set<hakc_function_def_t> NonTransferHAKCFunctions;

        void moveGlobalsToPMCSection();

        /**
         * @brief Determines if a symbol is used in EXPORT_SYMBOL macro
         * @param F
         * @return
         */
        bool functionIsExported(Function *F);

        // HAKCSystemInformation *SysInfo;
        std::shared_ptr<HAKCSystemInformation> SysInfo;

    public:
        unsigned totalDataChecks, totalCodeChecks, totalTransfers;
        std::map<Function *, std::set<CallInst *>> HAKCFunctions;

        std::shared_ptr<HAKCSystemInformation> GetSysInfo();

        explicit HAKCModuleAnalysis(Module &M);

        // bool functionIsTransferCandidate(Function *F) override;
        bool functionIsTransferCandidate(Function *F);
        
        bool valueShouldBeReplacedWithTransfer(Value *V);

        ConstantInt *getFunctionColor(Function *F);

        ConstantInt *getGlobalColor(GlobalVariable *GV);

        std::set<std::string> GetIgnoredGlobals();

        std::map<std::string, HAKCAllocationSize> GetKernelAllocationSizeMap();

        bool valueIsReadonlyPtr(Value *value);

        std::set<std::string> GetIgnoredTypes();

        std::set<std::string> GetNoTransferFunctions();

        sym_color_t GetMajoritySymbolColor();

        static std::string getColorStringFromValue(ConstantInt *color);

        static sym_color_t getColorFromValue(ConstantInt *Color);

        virtual ~HAKCModuleAnalysis() = default;
        // ~HAKCModuleAnalysis(){};

        bool isModuleTransformed();

        void performTransformations();

        bool functionInAnalysisSet(Function *F);

        void addCompartmentMetadata();

        HAKCTransformer &getTransformer();

        std::set<std::string> GetHAKCSourcePaths();

        std::set<std::string> GetSeparateNamespacePaths();

        std::set<std::string> GetSafeTransitionFunctions();

        // std::set<std::string> GetSafeTransitionFunctions_Arch();

        bool TransferFunctionShouldBeCreated(Function *F);

        bool AliasShouldBeCreated(Function *F);

        bool FunctionDefinedInAssembly(Function *F);

        std::set<hakc_function_def_t> GetHAKCFunctions();

        std::set<hakc_transfer_def_t> GetHAKCTransferFunctions();

        std::set<hakc_custom_transfer_def_t> GetHAKCCustomTransferFunctions();

        void InitHAKCFunctions();

        void InitAnalysis();

        std::string HACKCodeAuthenticationName();

        std::string HAKCDataAuthenticationName();

        std::string HAKCCompartmentTransferName();

        std::string HAKCPerCPUCompartmentTransferName();

        std::string HAKCEntryTokenName();

        Function *GetFunctionByName(StringRef Name, FunctionType *FuncTy);

        FunctionCallee GetFunctionCalleeByName(StringRef Name, FunctionType *FuncTy);

        StructType *GetKernelParamType();

        void generateModuleParamGetCtxFunction(GlobalVariable *GV);

        void transferModuleParams();
        
        void PrintStack();
    };

}// namespace hakc

#endif//HAKC_HAKCMODULEANALYSIS_H
