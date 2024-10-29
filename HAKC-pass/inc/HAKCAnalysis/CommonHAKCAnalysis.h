//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_COMMONHAKCANALYSIS_H
#define HAKC_COMMONHAKCANALYSIS_H

#include "HAKC-defs.h"
#include "HAKCFunctionDefinition/HAKCFunctionDefinition.h"
#include "HAKCFunctionDefinition/HAKCTransferFunction.h"
#include "HAKCPass.h"
#include "HAKCSystemInformation.h"

// #include "HAKCCompartment.h"
// #include "HAKCSymbol.h"
// #include "HAKCAllocationSize.h"
// #include "HAKCTransformers/HAKCTransformer.h"

// #include "HAKCTypeIdentifier/HAKCFunctionInfo.h"
// #include "HAKCGlobalInfo.h"
// #include "HAKCTypeInfo.h"
// #include "HAKCTypeIdentifier/HAKCDebugInfoProcessor.h"
// #include "HAKCAnalysis/HAKCModuleAnalysis.h"
// #include "HAKCPass.h"
// #include "HAKCTypeIdentifier/HAKCGlobalInfo.h"
// #include "HAKCTypeIdentifier/HAKCTypeInfo.h"
// #include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"
// #include "HAKCAllocationSize.h"
// #include "HAKCYAMLParser.h"
// #include "HAKCPointerManager.h"

namespace hakc {

    class HAKCTransformer;

    typedef std::function<llvm::Value *(llvm::Value *)> hakc_allocation_size_map_t;

    class CommonHAKCAnalysis {
    protected:
        /**
        * @brief Set to true to output debugging information
        */
        bool debug_output;

        explicit CommonHAKCAnalysis(bool debug);

        bool isHAKCFunction(Function *F);

        bool isFunctionStatic(Function *F);

        bool isSafeTransitionFunction(Function *F);

        bool functionIsAnalysisCandidate(Function *F);

        // bool valueShouldBeReplacedWithTransfer(Value *V);

        hakc_function_def_t getHAKCFunction(StringRef name);

        // Module &getModule();

        virtual HAKCTransformer &getTransformer() = 0;
        // HAKCTransformer &getTransformer();

        bool functionIsModParamGetCtx(Function *F);

        bool IsNoTransferFunction(Function *F);

        static std::set<StringRef> AddToSet(std::set<StringRef> Existing, std::set<StringRef> NewAdditions);

    public:
        Value *getDef(Value *V, bool followLoad, bool debug);

        std::vector<Value *> findDefChain(Value *v, bool followLoad = false, bool debug = false);

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

        static bool IsKernelCompartment(hakc_compartment_id_t ID);

        std::string getOutsideTransferName(Function *F);

        static std::string getVariadicTransferName(Function *F);

        static std::string getOriginalTransformedName(Function *F);

        static std::string getHAKCDebugName();

        // virtual std::set<StringRef> GetNoTransferFunctions() = 0; 
        virtual std::set<std::string> GetNoTransferFunctions() = 0; 

        // virtual std::set<StringRef> GetSafeTransitionFunctions();
        virtual std::set<std::string> GetSafeTransitionFunctions() = 0;

        // virtual std::set<StringRef> GetIgnoredTypes() = 0;
        virtual std::set<std::string> GetIgnoredTypes() = 0;

        bool IsHAKCTransferFunction(Function *F);

        bool IsHAKCFunction(Function *F);

        std::set<hakc_transfer_def_t> GetHAKCTransferFunctions();

        virtual std::set<hakc_function_def_t> GetHAKCFunctions() = 0;
        // virtual std::set<hakc_function_def_t> GetHAKCFunctions();
        // std::set<hakc_function_def_t> GetHAKCFunctions();

        // virtual std::map<StringRef, hakc_allocation_size_map_t> GetKernelAllocationSizeMap() = 0;
        virtual std::map<std::string, HAKCAllocationSize> GetKernelAllocationSizeMap() = 0;

        hakc_transfer_def_t GetHAKCTransferDef(StringRef name);

        // std::set<StringRef> GetIgnoredGlobals();
        std::set<std::string> GetIgnoredGlobals();
        // virtual std::set<std::string> GetIgnoredGlobals() = 0; 

        bool functionIsTransferCandidate(Function *f);

        static raw_ostream &getWriter();

        static unsigned getCompartmentStorageSizeInBits();

        static bool FunctionIsComplexVariadic(Function *F);

        static StringRef GetFunctionName(Function *F);

        static bool isRegisterRead(Value *v);

        bool IsKernelFunction(Function *F);

        bool isIgnoredType(Type *Ty);

        bool FunctionsAreInSameCompartment(Function *F, Function *G);

        bool callIsSafeTransition(CallInst *call);

        bool IsKernelAllocation(Value *V);

        bool IsCompartmentalizedFunction(Function *F);

    private:
        static bool valueHasAttribute(Value *v, Attribute::AttrKind Kind);

        static bool useHasAttribute(Use &U, Attribute::AttrKind Kind);
    };

}// namespace hakc

#endif//HAKC_COMMONHAKCANALYSIS_H
