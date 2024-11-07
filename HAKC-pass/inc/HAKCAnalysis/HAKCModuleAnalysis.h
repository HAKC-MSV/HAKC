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

namespace hakc {

#define HAKC_TRANSFER(Name, CompartmentIDIdx, ColorIdx) RegisterHAKCTransfer(std::make_shared<hakc::HAKCTransferFunction>(Name, (unsigned) 0, CompartmentIDIdx, ColorIdx))

#define HAKC_TRANSFER_NO_COLOR(Name, CompartmentIDIdx) RegisterHAKCTransfer(std::make_shared<hakc::HAKCTransferFunction>(Name, (unsigned) 0, CompartmentIDIdx))

#define HAKC_FUNCTION(Name) RegisterNonTransferHAKCFunction(std::make_shared<hakc::HAKCFunctionDefinition>(Name))

#define HAKC_CUSTOM_TRANSFER(CustomTransfer, Args...) RegisterCustomTransfer(std::make_shared<CustomTransfer>(Args)

    class HAKCFunctionAnalysis;

    class HAKCModuleAnalysis : public CommonHAKCAnalysis {
    protected:
        bool IsCompartmentalizedAndContainsDebugName;
        std::vector<HAKCCompartment> UsedCompartments;
        Module &M;
        std::vector<Function *> AnalysisFunctions;

        ConstantInt *GetColorValue(sym_color_t Color);

        GlobalValue *ExtractGlobalFromKernelParam(GlobalVariable *GV);

        void emitModParamGetCtx(GlobalValue *kernparam);

        bool functionEscapes(Function *F);

        void RegisterUsedCompartment(HAKCCompartment &compartment);

        virtual std::string getGlobalHAKCSectionName(GlobalVariable *GV, HAKCCompartmentalizationPolicy &Policy);

        virtual HAKCFunctionAnalysis *
        GetFunctionTransformation(Function *F, HAKCCompartmentalizationPolicy &Policy) = 0;

        void GetAnalysisFunctions();

        virtual std::shared_ptr<HAKCTransformer> CreateTransformer(HAKCCompartmentalizationPolicy &Policy) = 0;

        virtual void TransformModule(HAKCCompartmentalizationPolicy &Policy);

        virtual void TransformFunctions(HAKCCompartmentalizationPolicy &Policy);

        virtual void AddTransferFunctions(HAKCCompartmentalizationPolicy &Policy);

        void RegisterCustomTransfer(const hakc_custom_transfer_def_t &CustomTransfer);

        void RegisterHAKCTransfer(const hakc_transfer_def_t &Transfer);

        void RegisterNonTransferHAKCFunction(const hakc_function_def_t &HAKCFunction);

        virtual bool FunctionNeedsAnalysis(Function *F);

        virtual Function *CreateInitTransfer(GlobalVariable *GlobalVar, HAKCCompartmentalizationPolicy &Policy);

        virtual StringRef GlobalInitTransferPrefix() const;

        virtual StringRef GlobalInitTransferSectionName() const;

        virtual StringRef GlobalInitTransferPointerSectionName() const;

        virtual std::string
        GlobalVariableROSectionName(GlobalVariable *GlobalVar, HAKCCompartmentalizationPolicy &Policy);

        virtual void PopulateGlobalInitTransferFunc(Function *GlobTransfer, GlobalVariable *GlobalVar,
                                                    HAKCCompartmentalizationPolicy &Policy);

        virtual bool TransferIsNeeded(GlobalVariable *GlobalVar, HAKCCompartmentalizationPolicy &Policy);

        virtual bool
        ConstantStructTransferIsNeeded(ConstantStruct *ConstStruct, HAKCCompartmentalizationPolicy &Policy);

    private:
        std::shared_ptr<HAKCTransformer> transformer;

        std::vector<hakc_custom_transfer_def_t> CustomTransfers;
        std::set<hakc_transfer_def_t> Transfers;
        std::set<hakc_function_def_t> NonTransferHAKCFunctions;

        void MoveGlobalsToHAKCSection(HAKCCompartmentalizationPolicy &Policy);

        std::shared_ptr<HAKCSystemInformation> SysInfo;

    public:
        virtual ~HAKCModuleAnalysis() = default;

        virtual void performTransformations();

        bool functionInAnalysisSet(Function *F);

        virtual void AddCompartmentMetadata(HAKCCompartmentalizationPolicy &Policy);

        HAKCTransformer &getTransformer(HAKCCompartmentalizationPolicy &Policy);

        std::set<std::string> GetHAKCSourcePaths();

        std::set<std::string> GetSeparateNamespacePaths();

        std::set<std::string> GetSafeTransitionFunctions();

        bool TransferFunctionShouldBeCreated(Function *F);

        virtual bool TransferFunctionShouldBeCreated(Function *F, HAKCCompartmentalizationPolicy &Policy);

        virtual bool AliasShouldBeCreated(Function *F, HAKCCompartmentalizationPolicy &Policy);

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

        virtual StringRef HAKCSignWithDivisionName();

        virtual StringRef HAKCEntryTokenName();

        FunctionCallee GetFunctionCalleeByName(StringRef Name, FunctionType *FuncTy);

        StructType *GetKernelParamType();

        virtual void CreateInitGlobalMemberTransfers(HAKCCompartmentalizationPolicy &Policy);

        Module &GetModule();

        /**
            * @brief Determines if a symbol is used in EXPORT_SYMBOL macro
            * @param F
            * @return
            */
        virtual bool FunctionIsExported(Function *F);
    };

}// namespace hakc

#endif//HAKC_HAKCMODULEANALYSIS_H
