//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_COMMONHAKCANALYSIS_H
#define HAKC_COMMONHAKCANALYSIS_H

#include "HAKCPass.h"
#include "HAKCSystemInformation.h"
#include "HAKCFunctionDefinition/HAKCFunctionDefinition.h"
#include "HAKCFunctionDefinition/HAKCTransferFunction.h"

namespace hakc {

    class HAKCTransformer;

    typedef std::function<llvm::Value *(llvm::Value *)> hakc_allocation_size_map_t;

    class CommonHAKCAnalysis {
    protected:
        /**
        * @brief Set to true to output debugging information
        */
        bool debug_output;

        std::map<Value *, std::vector<Value *>> DefchainCache;

        explicit CommonHAKCAnalysis(bool debug);

        bool isHAKCFunction(Function *F);

        bool isFunctionStatic(Function *F);

        virtual bool isSafeTransitionFunction(Function *F);

        bool functionIsAnalysisCandidate(Function *F);

        bool valueShouldBeReplacedWithTransfer(Value *V);

        hakc_function_def_t getHAKCFunction(StringRef name);

        Module &getModule();

        virtual HAKCTransformer &getTransformer() = 0;

        virtual bool functionIsModParamGetCtx(Function *F);

        virtual bool IsNoTransferFunction(Function *F);

        static std::set<StringRef> AddToSet(std::set<StringRef> Existing, ArrayRef<StringRef> NewAdditions);

        virtual bool ValueIsUsedAsPointer(Value *V, bool debug);

    public:

        virtual Value *getDef(Value *V, bool followLoad, bool debug);

        virtual std::vector<Value *> findDefChain(Value *v, bool followLoad, bool debug);

        static bool argShouldTransfer(Value *V);

        static bool isPerCPUPointer(Use &U);

        static bool isPerCPUPointer(Value *V);

        static bool isKernelUserPointer(Use &U);

        static bool isKernelUserPointer(Value *V);

        virtual bool valueIsReadonlyPtr(Value *value);

        static bool FunctionHasPointerArg(Function *F);

        static bool isOutsideTransferFunc(Function *F);

        static bool isCapabilityReassignmentFunc(Function *F);

        static bool NoKernelTransferFunctionsSet();

        static bool IsKernelCompartment(hakc_compartment_id_t ID);

        static bool IsPointerLikeType(Type *Ty);

        std::string getOutsideTransferName(Function *F);

        static std::string getVariadicTransferName(Function *F);

        static std::string getOriginalTransformedName(Function *F);

        static std::string getHAKCDebugName();

        virtual std::set<StringRef> GetNoTransferFunctions() = 0;

        virtual std::set<StringRef> GetSafeTransitionFunctions() = 0;

        virtual std::set<StringRef> GetIgnoredTypes() = 0;

        bool IsHAKCTransferFunction(Function *F);

        bool IsHAKCFunction(Function *F);

        virtual std::set<hakc_transfer_def_t> GetHAKCTransferFunctions() = 0;

        virtual std::set<hakc_function_def_t> GetHAKCFunctions() = 0;

        virtual std::map<StringRef, hakc_allocation_size_map_t> GetKernelAllocationSizeMap() = 0;

        hakc_transfer_def_t GetHAKCTransferDef(StringRef name);

        virtual std::set<StringRef> GetIgnoredGlobals();

        virtual bool functionIsTransferCandidate(Function *f);

        static raw_ostream &getWriter();

        static unsigned getCompartmentStorageSizeInBits();

        static bool FunctionIsComplexVariadic(Function *F);

        static StringRef GetFunctionName(Function *F);

        static bool isRegisterRead(Value *v);

        bool IsKernelFunction(Function *F);

        bool isIgnoredType(Type *Ty);

        bool FunctionsAreInSameCompartment(Function *F, Function *G);

        bool callIsSafeTransition(CallBase *call);

        bool IsKernelAllocation(Value *V);

        bool IsCompartmentalizedFunction(Function *F);

        static bool IsStringType(Type *Ty);

        static Instruction *GetTargetTypeCast(Instruction *I, Type *TargetType);

        virtual std::set<Intrinsic::ID> GetBitshiftIntrinsics();

        virtual std::set<Instruction::BinaryOps> GetPointerManipulatingBinaryOps();

        bool IsCallInIntrinsicSet(CallBase *Call, std::set<Intrinsic::ID> &IntrinsicsSet);

        static std::string GetModuleFullPath(Module &M);

    private:
        static bool valueHasAttribute(Value *v, Attribute::AttrKind Kind);

        static bool useHasAttribute(Use &U, Attribute::AttrKind Kind);

    };

} // hakc

#endif //HAKC_COMMONHAKCANALYSIS_H
