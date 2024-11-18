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
    class HAKCModuleAnalysis {
    protected:
        SmallVector<HAKCCompartment, 8> UsedCompartments;
        CommonHAKCAnalysis &CommonAnalysis;
        std::vector<Function*> AnalysisFunctions;
        HAKCTypeIdentifier TypeIdentifier;
        HAKCCompartmentalizationPolicy &Policy;

        void InitAnalysis();

        GlobalValue *ExtractGlobalFromKernelParam(GlobalVariable *GV);

        void emitModParamGetCtx(GlobalValue *kernparam);

        bool functionEscapes(Function *F);

        void RegisterUsedCompartment(HAKCCompartment &compartment);

        virtual std::string getGlobalHAKCSectionName(GlobalVariable *GV);

        virtual void TransformModule();

        virtual void TransformFunctions();

        virtual bool FunctionNeedsAnalysis(Function *F);

        virtual Function *CreateInitTransfer(GlobalVariable *GlobalVar);

        virtual StringRef GlobalInitTransferPrefix() const;

        virtual StringRef GlobalInitTransferSectionName() const;

        virtual StringRef GlobalInitTransferPointerSectionName() const;

        virtual std::string
        GlobalVariableROSectionName(GlobalVariable *GlobalVar);

        virtual void PopulateGlobalInitTransferFunc(Function *GlobTransfer, GlobalVariable *GlobalVar);

        virtual bool TransferIsNeeded(GlobalVariable *GlobalVar);

        virtual bool
        ConstantStructTransferIsNeeded(ConstantStruct *ConstStruct);

        bool AliasShouldBeCreated(Function *F);

        bool isModuleCompartmentalized();

        void MoveGlobalsToHAKCSection();

        void AddTransferFunctions(HAKCCompartmentalizationPolicy &Policy);

    public:
        virtual ~HAKCModuleAnalysis() = default;

        HAKCModuleAnalysis(CommonHAKCAnalysis &CommonAnalysis, HAKCCompartmentalizationPolicy &Policy);

        virtual void performTransformations();

        virtual void AddCompartmentMetadata();

        bool TransferFunctionShouldBeCreated(Function *F);

        std::string HAKCEntryTokenName();

        virtual StringRef HAKCSignWithDivisionName();

        FunctionCallee GetFunctionCalleeByName(StringRef Name, FunctionType *FuncTy);

        StructType *GetKernelParamType();

        virtual void CreateInitGlobalMemberTransfers();

        Module &GetModule();

        bool FunctionDefinedInAssembly(Function *F);

        CommonHAKCAnalysis &GetCommonAnalysis();

        Function* GetFunctionByName(StringRef Name, FunctionType *FuncTy);
    };

}// namespace hakc

#endif//HAKC_HAKCMODULEANALYSIS_H
