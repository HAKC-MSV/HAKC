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
//
//        std::shared_ptr<HAKCTypeInfo> getHAKCType(const DIType *type);
//
//        std::shared_ptr<HAKCTypeInfo> getHAKCType(Type *type);
//
//        std::string getCallOperandOrigin(CallInst *call);
//
//        std::string getStoreOperandOrigin(StoreInst *storeInst);
//
//        Value *GetDef(Value *V);
//
//        std::vector<Value *> GetDefChain(Value *V);
//
        static std::string GetTransformedPath(StringRef Path);
//
//        Module &GetModule();

    protected:

        CommonHAKCAnalysis *AnalysisHelper;

//        std::shared_ptr<HAKCTypeInfo> addType(const DIType *diType, GlobalObject *GO);
//
//        std::shared_ptr<HAKCTypeInfo> addType(const DIType *diType, Type *Ty);
//
//        std::shared_ptr<HAKCTypeInfo> addNoDebugType(GlobalObject *GO);
//
//        bool GlobalShouldBeSkipped(GlobalVariable *GV);
//
//        void addEscapingSymbol(Function *F, std::string &escapingSymbol);
//
//        void addUseInIndirectCall(Function *F);
//
//        void addUseInUnknownOriginStore(Function *F);
//
//        Value *getOperandOrigin(Value *operand);
//
//        std::string getOperandString(Value *operand, std::shared_ptr<HAKCTypeInfo> hakcType);
//
//        std::string getOperandOriginString(Value *operand);
//
//        std::string getUseOriginString(Use *use, std::shared_ptr<HAKCTypeInfo> hakcType);
//
//        void findEscapes();
//
//        std::set<std::pair<std::string, std::string>>
//        getOperandStringTokens(Value *operand, std::shared_ptr<HAKCTypeInfo> &hakcType);
//
//        std::string combineTokens(std::set<std::pair<std::string, std::string>> &tokens);
//
        bool DiTypeShouldBeAnalyzed(const DIType *diType);
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

        std::shared_ptr<HAKCSymbolInfo> FindSymbol(Value *V);

    protected:
        std::map<const DIType*, std::shared_ptr<HAKCTypeInfo>> types;
        std::map<const DIGlobalVariable*, std::shared_ptr<HAKCGlobalInfo>> globals;
        std::map<const DISubprogram*, std::shared_ptr<HAKCFunctionInfo>> functions;
        std::map<std::shared_ptr<HAKCTypeInfo>, std::set<Type*>> LLVMTypeMapping;
        std::map<const DIType*, unsigned> AnonymousNumberMapping;
        unsigned CurrentAnonID;
    };

}// namespace hakc


#endif//PMC_HAKCTYPEIDENTIFIER_H
