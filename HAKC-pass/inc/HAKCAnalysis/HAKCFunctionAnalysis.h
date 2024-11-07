//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCFUNCTIONANALYSIS_H
#define HAKC_HAKCFUNCTIONANALYSIS_H

#include "llvm/IR/Dominators.h"

#include "CommonHAKCAnalysis.h"
#include "HAKCPointerManager.h"
#include "HAKCSystemInformation.h"
#include "HAKCModuleAnalysis.h"
#include "HAKCTransformers/HAKCTransformer.h"

namespace hakc {

    class HAKCModuleAnalysis;

    class CommonHAKCAnalysis;

    class HAKCPointerManager;

    /**
 * @brief This pass does the following:
 * 1. Find pointers that should be authenticated, add a call to authenticate, and then transform
 * all instructions that dereference the pointer.  Note, that the *actual* dereference might be
 * the result of arbitrary number of GEPs, in which case all intermediate GEP computations are cloned
 * using the authenticated pointer.
 *
 * 2. Insert a validity check for all indirect calls, and add a transfer of all pointer arguments to the
 * target address before the indirect call.  Immediately after the indirect call, the pointer arguments
 * are transferred back to their original clique.
 *
 * 3. Sign global variable pointers passed to functions so that subsequent authentications pass.
 *
 * The current policy is to pass along signed pointers to functions, which could then authenticate pointers
 * which the caller has already authenticated.  This might be redundant, and a source of overhead.
 */
    class HAKCFunctionAnalysis : public CommonHAKCAnalysis {
    protected:
        HAKCPointerManager PointerManager;

        /**
         * @brief Global variables used as function arguments
         */
        std::map<GlobalValue *, std::set<Instruction *>> GlobalArgumentUses;

        /**
         * @brief Used for ideal placement of authentication checks and cloned instructions
         */
        DominatorTree DTree;

        std::set<CallInst *> NonKernelDirectFunctionCallSet;

        std::set<CallInst *> HAKCFunctionCalls;

        Function *CurrentFunction;

        bool SetupHasRun;

        /* All functions used in comparisons and function call arguments should be transfer
         * functions, so replace direct uses with transfer functions */
        std::set<Instruction *> directFunctionUsers;

        unsigned CompartmentTransferCount;

        Instruction *
        addCompartmentTransferCall(Value *operand, const DebugLoc &debugLoc, Instruction *I, ConstantInt *Size,
                                   HAKCCompartmentalizationPolicy &Policy);

        bool userInFunction(Value *user);

        BasicBlock *
        findDominatorUseBlock(Value *ptr, std::set<Instruction *> &users);

        void createAllAuthenticatedPointers();

        void createMissingTransfers(HAKCCompartmentalizationPolicy &Policy);

        void transformPointerDereferences();

        bool argNeedsAuthentication(Use &arg);

        bool phiNodeUsesValue(PHINode *phiNode, Value *target, std::set<PHINode *> &visited);

        bool IsManualSafePointer(CallInst *Call);

        void HandleInstruction(Instruction *I, HAKCCompartmentalizationPolicy &Policy);

        Instruction *getUserInst(User *user);

        bool isPHIofGlobalsOnly(Value *ptr, std::set<PHINode *> &nodes);

        void RegisterPointerDereference(Use &use);

        bool isSafeTransitionFunction(Function *F);

        void handleLoad(LoadInst *load);

        virtual void handleComparison(CmpInst *compare, HAKCCompartmentalizationPolicy &Policy);

        virtual void handleCall(CallInst *call, HAKCCompartmentalizationPolicy &Policy);

        void handleCall(CallInst *call);

        void handleBinaryOperator(BinaryOperator *binOp);

        bool globalShouldBeTransferred(Use &globalValueArg);

        virtual void relocateFunctionSection(HAKCCompartmentalizationPolicy &Policy);

        virtual std::string getHAKCFunctionSectionName(HAKCCompartmentalizationPolicy &Policy);

        void CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls(HAKCCompartmentalizationPolicy &Policy);

        HAKCModuleAnalysis &getModuleAnalysis();

        virtual std::set<Intrinsic::ID> GetIntrinsicsNeedingAuthenticatedArgs();
        ConstantInt *getColor();

        HAKCTransformer &getTransformer();

        std::set<Intrinsic::ID> GetIntrinsicsNeedingAuthenticatedArgs();

        virtual std::set<Intrinsic::ID> GetIntrinsicsToClone();

        virtual void AddManagedPointer(Value *HAKCPointer);
        std::set<Intrinsic::ID> GetInstrinsicsToSkip();

        void AddManagedPointer(Value *HAKCPointer);

        void ReplaceInstructionOperand(Instruction *I, unsigned ArgNo, Value *OldValue, Value *NewValue,
                                       HAKCCompartmentalizationPolicy &Policy);

        void ReplaceDirectFunctionUsesWithTransfers(HAKCCompartmentalizationPolicy &Policy);

        void CheckCompareOperandForDirectFunctionUse(CmpInst *CmpI, HAKCCompartmentalizationPolicy &Policy,
                                                     unsigned int OpNo);

        void MaybeAddCompareToDirectUsers(CmpInst *CmpI, HAKCCompartmentalizationPolicy &Policy);

        virtual std::set<StringRef> GetSafePointerFunctionNames() = 0;

        virtual void UpdateHAKCFunctionParameters(HAKCCompartmentalizationPolicy &Policy);

        virtual void UpdateHAKCFunctionParameters_Arch(CallInst *CallI, HAKCCompartment &TargetCompartment,
                                                       hakc_transfer_def_t &HAKCTransferFunction,
                                                       HAKCCompartmentalizationPolicy &Policy) = 0;

        void AddInstrumentation(bool RelocateSection, HAKCCompartmentalizationPolicy &Policy);

        HAKCTransformer &getTransformer(HAKCCompartmentalizationPolicy &Policy);

        void CheckAndReplaceArgument(Value *V, Instruction *I, unsigned ArgNo, HAKCCompartmentalizationPolicy &Policy);

        HAKCModuleAnalysis *ModAnalysis;

        HAKCSystemInformation *SysInfo;

    public:
        HAKCFunctionAnalysis(Function *F, HAKCCompartmentalizationPolicy &Policy, bool debug);

        ~HAKCFunctionAnalysis() = default;

        bool modifiedFunction();

        void InstrumentCode(HAKCCompartmentalizationPolicy &Policy);

        virtual void setup(HAKCCompartmentalizationPolicy &Policy);

        std::set<StringRef> GetNoTransferFunctions();

        std::set<StringRef> GetSafeTransitionFunctions();

        std::set<hakc_transfer_def_t> GetHAKCTransferFunctions();

        std::map<std::string, HAKCAllocationSize> GetKernelAllocationSizeMap();

        std::set<StringRef> GetIgnoredTypes();

        std::set<hakc_function_def_t> GetHAKCFunctions();
        std::set<StringRef> GetIgnoredGlobals() override;

        std::set<hakc_function_def_t> GetHAKCFunctions() override;

        Value *getDef(Value *, bool, bool);

        Instruction *
        FindUseInsertionPoint(Value *v, std::set<Instruction *> &users);

        Value *
        AddDataAuthCheckAtLocation(Value *signed_ptr, Instruction *location, HAKCCompartmentalizationPolicy &Policy);

        Value *
        AddCodeAuthCheckAtLocation(Value *SignedPtr, Instruction *Location, HAKCCompartmentalizationPolicy &Policy);

        Value *AddSafePointerCreationAtLocation(Value *SignedPtr, Instruction *Location,
                                                HAKCCompartmentalizationPolicy &Policy);

        bool isCompartmentalizedFunction(HAKCCompartmentalizationPolicy &Policy);

        Function &getFunction();

        Instruction *CreateMissingTransfer(Instruction *PointerNeedingTransfer, HAKCCompartmentalizationPolicy &Policy);

        virtual Instruction *SignGlobalPointerWithColor(GlobalValue *GlobalVar, HAKCCompartmentalizationPolicy &Policy);

        Instruction *GetFinalAllocaDef(AllocaInst *Alloca);

        virtual bool isIntrinsicNeedingAuthentication(CallInst *);

        bool PointerIsAuthenticated_Arch(Value *Pointer);

        virtual bool PointerShouldBeConsideredCode(Value *Pointer);

        virtual bool PointerShouldBeManaged(Use &use);

        bool IsPHIOfGlobalsOnly(Value *V);

    };

}// namespace hakc

#endif//HAKC_HAKCFUNCTIONANALYSIS_H
