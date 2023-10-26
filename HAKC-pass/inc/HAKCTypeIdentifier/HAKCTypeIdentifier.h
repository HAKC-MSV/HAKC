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

#include "HAKCFunctionInfo.h"
#include "HAKCGlobalInfo.h"
#include "HAKCTypeInfo.h"
#include "HAKCDebugInfoProcessor.h"

#include <map>
#include <set>

using namespace llvm;

namespace hakc {

    class HAKCTypeInfo;

    class HAKCTypeIdentifier : public HAKCDebugInfoProcessor {
    public:
        HAKCTypeIdentifier(Module &M, CommonHAKCAnalysis *Analysis);

        void outputTypes(raw_fd_ostream &out);

        std::shared_ptr<HAKCTypeInfo> getHAKCType(const DIType *type);

        std::shared_ptr<HAKCTypeInfo> getHAKCType(Type *type);

        std::string getCallOperandOrigin(CallInst *call);

        std::string getStoreOperandOrigin(StoreInst *storeInst);

        Value *GetDef(Value *V);

        std::vector<Value *> GetDefChain(Value *V);

    protected:

        CommonHAKCAnalysis *AnalysisHelper;

        std::shared_ptr<HAKCTypeInfo> addType(const DIType *diType, GlobalObject *GO);

        std::shared_ptr<HAKCTypeInfo> addType(const DIType *diType, Type *Ty);

        std::shared_ptr<HAKCTypeInfo> addNoDebugType(GlobalObject *GO);

        bool GlobalShouldBeSkipped(GlobalVariable *GV);

        void addEscapingSymbol(Function *F, std::string &escapingSymbol);

        void addUseInIndirectCall(Function *F);

        void addUseInUnknownOriginStore(Function *F);

        Value *getOperandOrigin(Value *operand);

        std::string getOperandString(Value *operand, std::shared_ptr<HAKCTypeInfo> hakcType);

        std::string getOperandOriginString(Value *operand);

        std::string getUseOriginString(Use *use, std::shared_ptr<HAKCTypeInfo> hakcType);

        void findEscapes();

        std::set<std::pair<std::string, std::string>>
        getOperandStringTokens(Value *operand, std::shared_ptr<HAKCTypeInfo> &hakcType);

        std::string combineTokens(std::set<std::pair<std::string, std::string>> &tokens);

        bool diTypeShouldBeAnalyzed(const DIType *diType);

    protected:
        std::set<std::shared_ptr<HAKCTypeInfo>> types;
        std::set<std::shared_ptr<HAKCGlobalInfo>> symbols;
        std::set<const DIType *> opaquePtrs;

        void AddUserToType(std::shared_ptr<HAKCTypeInfo> &HakcType, User *User);
    };

}// namespace hakc


#endif//PMC_HAKCTYPEIDENTIFIER_H
