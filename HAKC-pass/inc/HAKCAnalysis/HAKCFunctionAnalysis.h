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

        Function *CurrentFunction;

        bool SetupHasRun;

        /* All functions used in comparisons and function call arguments should be transfer
         * functions, so replace direct uses with transfer functions */
        std::set<Instruction *> directFunctionUsers;

        unsigned CompartmentTransferCount;

        Instruction *
        addCompartmentTransferCall(Value *operand,
                                   const DebugLoc &debugLoc,
                                   Instruction *I,
                                   ConstantInt *Size);

        bool userInFunction(Value *user);

        BasicBlock *
        findDominatorUseBlock(Value *ptr, std::set<Instruction *> &users);

        void createAllAuthenticatedPointers();

        void createMissingTransfers();

        void transformPointerDereferences();

        bool argNeedsAuthentication(Use &arg);

        bool phiNodeUsesValue(PHINode *phiNode, Value *target, std::set<PHINode *> &visited);

        bool isSelectOfAuthenticatedPointers(Value *v);

        void HandleInstruction(Instruction *I);

        Instruction *getUserInst(User *user);

        bool isPHIofGlobalsOnly(Value *ptr, std::set<PHINode *> &nodes);

        bool pointerShouldBeChecked(Value *ptr);

        void registerPointerDereference(Use &use);

        bool isSafeTransitionFunction(Function *F);

        void handleLoad(LoadInst *load);

        void handleStore(StoreInst *store);

        void handleComparison(CmpInst *compare);

        void handleCall(CallInst *call);

        void handleBinaryOperator(BinaryOperator *binOp);

        bool globalShouldBeTransferred(Use &globalValueArg);

        void relocateFunctionSection();

        std::string getHAKCFunctionSectionName();

        void CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls();

        HAKCModuleAnalysis &getModuleAnalysis();

        ConstantInt *getColor();

        HAKCTransformer &getTransformer();

        std::set<Intrinsic::ID> GetIntrinsicsNeedingAuthenticatedArgs();

        std::set<Intrinsic::ID> GetInstrinsicsToSkip();

        void AddManagedPointer(Value *HAKCPointer);

        void ReplaceInstructionOperand(Instruction *I, unsigned ArgNo, Value *OldValue, Value *NewValue);

        void ReplaceDirectFunctionUsesWithTransfers();

        void CheckCompareOperandForDirectFunctionUse(CmpInst *CmpI, unsigned OpNo);

        void MaybeAddCompareToDirectUsers(CmpInst *CmpI);

        HAKCModuleAnalysis *ModAnalysis;

        HAKCSystemInformation *SysInfo;

    public:
        // HAKCFunctionAnalysis(Function *F, bool debug);
        HAKCFunctionAnalysis(Function *F, HAKCModuleAnalysis *ModAnalysis, bool debug);

        ~HAKCFunctionAnalysis() = default;

        bool modifiedFunction();

        void InstrumentCompartmentalizedCode();

        void InstrumentKernelCode();

        void setup();

        std::set<std::string> GetNoTransferFunctions();

        std::set<std::string> GetSafeTransitionFunctions();

        std::set<hakc_transfer_def_t> GetHAKCTransferFunctions();

        std::map<std::string, HAKCAllocationSize> GetKernelAllocationSizeMap();

        std::set<std::string> GetIgnoredTypes();

        std::set<hakc_function_def_t> GetHAKCFunctions();

        Value *getDef(Value *, bool, bool);

        Instruction *
        FindUseInsertionPoint(Value *v, std::set<Instruction *> &users);

        Value *
        AddDataAuthCheckAtLocation(Value *signed_ptr, Instruction *location);

        Value *AddCodeAuthCheckAtLocation(Value *SignedPtr, Instruction *Location);

        Value *AddSafePointerCreationAtLocation(Value *SignedPtr, Instruction *Location);

        bool isCompartmentalizedFunction();

        Function &getFunction();

        Instruction *CreateMissingTransfer(Instruction *PointerNeedingTransfer);

        Instruction *GetFinalAllocaDef(AllocaInst *Alloca);

        bool isIntrinsicNeedingAuthentication(CallInst *);

        bool PointerIsAuthenticated_Arch(Value *Pointer);

        unsigned GetCompartmentTransferCount();

        unsigned GetDataAuthenticationCount();

        unsigned GetCodeAuthenticationCount();

        bool PointerShouldBeConsideredCode(Value *Pointer);
    };

}// namespace hakc

#endif//HAKC_HAKCFUNCTIONANALYSIS_H
