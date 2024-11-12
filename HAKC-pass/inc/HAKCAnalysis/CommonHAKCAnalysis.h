//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_COMMONHAKCANALYSIS_H
#define HAKC_COMMONHAKCANALYSIS_H

#include "HAKCPass.h"
#include "HAKCFunctionDefinition/HAKCFunctionDefinition.h"
#include "HAKCFunctionDefinition/HAKCTransferFunction.h"
#include "HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include "HAKCOstream.h"
#include "HAKCSystem/HAKCSystemInformation.h"


#include <map>

namespace hakc {

    class HAKCTransformer;

    class HAKCCompartmentalizationPolicy;

    typedef std::function<llvm::Value *(llvm::Value *)> hakc_allocation_size_map_t;

    class CommonHAKCAnalysis {
    protected:
        /**
        * @brief Set to true to output debugging information
        */
        bool debug_output;

        std::map<Value *, std::vector<Value *>> DefchainCache;
        HAKCSystemInformation SystemInfo;

        CommonHAKCAnalysis(Module &M, StringRef ConfigPath, bool debug);

        bool isHAKCFunction(Function *F);

        static bool isFunctionStatic(Function *F);

        bool isSafeTransitionFunction(Function *F);

        bool functionIsAnalysisCandidate(Function *F);

        bool valueShouldBeReplacedWithTransfer(Value *V, HAKCCompartmentalizationPolicy &Policy);

        hakc_function_def_t getHAKCFunction(StringRef name);

        virtual HAKCTransformer &getTransformer() = 0;

        bool functionIsModParamGetCtx(Function *F);

        bool IsNoTransferFunction(Function *F);

        static std::set<StringRef> AddToSet(std::set<StringRef> Existing, std::set<StringRef> NewAdditions);

        static std::set<StringRef> AddToSet(std::set<StringRef> Existing, const std::set<StringRef> &NewAdditions);

        static bool IsFunctionInList(Function *F, iterator_range<HAKCFunctionList::iterator> Range);

    public:
        HAKCSystemInformation& GetSystemInfo();

        Value *getDef(Value *V, bool followLoad, bool debug);

        virtual std::vector<Value *> findDefChain(Value *v, bool followLoad, bool debug);

        static bool argShouldTransfer(Value *V);

        static bool isPerCPUPointer(Use &U);

        static bool isPerCPUPointer(Value *V);

        static bool isKernelUserPointer(Use &U);

        static bool isKernelUserPointer(Value *V);

        bool valueIsReadonlyPtr(Value *value);

        static bool FunctionHasPointerArg(Function *F);

        static bool isOutsideTransferFunc(Function *F);

        static bool isCapabilityReassignmentFunc(Function *F);

        static bool NoKernelTransferFunctionsSet();

        static bool IsPointerLikeType(Type *Ty);

        std::string getOutsideTransferName(Function *F);

        static std::string getVariadicTransferName(Function *F);

        static std::string getOriginalTransformedName(Function *F);

        static std::string getHAKCDebugName();

        static std::string GetDBPath();

        bool IsHAKCTransferFunction(Function *F);

        bool IsHAKCFunction(Function *F);

        virtual std::set<hakc_function_def_t> GetHAKCFunctions() = 0;

        bool functionIsTransferCandidate(Function *F, HAKCCompartmentalizationPolicy &Policy);

        static hakc::HAKCOstream &getWriter();

        static unsigned getCompartmentStorageSizeInBits();

        static bool FunctionIsComplexVariadic(Function *F);

        static StringRef GetFunctionName(Function *F);

        static bool isRegisterRead(Value *v);

        bool isIgnoredType(Type *Ty);

        bool IsIgnoredGlobal(Value *V);

        static bool FunctionsAreInSameCompartment(Function *F, Function *G, HAKCCompartmentalizationPolicy &Policy);

        bool callIsSafeTransition(CallBase *call);

        bool IsKernelAllocation(Value *V);

        static bool IsCompartmentalizedFunction(Function *F, HAKCCompartmentalizationPolicy &Policy);

        static bool IsStringType(Type *Ty);

        static Instruction *GetTargetTypeCast(Instruction *I, Type *TargetType);

        virtual std::set<Intrinsic::ID> GetBitshiftIntrinsics();

        virtual std::set<Instruction::BinaryOps> GetPointerManipulatingBinaryOps();

        bool IsCallInIntrinsicSet(CallBase *Call, std::set<Intrinsic::ID> &IntrinsicsSet) const;

        static std::string GetModuleFullPath(Module &M);

        static bool IsMultiSSAUser(Value *V);

        static bool IsConstantUsedInGlobal(Value *V);

        static void SortGlobalList(std::vector<GlobalVariable *> &GlobalList);

        static void SortFunctionList(std::vector<Function *> &FuncList);

        static bool IsKernelSymbol(GlobalValue *GV, HAKCCompartmentalizationPolicy &Policy);

        static void VerifyFunction(Function *F);

    private:
        static bool valueHasAttribute(Value *v, Attribute::AttrKind Kind);

        static bool useHasAttribute(Use &U, Attribute::AttrKind Kind);
    };

}// namespace hakc

#endif//HAKC_COMMONHAKCANALYSIS_H
