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

    typedef std::function<llvm::Value *(llvm::Value *)> hakc_allocation_size_map_t;

    class CommonHAKCAnalysis {
    protected:
        Module &M;

        std::map<Value *, SmallVector<Value *>> DefchainCache;

        HAKCSystemInformation SystemInfo;

        bool FunctionIsAnalysisCandidate(Function *F);

        bool valueShouldBeReplacedWithTransfer(Value *V, HAKCCompartmentalizationPolicy &Policy);

        hakc_function_def_t getHAKCFunction(StringRef name);

        bool functionIsModParamGetCtx(Function *F);

        bool IsNoTransferFunction(Function *F);

        static bool IsFunctionInFunctionList(Function *F, iterator_range<FunctionList::iterator> Range);

        static bool IsFunctionInHAKCFunctionList(Function *F, iterator_range<HAKCFunctionList::iterator> Range);

        static bool IsFunctionInHAKCTransferFunctionList(Function *F, iterator_range<HAKCTransferList::iterator> Range);

    public:
        explicit CommonHAKCAnalysis(Module &M, StringRef ConfigPath);

        HAKCSystemInformation &GetSystemInfo();

        Module &GetModule();

        Value *getDef(Value *V, bool followLoad);

        void findDefChain(Value *v, bool followLoad, SmallVectorImpl<Value*> &Results);

        static bool argShouldTransfer(Value *V);

        static bool isPerCPUPointer(Value *V);

        static bool isKernelUserPointer(Value *V);

        bool valueIsReadonlyPtr(Value *value);

        static bool FunctionIsStatic(Function *F);

        static bool FunctionHasPointerArg(Function *F);

        static bool isOutsideTransferFunc(Function *F);

        static bool isCapabilityReassignmentFunc(Function *F);

        static bool NoKernelTransferFunctionsSet();

        static bool IsPointerLikeType(Type *Ty);

        std::string GetOutsideTransferName(Function *F);

        bool IsSafeTransitionFunction(Function *F);

        static std::string getVariadicTransferName(Function *F);

        static std::string getOriginalTransformedName(Function *F);

        bool IsHAKCTransferFunction(Function *F);

        bool IsHAKCValidationFunction(Function *F);

        bool IsHAKCCompartmentalizationSupportFunction(Function *F);

        bool IsHAKCFunction(Function *F);

        bool functionIsTransferCandidate(Function *F, HAKCCompartmentalizationPolicy &Policy);

        static hakc::HAKCOstream &getWriter();

        static FunctionType *GetDataAuthenticationFunctionType(Module &M, unsigned AddrSpace = 0);

        static FunctionType *GetCodeAuthenticationFunctionType(Module &M, unsigned AddrSpace = 0);

        static FunctionType *GetTransferFunctionType(Module &M, unsigned AddrSpace = 0);

        static unsigned getCompartmentStorageSizeInBits();

        static bool FunctionIsComplexVariadic(Function *F);

        static StringRef GetFunctionName(Function *F);

        static bool isRegisterRead(Value *v);

        bool IsIgnoredType(Type *Ty);

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
