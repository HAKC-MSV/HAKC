//
// Created by derrick on 9/8/21.
//

#ifndef PMC_HAKCTYPEIDENTIFIER_H
#define PMC_HAKCTYPEIDENTIFIER_H

#include "llvm/ADT/SmallString.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/MD5.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Transforms/Utils/Local.h"

#include "HAKCAnalysis/CommonHAKCAnalysis.h"

#include "HAKCDebugInfoProcessor.h"
#include "HAKCFunctionInfo.h"
#include "HAKCGlobalInfo.h"

#include <map>
#include <set>

using namespace llvm;

namespace hakc {

    class HAKCTypeIdentifier : public HAKCDebugInfoProcessor {
    public:
        HAKCTypeIdentifier(Module &M, CommonHAKCAnalysis *Analysis);

        void OutputYAML(raw_ostream &out);
        static std::string GetTransformedPath(StringRef Path);

    protected:

        CommonHAKCAnalysis *AnalysisHelper;
        std::shared_ptr<HAKCTypeInfo> HandleType(const DIType *type);
        std::shared_ptr<HAKCTypeInfo> FindType(const DIType *type);
        void AddTypeMapping(const DIType* type, const std::shared_ptr<HAKCTypeInfo>& HAKCType);
        std::string GetTypeName(const DIType *type);
        std::shared_ptr<HAKCGlobalInfo> HandleGlobal(const DIGlobalVariable* DIGV);
        void AddGlobalMapping(const DIGlobalVariable* DIGV, const std::shared_ptr<HAKCGlobalInfo>& HAKCSymbol);
        void AddLLVMTypeMapping(const std::shared_ptr<HAKCTypeInfo> &HAKCType, Type *Ty);
        GlobalVariable *FindGlobal(const DIGlobalVariable *DIGV);
        std::shared_ptr<HAKCFunctionInfo> HandleFunction(const DISubprogram *SubProg);
        void AddFunctionMapping(const DISubprogram *SubProg, const std::shared_ptr<HAKCFunctionInfo>& HAKCFunction);
        void FindAllGlobalsUsed(Value *V, std::set<GlobalObject*> &GlobalSet);
        void FindUsesInGlobals();
        void FinalizeTypes();
        void FindUsesInFunctions();
        void FindTypesInFunctions();
        unsigned GetAnonymousID(const DIType* type);
        bool LLVMTypeMappingSanityCheck(const DIType *type, Type *Ty);
        std::string ConstructStructName(StructType *StructTy);
        std::shared_ptr<hakc::HAKCSymbolInfo> AddUnmappedGlobal(GlobalObject *GlobalObj);
        std::shared_ptr<hakc::HAKCFunctionInfo> AddUnmappedFunction(Function *F);
        void AddUsedGlobals(std::set<GlobalObject*> &GlobalObjects, const std::shared_ptr<hakc::HAKCSymbolInfo>& UserSymbol);

        std::shared_ptr<HAKCSymbolInfo> FindSymbol(Value *V, bool SearchUnmapped = false);
        std::shared_ptr<HAKCFunctionInfo> FindFunction(Function *F, bool SearchUnmapped = false);
        std::shared_ptr<HAKCGlobalInfo> FindGlobal(GlobalVariable *GV, bool SearchUnmapped = false);
        std::shared_ptr<HAKCTypeInfo> FindCalledFunctionType(FunctionType *FunctionTy);
        std::shared_ptr<HAKCTypeInfo> FindPointerType(PointerType *PointerTy);

        std::shared_ptr<HAKCTypeInfo> FindType(Type *Ty);

        void FindIndirectCallSource(CallInst *CallI, std::vector<std::shared_ptr<HAKCIndirectCallSourceLink>> &Path);
        void CreateIndirectCallSourceLink(Value *V, std::vector<std::shared_ptr<HAKCIndirectCallSourceLink>> &Path);

    protected:
        std::map<const DIType*, std::shared_ptr<HAKCTypeInfo>> types;
        std::map<const DIGlobalVariable*, std::shared_ptr<HAKCGlobalInfo>> globals;
        std::map<const DISubprogram*, std::shared_ptr<HAKCFunctionInfo>> functions;
        std::map<std::shared_ptr<HAKCTypeInfo>, std::set<Type*>> LLVMTypeMapping;
        std::map<const DIType*, unsigned> AnonymousNumberMapping;
        std::set<std::shared_ptr<HAKCGlobalInfo>> UnmappedGlobals;
        std::set<std::shared_ptr<HAKCFunctionInfo>> UnmappedFunctions;
        unsigned CurrentAnonID;
    };

}// namespace hakc


#endif//PMC_HAKCTYPEIDENTIFIER_H
