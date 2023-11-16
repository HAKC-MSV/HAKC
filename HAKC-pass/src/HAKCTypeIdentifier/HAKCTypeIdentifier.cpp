//
// Created by derrick on 9/8/21.
//
#include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"
#include "HAKC-defs.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "HAKCTypeIdentifier/HAKCFunctionInfo.h"
#include "HAKCTypeIdentifier/HAKCGlobalInfo.h"
#include "HAKCTypeIdentifier/HAKCTypeInfo.h"

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/IR/DerivedTypes.h"

#include <sstream>

static std::string UNKNOWN_ORIGIN = "unknown-origin";

std::string
hakc::HAKCTypeIdentifier::getStoreOperandOrigin(StoreInst *storeInst) {
    std::string result = getOperandOriginString(storeInst->getPointerOperand());

    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Found origin hash for store ";
        storeInst->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << ": " << result << "\n";
    }
    return result;
}

Value *hakc::HAKCTypeIdentifier::getOperandOrigin(Value *operand) {
    Value *result = nullptr;
    std::vector<Value *> defChain =
            AnalysisHelper->findDefChain(operand, true, debug);
    for (Value *v: defChain) {
        if (isa<GetElementPtrInst>(v)) {
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Found GEP for ";
                operand->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << ": ";
                v->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            result = v;
            break;
        }
    }

    if (!result) {
        result = defChain.front();
    }

    return result;
}

std::set<std::pair<std::string, std::string>>
hakc::HAKCTypeIdentifier::getOperandStringTokens(
        Value *operand, std::shared_ptr<HAKCTypeInfo> &hakcType) {
    std::set<std::pair<std::string, std::string>> result;

    if (hakcType) {
        result.insert(std::make_pair("type", hakcType->getHash()));
        if (auto *gep = dyn_cast<GetElementPtrInst>(operand)) {
            APInt offset(64, 0, true);
            bool foundOffset =
                    gep->accumulateConstantOffset(M.getDataLayout(), offset);
            if (foundOffset) {
                result.insert(
                        std::make_pair("offset", std::to_string(offset.getSExtValue())));
                /* Ensure that we record all type accesses through indirections */
                std::vector<Value *> indices;
                unsigned i = 0;
                for (auto &index: gep->indices()) {
                    if (i < gep->getNumIndices() - 1) {
                        indices.push_back(index.get());
                        Type *accessedType = GetElementPtrInst::getIndexedType(gep->getSourceElementType(), indices);
                        auto accessedHakcType = getHAKCType(accessedType);
                        if (accessedHakcType) {
                            accessedHakcType->addUser(gep->getFunction());
                        }
                    }
                    i++;
                }
            }
        } else if (auto *argument = dyn_cast<Argument>(operand)) {
            result.insert(
                    std::make_pair("argument", std::to_string(argument->getArgNo())));
        } else if (auto *gv = dyn_cast<GlobalVariable>(operand)) {
            result.insert(std::make_pair("global-name", gv->getName().str()));
        } else if (auto *f = dyn_cast<Function>(operand)) {
            result.insert(std::make_pair("function-name", f->getName().str()));
        }
    }
    return result;
}

std::string hakc::HAKCTypeIdentifier::combineTokens(
        std::set<std::pair<std::string, std::string>> &tokens) {
    std::string result;
    if (!tokens.empty()) {
        result = "{ ";
        unsigned idx = 0;
        for (auto &it: tokens) {
            result += it.first;
            result += ": ";
            result += it.second;
            idx++;
            if (idx < tokens.size()) {
                result += ", ";
            }
        }
        result += " }";
    }
    return result;
}

std::string
hakc::HAKCTypeIdentifier::getOperandString(Value *operand,
                                           std::shared_ptr<HAKCTypeInfo> hakcType) {
    std::string result = UNKNOWN_ORIGIN;

    auto tokens = getOperandStringTokens(operand, hakcType);
    if (!tokens.empty()) {
        result = combineTokens(tokens);
    }

    return result;
}

std::string hakc::HAKCTypeIdentifier::getOperandOriginString(Value *operand) {
    Value *def;
    std::string result;
    std::shared_ptr<hakc::HAKCTypeInfo> hakcType = nullptr;

    def = getOperandOrigin(operand);
    if (auto *gep = dyn_cast<GetElementPtrInst>(def)) {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Finding type for GEP ";
            gep->getSourceElementType()->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }
        hakcType = getHAKCType(gep->getSourceElementType());
        if (!hakcType) {
            hakcType = getHAKCType(gep->getSourceElementType()->getPointerTo());
        }
        if (!hakcType && gep->getSourceElementType()->isArrayTy()) {
            hakcType = getHAKCType(gep->getSourceElementType()->getArrayElementType());
        }
        if (!hakcType) {
            auto type = getValueMetadataDIType(operand);
            if (type) {
                hakcType = getHAKCType(type);
            }
        }
    } else if (!isa<Operator>(def) /* Yes, subr_kdb.c in FreeBSD uses an inttoptr cast for some code! */ ) {
        hakcType = getHAKCType(def->getType());
        if (!hakcType) {
            auto type = getValueMetadataDIType(operand);
            if (type) {
                hakcType = getHAKCType(type);
            }
        }
    }

    result = getOperandString(def, hakcType);

    return result;
}

std::string hakc::HAKCTypeIdentifier::getUseOriginString(
        Use *use, std::shared_ptr<HAKCTypeInfo> hakcType) {
    std::string result = UNKNOWN_ORIGIN;
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Getting use origin of ";
        if (auto *F = dyn_cast<Function>(use->get())) {
            CommonHAKCAnalysis::getWriter() << F->getName();
        } else {
            use->get()->print(CommonHAKCAnalysis::getWriter());
        }
        CommonHAKCAnalysis::getWriter() << "\n";
        CommonHAKCAnalysis::getWriter() << "hakcType: " << hakcType->getTypeStringRepresentation() << "\n";
        hakcType->getType()->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\n";
    }
    auto tokens = getOperandStringTokens(use->getUser(), hakcType);
    if (!tokens.empty()) {
        if (auto *constStruct =
                dyn_cast<ConstantStruct>(use->getUser())) {
            const StructLayout *layout = getStructLayout(constStruct->getType());
            tokens.insert(std::make_pair(
                    "offset",
                    std::to_string(layout->getElementOffset(use->getOperandNo()))));
        } else if (isa<CallInst>(use->getUser())) {
            tokens.insert(
                    std::make_pair("argument", std::to_string(use->getOperandNo())));
        }

        result = combineTokens(tokens);
    }

    return result;
}

std::string hakc::HAKCTypeIdentifier::getCallOperandOrigin(CallInst *call) {
    std::string result = getOperandOriginString(call->getCalledOperand());

    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Found origin hash for call ";
        call->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << ": " << result << "\n";
    }

    return result;
}

std::shared_ptr<hakc::HAKCTypeInfo>
hakc::HAKCTypeIdentifier::getHAKCType(Type *type) {
    for (auto t: types) {
        if (t->getType() == type) {
            return t;
        }
    }

    return nullptr;
}

void hakc::HAKCTypeIdentifier::addUseInUnknownOriginStore(Function *F) {
    bool found = false;
    for (auto &f: symbols) {
        if (f->getValue() == F) {
            f->setUsedInUnknownOriginStore();
            found = true;
            break;
        }
    }

    if (!found && debug) {
        CommonHAKCAnalysis::getWriter() << "Could not find function " << F->getName() << "\n";
    }
}

void hakc::HAKCTypeIdentifier::addUseInIndirectCall(Function *F) {
    bool found = false;
    for (auto &f: symbols) {
        if (f->getValue() == F) {
            f->setUsedInIndirectCalls();
            found = true;
            break;
        }
    }

    if (!found && debug) {
        CommonHAKCAnalysis::getWriter() << "Could not find function " << F->getName() << "\n";
    }
}

Value *hakc::HAKCTypeIdentifier::GetDef(Value *V) {
    return AnalysisHelper->getDef(V, false, debug);
}

std::vector<Value *> hakc::HAKCTypeIdentifier::GetDefChain(Value *V) {
    return AnalysisHelper->findDefChain(V, false, debug);
}

void hakc::HAKCTypeIdentifier::findEscapes() {
    for (auto &F: M.getFunctionList()) {
        debug = (F.getName() == CommonHAKCAnalysis::getHAKCDebugName());
        if (CommonHAKCAnalysis::isOutsideTransferFunc(&F) || F.isIntrinsic()) {
            continue;
        }

        for (auto &use: F.uses()) {
            std::string symbol;

            if (debug) {
                CommonHAKCAnalysis::getWriter() << F.getName() << " User: ";
                use.getUser()->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " operand number " << std::to_string(use.getOperandNo())
                                                << "\n";
            }
            if (auto *gv = dyn_cast<GlobalVariable>(use.getUser())) {
                if (debug) {
                    CommonHAKCAnalysis::getWriter() << "User is a global variable\n";
                }
                Type *gvType = gv->getType()->getPointerElementType();
                auto type = getHAKCType(gvType);
                if (!type) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Unexpected HAKCTypeInfo for global " << gv->getName()
                                                        << " of type ";
                        gvType->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                    }
                    continue;
                }
                symbol = getOperandString(gv, type);
            } else if (auto *call = dyn_cast<CallInst>(use.getUser())) {
                if (debug) {
                    CommonHAKCAnalysis::getWriter() << "User is a CallInst\n";
                }
                for (auto &arg: call->args()) {
                    if (arg.get() == &F) {
                        if (!call->getCalledFunction()) {
                            CommonHAKCAnalysis::getWriter() << F.getName() << " is used in an indirect call in "
                                                            << call->getFunction()->getName() << ": ";
                            call->print(CommonHAKCAnalysis::getWriter());
                            CommonHAKCAnalysis::getWriter() << "\n";
                            addUseInIndirectCall(&F);
                            continue;
                        }
                        symbol = getOperandString(arg, getHAKCType(F.getType()));
                        break;
                    }
                }
                /* We don't care about the actual invocation of a function */
                if (symbol.empty()) {
                    continue;
                }
            } else if (auto *store = dyn_cast<StoreInst>(use.getUser())) {
                if (debug) {
                    CommonHAKCAnalysis::getWriter() << "User is a StoreInst\n";
                }
                symbol = getStoreOperandOrigin(store);

                if (symbol == UNKNOWN_ORIGIN) {
                    if (debug) {
                        auto def = GetDef(store->getPointerOperand());
                        CommonHAKCAnalysis::getWriter() << "Unexpected HAKCTypeInfo for store ";
                        store->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << " with def ";
                        def->getType()->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << " in function\n";
                        store->getFunction()->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                    }
                    addUseInUnknownOriginStore(&F);
                    continue;
                }
            } else if (auto *constant =
                    dyn_cast<ConstantStruct>(use.getUser())) {
                auto type = getHAKCType(constant->getType());
                if (!type) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Unexpected HAKCTypeInfo for constant ";
                        constant->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                        CommonHAKCAnalysis::getWriter() << " with type ";
                        constant->getType()->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                        CommonHAKCAnalysis::getWriter() << "Users of constant:\n";
                        for (auto *cuser: constant->users()) {
                            cuser->print(CommonHAKCAnalysis::getWriter());
                            CommonHAKCAnalysis::getWriter() << "\n";
                        }
                    }
                    continue;
                }
                symbol = getUseOriginString(&use, type);
            } else if (isa<BlockAddress>(use.getUser())) {
                continue;
            } else if (auto *selectInst = dyn_cast<SelectInst>(use.getUser())) {
                if (debug) {
                    CommonHAKCAnalysis::getWriter() << "User is a SelectInst\n";
                }
                for (auto *selectUser: selectInst->users()) {
                    if (auto *selectUserStore = dyn_cast<StoreInst>(selectUser)) {
                        symbol = getStoreOperandOrigin(selectUserStore);
                        break;
                    }
                }
            }

            if (!symbol.empty()) {
                addEscapingSymbol(&F, symbol);
            } else {
                if (debug) {
                    CommonHAKCAnalysis::getWriter() << "Unhandled use for " << F.getName() << ":\n";
                    if (auto *FUser = dyn_cast<Function>(use.getUser())) {
                        CommonHAKCAnalysis::getWriter() << FUser->getName();
                    } else {
                        use->print(CommonHAKCAnalysis::getWriter());
                    }
                    CommonHAKCAnalysis::getWriter() << "\n";
                }
                continue;
            }
        }
    }
}

bool hakc::HAKCTypeIdentifier::diTypeShouldBeAnalyzed(const DIType *diType) {
    return !isAnonStructOrUnion(diType) && !isArrayOfAnonStructsOrUnions(diType) && !isPointerToAnonStructOrUnion
            (diType);
}

bool hakc::HAKCTypeIdentifier::GlobalShouldBeSkipped(GlobalVariable *GV) {
    bool result = (GV->getNumUses() == 0 && !GV->hasExternalLinkage()) ||
                  GV->getName().empty() || GV->hasPrivateLinkage() ||
                  GV->getName().startswith(".") ||
                  /* Skip constant strings */
                  (GV->hasInitializer() && isa<Constant>(GV->getInitializer()) &&
                   GV->getInitializer()->getType()->isArrayTy() &&
                   GV->getInitializer()->getType()->getArrayElementType()->isIntegerTy(8));
    if (!result && !isa<Function>(GV)) {
        if (auto *StructTy = dyn_cast<StructType>(GV->getType()->getPointerElementType())) {
            result = StructTy->isOpaque();
        }
    }
    return result;
}

hakc::HAKCTypeIdentifier::HAKCTypeIdentifier(Module &M, CommonHAKCAnalysis *AnalysisHelper)
        : HAKCDebugInfoProcessor(M), AnalysisHelper(AnalysisHelper), types() {

    for (auto &Global: M.getGlobalList()) {
        debug = (Global.getName() == CommonHAKCAnalysis::getHAKCDebugName());
        if (GlobalShouldBeSkipped(&Global)) {
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Skipping " << Global.getName();
                if (Global.getNumUses() == 0) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << ": Zero uses";
                    }
                }
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            continue;
        }
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Users of ";
            Global.print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << ":\n";
            for (auto *User: Global.users()) {
                CommonHAKCAnalysis::getWriter() << "\t";
                for (auto *link: GetDefChain(User)) {
                    link->print(CommonHAKCAnalysis::getWriter());
                    for (auto *linkuser: link->users()) {
                        CommonHAKCAnalysis::getWriter() << "\n\t\t";
                        linkuser->print(CommonHAKCAnalysis::getWriter());
                    }
                    CommonHAKCAnalysis::getWriter() << "\n";
                }
                CommonHAKCAnalysis::getWriter() << "\n";
            }
        }
        SmallVector<DIGlobalVariableExpression *, 5> DebugInfo;
        Global.getDebugInfo(DebugInfo);
        DIGlobalVariable *diGlobal = nullptr;
        if (!DebugInfo.empty()) {
            diGlobal = DebugInfo[0]->getVariable();
        }
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Found " << std::to_string(DebugInfo.size()) << " DI objects\n";
        }
        std::shared_ptr<HAKCTypeInfo> HakcType = nullptr;
        std::shared_ptr<HAKCGlobalInfo> symbol = nullptr;
        if (!diGlobal || !diTypeShouldBeAnalyzed(diGlobal->getType())) {
            if (debug) {
                CommonHAKCAnalysis::getWriter() << Global.getName() << ": ";
                if (!diGlobal) {
                    CommonHAKCAnalysis::getWriter() << "Could not find debug info.\n";
                    Global.print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << "\n";
                } else {
                    CommonHAKCAnalysis::getWriter() << "Type should not be Analyzed. Skipping...\n";
                }
            }
            if (diGlobal && !diTypeShouldBeAnalyzed(diGlobal->getType())) {
                continue;
            }
            HakcType = addNoDebugType(&Global);
            symbol = std::make_shared<HAKCGlobalInfo>(&Global, *this, "", "", 0);
        } else {
            auto *UnwrappedDIType = unwrapDIType(diGlobal->getType());
            HakcType = addType(UnwrappedDIType, &Global);
            symbol = std::make_shared<HAKCGlobalInfo>(&Global, *this,
                                                      diGlobal->getDirectory(),
                                                      diGlobal->getFilename(),
                                                      diGlobal->getLine());
        }
        if (!HakcType || !symbol) {
            CommonHAKCAnalysis::getWriter() << "Did not create all needed objects\n";
            throw std::exception();
        }

        symbols.insert(symbol);
        for (auto *User: Global.users()) {
            AddUserToType(HakcType, User);
        }
    }

    for (auto &F: M.getFunctionList()) {
        auto *subprogram = F.getSubprogram();
        debug = F.getName() == CommonHAKCAnalysis::getHAKCDebugName();
        if (functionShouldBeSkipped(&F) || !subprogram) {
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Skipping " << F.getName() << "\n";
            }
            continue;
        }
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Analyzing " << F.getName() << "\n";
        }
        if (debug) {
            M.print(CommonHAKCAnalysis::getWriter(), nullptr);
            CommonHAKCAnalysis::getWriter() << "\n";
        }
        for (auto it = inst_begin(F); it != inst_end(F); ++it) {
            Instruction *inst = &*it;
            StructType *StructTy = nullptr;
            if (auto *Store = dyn_cast<StoreInst>(inst)) {
                auto *Def = GetDef(Store->getPointerOperand());
                if (Def->getType()->isPointerTy()) {
                    StructTy = dyn_cast<StructType>(Def->getType()->getPointerElementType());
                }
                if (debug && !StructTy) {
                    CommonHAKCAnalysis::getWriter() << "Def: ";
                    Def->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << "\n";
                }
            } else if (auto *Load = dyn_cast<LoadInst>(inst)) {
                auto *Def = GetDef(Load->getPointerOperand());
                if (Def->getType()->isPointerTy()) {
                    StructTy = dyn_cast<StructType>(Def->getType()->getPointerElementType());
                }
                if (debug && !StructTy) {
                    CommonHAKCAnalysis::getWriter() << "Def: ";
                    Def->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << "\n";
                }
            }
            if (StructTy && !StructTy->isOpaque()) {
                auto *DiType = findDiType(StructTy);
                if (DiType) {
                    auto HakcType = getHAKCType(DiType);
                    if (!HakcType) {
                        HakcType = addType(DiType, StructTy);
                    }
                    HakcType->addUser(&F);
                } else {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Could not find DITYpe for ";
                        StructTy->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << " for Instruction ";
                        inst->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                    }
                }
            } else {
                if (debug && (isa<StoreInst>(inst) || isa<LoadInst>(inst))) {
                    inst->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << " does not involve a StructType\n";
                }
            }
        }
        auto symbol = std::make_shared<HAKCFunctionInfo>(&F, *this);
        symbols.insert(symbol);
    }

    findEscapes();
}

void hakc::HAKCTypeIdentifier::AddUserToType(std::shared_ptr<HAKCTypeInfo> &HakcType, User *User) {
    if (auto *I = dyn_cast<Instruction>(User)) {
        HakcType->addUser(I->getFunction());
    } else if (auto *GV = dyn_cast<GlobalVariable>(User)) {
        HakcType->addUser(GV);
    } else if (auto *O = dyn_cast<Operator>(User)) {
        for (auto *OUser: O->users()) {
            AddUserToType(HakcType, OUser);
        }
    } else if (auto *C = dyn_cast<Constant>(User)) {
        for (auto *CUser: C->users()) {
            AddUserToType(HakcType, CUser);
        }
    } else {
        CommonHAKCAnalysis::getWriter() << "Unhandled User: ";
        User->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\n";
        throw std::exception();
    }
}

void hakc::HAKCTypeIdentifier::addEscapingSymbol(Function *F,
                                                 std::string &escapingSymbol) {
    bool found = false;
    for (auto &f: symbols) {
        if (f->getValue() == F) {
            f->addEscapingSymbol(escapingSymbol);
            found = true;
            break;
        }
    }

    if (!found && debug) {
        CommonHAKCAnalysis::getWriter() << "Could not find function " << F->getName() << "\n";
    }
}

std::shared_ptr<hakc::HAKCTypeInfo>
hakc::HAKCTypeIdentifier::getHAKCType(const DIType *type) {
    type = unwrapDIType(type);
    for (auto p: types) {
        if (p->getDiType() == type) {
            return p;
        }
    }

    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Could not get HAKCTypeInfo for type ";
        printDIType(type, 0);
        CommonHAKCAnalysis::getWriter() << "\n";
    }

    return nullptr;
}

std::string hakc::HAKCTypeIdentifier::GetTransformedPath(std::string Path) {
    if(Path.empty()) {
        return Path;
    }

    auto* SourcePath = std::getenv(hakc::HAKC_SOURCE_PATH.str().c_str());
    if(!SourcePath || std::strlen(SourcePath) == 0) {
        CommonHAKCAnalysis::getWriter() << "Invalid " << hakc::HAKC_SOURCE_PATH << "!\n";
        throw std::exception();
    }

    auto *BuildPath = std::getenv(hakc::HAKC_BUILD_PATH.str().c_str());
    if(!BuildPath || std::strlen(BuildPath) == 0) {
        CommonHAKCAnalysis::getWriter() << "Invalid " << hakc::HAKC_BUILD_PATH << "!\n";
        throw std::exception();
    }

    StringRef PathRef(Path);
    auto Result = Path;
    unsigned length = 0;
    std::string Replacement;
    if(PathRef.startswith(BuildPath)) {
        length = std::strlen(BuildPath);
        Replacement = HAKC_BUILD_PATH_REPLACEMENT.str();
    } else if(PathRef.startswith(SourcePath)) {
        length = std::strlen(SourcePath);
        Replacement = HAKC_SOURCE_PATH_REPLACEMENT.str();
    } else {
        CommonHAKCAnalysis::getWriter() << "Path " << PathRef << " does not start with either "
        << BuildPath << " or " << SourcePath << "!\n";
//        return Path;
                throw std::exception();
    }

    if(!sys::path::is_separator(Path[length])) {
        Replacement += sys::path::get_separator();
    }

    Result.replace(0, length, Replacement);
    return Result;
}

std::string hakc::HAKCTypeIdentifier::GetTransformedPath(SmallVector<char> &Path) {
    std::stringstream sstream;
    for(auto c : Path) {
        sstream << c;
    }

    return GetTransformedPath(sstream.str());
}

void hakc::HAKCTypeIdentifier::outputTypes(raw_fd_ostream &out) {
    std::error_code err;
    SmallVector<char> sourcePath;
    err = sys::fs::real_path(M.getSourceFileName(), sourcePath, true);
    if (err) {
        CommonHAKCAnalysis::getWriter() << "Could not get real path to " << M.getSourceFileName() << "\n";
        throw std::exception();
    }

    out << "---\n";
    out << "CU: ";
    out << GetTransformedPath(sourcePath);
    out << "\n";

    out << "types:\n";
    for (auto &t: types) {
        if (!t->getType()->isStructTy()) {
            continue;
        }
        std::string yml = t->getYaml();
        if (yml.empty()) {
            continue;
        }
        StringRef yaml(yml);
        SmallVector<StringRef> lines;
        yaml.split(lines, "\n");
        for (auto line: lines) {
            out << "  " << line << "\n";
        }
    }

    out << "symbols:\n";
    for (auto &s: symbols) {
        debug = s->getName() == CommonHAKCAnalysis::getHAKCDebugName();
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Outputting YAML for " << s->getValue()->getName() << "\n";
            CommonHAKCAnalysis::getWriter() << "typeString = " << s->getTypeStringRepresentation() << "\n";
        }
        std::string yml = s->getYaml();
        if (yml.empty()) {
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "\tYAML empty for " << s->getValue()->getName() << "\n";
            }
            continue;
        }
        StringRef yaml(yml);
        SmallVector<StringRef> lines;
        yaml.split(lines, "\n");
        for (auto line: lines) {
            out << "  " << line << "\n";
        }
    }
}

std::shared_ptr<hakc::HAKCTypeInfo>
hakc::HAKCTypeIdentifier::addType(const DIType *diType, GlobalObject *GO) {
    std::shared_ptr<hakc::HAKCTypeInfo> result;
    Type *Ty;
    if (auto *F = dyn_cast<Function>(GO)) {
        Ty = F->getFunctionType();
    } else {
        Ty = GO->getType()->getPointerElementType();
    }
    if (auto *StructTy = dyn_cast<StructType>(Ty)) {
        if (StructTy->isOpaque()) {
            GO->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " is an opaque struct\n";
            throw std::exception();
        }
    }
    result = addType(diType, Ty);
    result->addUser(GO);
    return result;
}

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::addType(const DIType *diType, Type *Ty) {
    std::shared_ptr<hakc::HAKCTypeInfo> result;
    if (!diType) {
        CommonHAKCAnalysis::getWriter() << "Null diType!\n";
        throw std::exception();
    }

    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Adding ";
        printDIType(diType, 0);
        CommonHAKCAnalysis::getWriter() << "\n";
    }
    diType = unwrapDIType(diType);

    result = getHAKCType(diType);
    if (result) {
        if (debug) {
            printDIType(diType, 0);
            CommonHAKCAnalysis::getWriter() << " already added\n";
        }
    } else {
        result = std::make_shared<HAKCTypeInfo>(diType, Ty, *this);
        if (auto *StructTy = dyn_cast<StructType>(Ty)) {
            if (StructTy->isOpaque()) {
                CommonHAKCAnalysis::getWriter() << "Tried to add Opaque StructType: ";
                Ty->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
                throw std::exception();
            }
        }
        types.insert(result);
    }
    return result;
}

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::addNoDebugType(GlobalObject *GO) {
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Adding GlobalObject with no debug information: ";
        GO->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\n";
    }
    Type *Ty = GO->getType()->getPointerElementType();
    while (Ty->isArrayTy()) {
        Ty = Ty->getArrayElementType();
    }
    if (auto *StructTy = dyn_cast<StructType>(Ty)) {
        if (StructTy->isOpaque()) {
            GO->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " is an opaque struct\n";
            throw std::exception();
        }
    }
    auto result = std::make_shared<hakc::HAKCTypeInfo>(Ty, *this);
    types.insert(result);
    result->addUser(GO);
    return result;
}

Module &hakc::HAKCTypeIdentifier::GetModule() {
    return M;
}
