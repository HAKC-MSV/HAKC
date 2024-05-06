//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCFUNCTIONANALYSIS_H
#define HAKC_HAKCFUNCTIONANALYSIS_H

#include "llvm/IR/Dominators.h"

#include "HAKCModuleAnalysis.h"
#include "HAKCTransformers/HAKCTransformer.h"
#include "HAKCPointerManager.h"

namespace hakc {

    class HAKCModuleAnalysis;

    class CommonHAKCAnalysis;

    class HAKCPointerManager;

    template<unsigned ArgNo>
    llvm::Value *CallArgumentSize(llvm::Value *Call) {
        if (auto *CallB = dyn_cast<CallBase>(Call)) {
            auto *ArgTy = CallB->getArgOperand(ArgNo)->getType();
            auto Size = CallB->getFunction()->getParent()->getDataLayout().getTypeStoreSize(ArgTy);
            if (Size > 0) {
                return ConstantInt::get(IntegerType::getInt64Ty(CallB->getContext()), Size);
            }
        }

        return nullptr;
    }

    template<unsigned argNo>
    llvm::Value *simpleArgumentSize(llvm::Value *allocation) {
        if (llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(allocation)) {
            IRBuilder<> irBuilder(call);
            Value *size = call->getArgOperand(argNo);
            size = irBuilder.CreateZExtOrBitCast(size, irBuilder.getInt64Ty());
            return size;
        }

        return nullptr;
    }

    template<unsigned size>
    llvm::Value *simpleStaticSize(llvm::Value *allocation) {
        return llvm::ConstantInt::get(Type::getInt64Ty(allocation->getContext()), size, false);
    }

    template<unsigned size, unsigned argNo>
    llvm::Value *staticPlusArgument(llvm::Value *allocation) {
        if (llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(allocation)) {
            ConstantInt *argumentSize = dyn_cast<ConstantInt>(call->getArgOperand(argNo));
            return ConstantInt::get(Type::getInt64Ty(allocation->getContext()), argumentSize->getZExtValue() + size,
                                    false);
        }

        return nullptr;
    }

    template<unsigned argNo1, unsigned argNo2>
    Value *multiplyTwoArguments(Value *allocation) {
        if (CallInst *call = dyn_cast<CallInst>(allocation)) {
            IRBuilder<> irBuilder(call);
            auto *int64Ty = irBuilder.getInt64Ty();
            /* Defying all reason, somehow some functions have different argument counts than
             * expected. See kmalloc_array in the IR for linereq_ioctl. So in that case, take
             * the lowest argument value.
             */
            Value *fullSize = nullptr;
            if (argNo1 >= call->getNumArgOperands() || argNo2 >= call->getNumArgOperands()) {
                if (argNo1 <= argNo2) {
                    fullSize = call->getArgOperand(argNo1);
                } else {
                    fullSize = call->getArgOperand(argNo2);
                }
            } else {
                fullSize = irBuilder.CreateMul(
                        irBuilder.CreateZExt(call->getArgOperand(argNo1), int64Ty),
                        irBuilder.CreateZExt(call->getArgOperand(argNo2), int64Ty));
            }
            fullSize = irBuilder.CreateZExtOrBitCast(fullSize, int64Ty);
            return fullSize;
        }

        return nullptr;
    }

    template<unsigned argNo, unsigned index0>
    Value *argumentGEP(Value *allocation) {
        if (CallInst *call = dyn_cast<CallInst>(allocation)) {
            /*HAKCIRBuilder<> irBuilder(call);
            IntegerType *sizeTy = irBuilder.getInt64Ty();
            std::vector<Value*> indices;
            indices.push_back(ConstantInt::get(sizeTy, index0, false));
            Value *gep = irBuilder.CreateGEP(sizeTy, call->getArgOperand(argNo), indices);
            Value *size = irBuilder.CreateLoad(sizeTy, gep);
            return size;*/

            // TODO: Fix this
            return llvm::ConstantInt::get(Type::getInt64Ty(allocation->getContext()), 64, false);
        }

        return nullptr;
    }

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

        bool IsManualSafePointer(CallInst *Call);

        void HandleInstruction(Instruction *I);

        Instruction *getUserInst(User *user);

        bool isPHIofGlobalsOnly(Value *ptr, std::set<PHINode *> &nodes);

        void RegisterPointerDereference(Use &use);

        virtual void handleLoad(LoadInst *load);

        virtual void handleStore(StoreInst *store);

        virtual void handleComparison(CmpInst *compare);

        virtual void handleCall(CallInst *call);

        virtual void handleBinaryOperator(BinaryOperator *binOp);

        bool globalShouldBeTransferred(Use &globalValueArg);

        virtual void relocateFunctionSection();

        virtual std::string getHAKCFunctionSectionName();

        void CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls();

        virtual HAKCModuleAnalysis &getModuleAnalysis() = 0;

        HAKCTransformer &getTransformer() override;

        virtual std::set<Intrinsic::ID> GetIntrinsicsNeedingAuthenticatedArgs();

        virtual std::set<Intrinsic::ID> GetInstrinsicsToSkip();

        virtual std::set<Intrinsic::ID> GetIntrinsicsToClone();

        virtual void AddManagedPointer(Value *HAKCPointer);

        void ReplaceInstructionOperand(Instruction *I, unsigned ArgNo, Value *OldValue, Value *NewValue);

        void ReplaceDirectFunctionUsesWithTransfers();

        void CheckCompareOperandForDirectFunctionUse(CmpInst *CmpI, unsigned OpNo);

        void MaybeAddCompareToDirectUsers(CmpInst *CmpI);

        virtual std::set<StringRef> GetSafePointerFunctionNames() = 0;

        virtual void UpdateHAKCFunctionParameters();

        virtual void UpdateHAKCFunctionParameters_Arch(CallInst *CallI, hakc_compartment_id_t TargetID,
                                                       hakc_transfer_def_t &HAKCTransferFunction) = 0;

        void AddInstrumentation(bool RelocateSection);

    public:
        HAKCFunctionAnalysis(Function *F, bool debug);

        virtual ~HAKCFunctionAnalysis() = default;

        bool modifiedFunction();

        void InstrumentCompartmentalizedCode();

        void InstrumentKernelCode();

        virtual void setup();

        std::set<StringRef> GetNoTransferFunctions() override;

        std::set<StringRef> GetSafeTransitionFunctions() override;

        std::set<hakc_transfer_def_t> GetHAKCTransferFunctions() override;

        std::map<StringRef, hakc_allocation_size_map_t> GetKernelAllocationSizeMap() override;

        std::set<StringRef> GetIgnoredTypes() override;

        std::set<StringRef> GetIgnoredGlobals() override;

        std::set<hakc_function_def_t> GetHAKCFunctions() override;

        Value *getDef(Value *, bool, bool) override;

        Instruction *
        FindUseInsertionPoint(Value *v, std::set<Instruction *> &users);

        Value *
        AddDataAuthCheckAtLocation(Value *signed_ptr, Instruction *location);

        Value *AddCodeAuthCheckAtLocation(Value *SignedPtr, Instruction *Location);

        Value *AddSafePointerCreationAtLocation(Value *SignedPtr, Instruction *Location);

        bool isCompartmentalizedFunction();

        Function &getFunction();

        Instruction *CreateMissingTransfer(Instruction *PointerNeedingTransfer);

        virtual Instruction *SignGlobalPointerWithColor(GlobalValue *GlobalVar);

        virtual Instruction *GetFinalAllocaDef(AllocaInst *Alloca);

        virtual bool IsIntrinsicNeedingAuthentication(CallBase *Call);

        virtual bool IsIntrinsicsNeedingCloning(CallBase *Call);

        virtual bool PointerIsAuthenticated_Arch(Value *Pointer);

        unsigned GetCompartmentTransferCount() const;

        unsigned GetDataAuthenticationCount();

        unsigned GetCodeAuthenticationCount();

        virtual bool PointerShouldBeConsideredCode(Value *Pointer);

        virtual bool PointerShouldBeManaged(Use &use);

        bool IsPHIOfGlobalsOnly(Value *V);

    };

} // hakc

#endif //HAKC_HAKCFUNCTIONANALYSIS_H
