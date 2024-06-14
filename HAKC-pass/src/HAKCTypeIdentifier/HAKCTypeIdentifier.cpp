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

#include "llvm/Support/Path.h"
#include "llvm/IR/DerivedTypes.h"

#include <sstream>
#include <algorithm>

static std::string UNKNOWN_ORIGIN = "unknown-origin";

//std::string
//hakc::HAKCTypeIdentifier::getStoreOperandOrigin(StoreInst *storeInst) {
//    std::string result = getOperandOriginString(storeInst->getPointerOperand());
//
//    if (debug) {
//        CommonHAKCAnalysis::getWriter() << "Found origin hash for store ";
//        storeInst->print(CommonHAKCAnalysis::getWriter());
//        CommonHAKCAnalysis::getWriter() << ": " << result << "\n";
//    }
//    return result;
//}

//Value *hakc::HAKCTypeIdentifier::getOperandOrigin(Value *operand) {
//    Value *result = nullptr;
//    std::vector<Value *> defChain =
//            AnalysisHelper->findDefChain(operand, true, debug);
//    for (Value *v: defChain) {
//        if (isa<GetElementPtrInst>(v)) {
//            if (debug) {
//                CommonHAKCAnalysis::getWriter() << "Found GEP for ";
//                operand->print(CommonHAKCAnalysis::getWriter());
//                CommonHAKCAnalysis::getWriter() << ": ";
//                v->print(CommonHAKCAnalysis::getWriter());
//                CommonHAKCAnalysis::getWriter() << "\n";
//            }
//            result = v;
//            break;
//        }
//    }
//
//    if (!result) {
//        result = defChain.front();
//    }
//
//    return result;
//}

//std::set<std::pair<std::string, std::string>>
//hakc::HAKCTypeIdentifier::getOperandStringTokens(
//        Value *operand, std::shared_ptr<HAKCTypeInfo> &hakcType) {
//    std::set<std::pair<std::string, std::string>> result;
//
//    if (hakcType) {
//        result.insert(std::make_pair("type", hakcType->getTypeStringRepresentation().str()));
//        if (auto *gep = dyn_cast<GetElementPtrInst>(operand)) {
//            APInt offset(64, 0, true);
//            bool foundOffset =
//                    gep->accumulateConstantOffset(M.getDataLayout(), offset);
//            if (foundOffset) {
//                result.insert(
//                        std::make_pair("offset", std::to_string(offset.getSExtValue())));
//                /* Ensure that we record all type accesses through indirections */
//                std::vector<Value *> indices;
//                unsigned i = 0;
//                for (auto &index: gep->indices()) {
//                    if (i < gep->getNumIndices() - 1) {
//                        indices.push_back(index.get());
//                        Type *accessedType = GetElementPtrInst::getIndexedType(gep->getSourceElementType(), indices);
//                        auto accessedHakcType = getHAKCType(accessedType);
//                        if (accessedHakcType) {
//                            accessedHakcType->addUser(gep->getFunction());
//                        }
//                    }
//                    i++;
//                }
//            }
//        } else if (auto *argument = dyn_cast<Argument>(operand)) {
//            result.insert(
//                    std::make_pair("argument", std::to_string(argument->getArgNo())));
//        } else if (auto *gv = dyn_cast<GlobalVariable>(operand)) {
//            result.insert(std::make_pair("global-name", gv->getName().str()));
//        } else if (auto *f = dyn_cast<Function>(operand)) {
//            result.insert(std::make_pair("function-name", f->getName().str()));
//        }
//    }
//    return result;
//}

//std::string hakc::HAKCTypeIdentifier::combineTokens(
//        std::set<std::pair<std::string, std::string>> &tokens) {
//    std::string result;
//    if (!tokens.empty()) {
//        result = "{ ";
//        unsigned idx = 0;
//        for (auto &it: tokens) {
//            result += it.first;
//            result += ": ";
//            result += it.second;
//            idx++;
//            if (idx < tokens.size()) {
//                result += ", ";
//            }
//        }
//        result += " }";
//    }
//    return result;
//}

//std::string
//hakc::HAKCTypeIdentifier::getOperandString(Value *operand,
//                                           std::shared_ptr<HAKCTypeInfo> hakcType) {
//    std::string result = UNKNOWN_ORIGIN;
//
//    auto tokens = getOperandStringTokens(operand, hakcType);
//    if (!tokens.empty()) {
//        result = combineTokens(tokens);
//    }
//
//    return result;
//}

//std::string hakc::HAKCTypeIdentifier::getOperandOriginString(Value *operand) {
//    Value *def;
//    std::string result;
//    std::shared_ptr<hakc::HAKCTypeInfo> hakcType = nullptr;
//
//    def = getOperandOrigin(operand);
//    if (auto *gep = dyn_cast<GetElementPtrInst>(def)) {
//        if (debug) {
//            CommonHAKCAnalysis::getWriter() << "Finding type for GEP ";
//            gep->getSourceElementType()->print(CommonHAKCAnalysis::getWriter());
//            CommonHAKCAnalysis::getWriter() << "\n";
//        }
//        hakcType = getHAKCType(gep->getSourceElementType());
//        if (!hakcType) {
//            hakcType = getHAKCType(gep->getSourceElementType()->getPointerTo());
//        }
//        if (!hakcType && gep->getSourceElementType()->isArrayTy()) {
//            hakcType = getHAKCType(gep->getSourceElementType()->getArrayElementType());
//        }
//        if (!hakcType) {
//            auto type = getValueMetadataDIType(operand);
//            if (type) {
//                hakcType = getHAKCType(type);
//            }
//        }
//    } else if (!isa<Operator>(def) /* Yes, subr_kdb.c in FreeBSD uses an inttoptr cast for some code! */ ) {
//        hakcType = getHAKCType(def->getType());
//        if (!hakcType) {
//            auto type = getValueMetadataDIType(operand);
//            if (type) {
//                hakcType = getHAKCType(type);
//            }
//        }
//    }
//
//    result = getOperandString(def, hakcType);
//
//    return result;
//}

//std::string hakc::HAKCTypeIdentifier::getUseOriginString(
//        Use *use, std::shared_ptr<HAKCTypeInfo> hakcType) {
//    std::string result = UNKNOWN_ORIGIN;
//    if (debug) {
//        CommonHAKCAnalysis::getWriter() << "Getting use origin of ";
//        if (auto *F = dyn_cast<Function>(use->get())) {
//            CommonHAKCAnalysis::getWriter() << F->getName();
//        } else {
//            use->get()->print(CommonHAKCAnalysis::getWriter());
//        }
//        CommonHAKCAnalysis::getWriter() << "\n";
//        CommonHAKCAnalysis::getWriter() << "hakcType: " << hakcType->get << "\n";
//        hakcType->getType()->print(CommonHAKCAnalysis::getWriter());
//        CommonHAKCAnalysis::getWriter() << "\n";
//    }
//    auto tokens = getOperandStringTokens(use->getUser(), hakcType);
//    if (!tokens.empty()) {
//        if (auto *constStruct =
//                dyn_cast<ConstantStruct>(use->getUser())) {
//            const StructLayout *layout = getStructLayout(constStruct->getType());
//            tokens.insert(std::make_pair(
//                    "offset",
//                    std::to_string(layout->getElementOffset(use->getOperandNo()))));
//        } else if (isa<CallInst>(use->getUser())) {
//            tokens.insert(
//                    std::make_pair("argument", std::to_string(use->getOperandNo())));
//        }
//
//        result = combineTokens(tokens);
//    }
//
//    return result;
//}

//std::string hakc::HAKCTypeIdentifier::getCallOperandOrigin(CallInst *call) {
//    std::string result = getOperandOriginString(call->getCalledOperand());
//
//    if (debug) {
//        CommonHAKCAnalysis::getWriter() << "Found origin hash for call ";
//        call->print(CommonHAKCAnalysis::getWriter());
//        CommonHAKCAnalysis::getWriter() << ": " << result << "\n";
//    }
//
//    return result;
//}

//std::shared_ptr<hakc::HAKCTypeInfo>
//hakc::HAKCTypeIdentifier::getHAKCType(Type *type) {
//    for (auto t: types) {
//        if (t->getType() == type) {
//            return t;
//        }
//    }
//
//    return nullptr;
//}

//void hakc::HAKCTypeIdentifier::addUseInUnknownOriginStore(Function *F) {
//    bool found = false;
//    for (auto &f: symbols) {
//        if (f->getValue() == F) {
//            f->setUsedInUnknownOriginStore();
//            found = true;
//            break;
//        }
//    }
//
//    if (!found && debug) {
//        CommonHAKCAnalysis::getWriter() << "Could not find function " << F->getName() << "\n";
//    }
//}

//void hakc::HAKCTypeIdentifier::addUseInIndirectCall(Function *F) {
//    bool found = false;
//    for (auto &f: symbols) {
//        if (f->getValue() == F) {
//            f->setUsedInIndirectCalls();
//            found = true;
//            break;
//        }
//    }
//
//    if (!found && debug) {
//        CommonHAKCAnalysis::getWriter() << "Could not find function " << F->getName() << "\n";
//    }
//}
//
//Value *hakc::HAKCTypeIdentifier::GetDef(Value *V) {
//    return AnalysisHelper->getDef(V, false, debug);
//}
//
//std::vector<Value *> hakc::HAKCTypeIdentifier::GetDefChain(Value *V) {
//    return AnalysisHelper->findDefChain(V, false, debug);
//}

//void hakc::HAKCTypeIdentifier::findEscapes() {
//    for (auto &F: M.getFunctionList()) {
//        debug = (F.getName() == CommonHAKCAnalysis::getHAKCDebugName());
//        if (CommonHAKCAnalysis::isOutsideTransferFunc(&F) || F.isIntrinsic()) {
//            continue;
//        }
//
//        for (auto &use: F.uses()) {
//            std::string symbol;
//
//            if (debug) {
//                CommonHAKCAnalysis::getWriter() << F.getName() << " User: ";
//                use.getUser()->print(CommonHAKCAnalysis::getWriter());
//                CommonHAKCAnalysis::getWriter() << " operand number " << std::to_string(use.getOperandNo())
//                                                << "\n";
//            }
//            if (auto *gv = dyn_cast<GlobalVariable>(use.getUser())) {
//                if (debug) {
//                    CommonHAKCAnalysis::getWriter() << "User is a global variable\n";
//                }
//                Type *gvType = gv->getType()->getPointerElementType();
//                auto type = getHAKCType(gvType);
//                if (!type) {
//                    if (debug) {
//                        CommonHAKCAnalysis::getWriter() << "Unexpected HAKCTypeInfo for global " << gv->getName()
//                                                        << " of type ";
//                        gvType->print(CommonHAKCAnalysis::getWriter());
//                        CommonHAKCAnalysis::getWriter() << "\n";
//                    }
//                    continue;
//                }
//                symbol = getOperandString(gv, type);
//            } else if (auto *call = dyn_cast<CallInst>(use.getUser())) {
//                if (debug) {
//                    CommonHAKCAnalysis::getWriter() << "User is a CallInst\n";
//                }
//                for (auto &arg: call->args()) {
//                    if (arg.get() == &F) {
//                        if (!call->getCalledFunction()) {
//                            CommonHAKCAnalysis::getWriter() << F.getName() << " is used in an indirect call in "
//                                                            << call->getFunction()->getName() << ": ";
//                            call->print(CommonHAKCAnalysis::getWriter());
//                            CommonHAKCAnalysis::getWriter() << "\n";
//                            addUseInIndirectCall(&F);
//                            continue;
//                        }
//                        symbol = getOperandString(arg, getHAKCType(F.getType()));
//                        break;
//                    }
//                }
//                /* We don't care about the actual invocation of a function */
//                if (symbol.empty()) {
//                    continue;
//                }
//            } else if (auto *store = dyn_cast<StoreInst>(use.getUser())) {
//                if (debug) {
//                    CommonHAKCAnalysis::getWriter() << "User is a StoreInst\n";
//                }
//                symbol = getStoreOperandOrigin(store);
//
//                if (symbol == UNKNOWN_ORIGIN) {
//                    if (debug) {
//                        auto def = GetDef(store->getPointerOperand());
//                        CommonHAKCAnalysis::getWriter() << "Unexpected HAKCTypeInfo for store ";
//                        store->print(CommonHAKCAnalysis::getWriter());
//                        CommonHAKCAnalysis::getWriter() << " with def ";
//                        def->getType()->print(CommonHAKCAnalysis::getWriter());
//                        CommonHAKCAnalysis::getWriter() << " in function\n";
//                        store->getFunction()->print(CommonHAKCAnalysis::getWriter());
//                        CommonHAKCAnalysis::getWriter() << "\n";
//                    }
//                    addUseInUnknownOriginStore(&F);
//                    continue;
//                }
//            } else if (auto *constant =
//                    dyn_cast<ConstantStruct>(use.getUser())) {
//                auto type = getHAKCType(constant->getType());
//                if (!type) {
//                    if (debug) {
//                        CommonHAKCAnalysis::getWriter() << "Unexpected HAKCTypeInfo for constant ";
//                        constant->print(CommonHAKCAnalysis::getWriter());
//                        CommonHAKCAnalysis::getWriter() << "\n";
//                        CommonHAKCAnalysis::getWriter() << " with type ";
//                        constant->getType()->print(CommonHAKCAnalysis::getWriter());
//                        CommonHAKCAnalysis::getWriter() << "\n";
//                        CommonHAKCAnalysis::getWriter() << "Users of constant:\n";
//                        for (auto *cuser: constant->users()) {
//                            cuser->print(CommonHAKCAnalysis::getWriter());
//                            CommonHAKCAnalysis::getWriter() << "\n";
//                        }
//                    }
//                    continue;
//                }
//                symbol = getUseOriginString(&use, type);
//            } else if (isa<BlockAddress>(use.getUser())) {
//                continue;
//            } else if (auto *selectInst = dyn_cast<SelectInst>(use.getUser())) {
//                if (debug) {
//                    CommonHAKCAnalysis::getWriter() << "User is a SelectInst\n";
//                }
//                for (auto *selectUser: selectInst->users()) {
//                    if (auto *selectUserStore = dyn_cast<StoreInst>(selectUser)) {
//                        symbol = getStoreOperandOrigin(selectUserStore);
//                        break;
//                    }
//                }
//            }
//
//            if (!symbol.empty()) {
//                addEscapingSymbol(&F, symbol);
//            } else {
//                if (debug) {
//                    CommonHAKCAnalysis::getWriter() << "Unhandled use for " << F.getName() << ":\n";
//                    if (auto *FUser = dyn_cast<Function>(use.getUser())) {
//                        CommonHAKCAnalysis::getWriter() << FUser->getName();
//                    } else {
//                        use->print(CommonHAKCAnalysis::getWriter());
//                    }
//                    CommonHAKCAnalysis::getWriter() << "\n";
//                }
//                continue;
//            }
//        }
//    }
//}
//
bool hakc::HAKCTypeIdentifier::DiTypeShouldBeAnalyzed(const DIType *diType) {
    return !isAnonStructOrUnion(diType) &&
           !isArrayOfAnonStructsOrUnions(diType) &&
           !isPointerToAnonStructOrUnion(diType);
}
//
//bool hakc::HAKCTypeIdentifier::GlobalShouldBeSkipped(GlobalVariable *GV) {
//    bool result = (GV->getNumUses() == 0 && !GV->hasExternalLinkage()) ||
//                  GV->getName().empty() || GV->hasPrivateLinkage() ||
//                  GV->getName().startswith(".") ||
//                  /* Skip constant strings */
//                  (GV->hasInitializer() &&
//                   isa<Constant>(GV->getInitializer()) &&
//                   CommonHAKCAnalysis::IsStringType(GV->getInitializer()->getType()));
//    if (!result && !isa<Function>(GV)) {
//        if (auto *StructTy = dyn_cast<StructType>(GV->getType()->getPointerElementType())) {
//            result = StructTy->isOpaque();
//        }
//    }
//    return result;
//}

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::FindType(const DIType *type) {
    if (!type) {
        CommonHAKCAnalysis::getWriter() << "Trying to find null type\n";
        throw std::exception();
    }
    auto it = types.find(type);
    if (it == types.end()) {
        return nullptr;
    } else {
        return it->second;
    }
}

std::string hakc::HAKCTypeIdentifier::ConstructStructName(StructType *StructTy) {
    std::string StructName;
    if (StructTy->hasName()) {
        llvm::raw_string_ostream sstream(StructName);
        sstream << StructTy->getName();

        StructName.erase(std::remove(StructName.begin(), StructName.end(), '%'), StructName.end());
        for (unsigned i = 0; i < StructName.size(); i++) {
            if (StructName[i] == '.') {
                StructName[i] = ' ';
            }
        }
    }
    return StructName;
}

//std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::FindType(Type *Ty) {
//    for (auto &it: globals) {
//        if (it.second->GetGlobalVariable()->getType() == Ty) {
//            return it.second->GetType();
//        }
//    }
//    for (auto &it: functions) {
//        if (it.second->GetFunction()->getType() == Ty) {
//            return it.second->GetType();
//        }
//    }
//    for(auto &it : NoDebugTypes) {
//        if(it.first == Ty) {
//            return it.second;
//        }
//    }
//    if (auto *StructTy = dyn_cast<StructType>(Ty)) {
//        if (StructTy->hasName()) {
//            std::string StructName = ConstructStructName(StructTy);
//
//            if(debug) {
//                CommonHAKCAnalysis::getWriter() << "Searching for type with name " << StructName << "\n";
//            }
//
//            for (auto &it: types) {
//                if(it.second->GetName() == StructName) {
//                    return it.second;
//                }
//            }
//        }
//    }
//
//
//    return nullptr;
//}

void hakc::HAKCTypeIdentifier::AddTypeMapping(const DIType *type, const std::shared_ptr<HAKCTypeInfo> &HAKCType) {
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Adding mapping " << *type << " -> " << *HAKCType << "\n";
    }
    std::set<unsigned> TagsToSize = {
            dwarf::DW_TAG_structure_type,
            dwarf::DW_TAG_union_type,
    };
    if (isa<DIBasicType>(type) || TagsToSize.find(type->getTag()) != TagsToSize.end()) {
        HAKCType->SetSizeInBits(type->getSizeInBits());
    }
    HAKCType->SetDbgType(type);
    types[type] = HAKCType;
}

unsigned hakc::HAKCTypeIdentifier::GetAnonymousID(const DIType *type) {
    unsigned ID;
    if (AnonymousNumberMapping.find(type) == AnonymousNumberMapping.end()) {
        AnonymousNumberMapping[type] = CurrentAnonID;
        ID = CurrentAnonID;
        CurrentAnonID++;
    } else {
        ID = AnonymousNumberMapping[type];
    }

    return ID;
}

std::string hakc::HAKCTypeIdentifier::GetTypeName(const DIType *type) {
    std::string Name;
    llvm::raw_string_ostream out(Name);

    if (auto *SubroutineTy = dyn_cast<DISubroutineType>(type)) {
        for (unsigned i = 0; i < SubroutineTy->getTypeArray()->getNumOperands(); i++) {
            auto *CurrTy = SubroutineTy->getTypeArray()[i];
            if (!CurrTy) {
                if (i == 0) {
                    out << "void";
                } else if (i == SubroutineTy->getTypeArray()->getNumOperands() - 1) {
                    out << "...";
                } else {
                    CommonHAKCAnalysis::getWriter() << "Null operand at " << i << " for " << *type << "\n";
                    CommonHAKCAnalysis::getWriter() << "Current type: " << Name << "\n";
                    throw std::exception();
                }
            } else {
                out << GetTypeName(CurrTy);
            }

            if (i == 0) {
                out << " (";
            } else if (i < SubroutineTy->getTypeArray()->getNumOperands() - 1) {
                out << ", ";
            }
        }
        out << ")";
    } else if (auto *DerivedTy = dyn_cast<DIDerivedType>(type)) {
        if (DerivedTy->getTag() == dwarf::DW_TAG_pointer_type) {
            if (!DerivedTy->getBaseType()) {
                out << "void";
            } else {
                out << GetTypeName(DerivedTy->getBaseType());
            }
            out << "*";
        } else if (DerivedTy->getTag() == dwarf::DW_TAG_typedef) {
            out << DerivedTy->getName();
        } else if (DerivedTy->getTag() == dwarf::DW_TAG_volatile_type) {
            out << "volatile " << GetTypeName(DerivedTy->getBaseType());
        } else if (DerivedTy->getTag() == dwarf::DW_TAG_const_type) {
            out << "const ";
            if (!DerivedTy->getBaseType()) {
                out << "void";
            } else {
                out << GetTypeName(DerivedTy->getBaseType());
            }
        } else {
            CommonHAKCAnalysis::getWriter() << "Unhandled DIDerivedType tag\n";
            printDIType(DerivedTy, 0);
            CommonHAKCAnalysis::getWriter() << "\n";
            throw std::exception();
        }
    } else if (auto *CompositeTy = dyn_cast<DICompositeType>(type)) {
        if (CompositeTy->getTag() == dwarf::DW_TAG_array_type) {
            out << GetTypeName(CompositeTy->getBaseType()) << "[]";
        } else if (CompositeTy->getTag() == dwarf::DW_TAG_structure_type) {
            out << "struct ";
            if (CompositeTy->getName().empty()) {
                out << "anon." << GetAnonymousID(CompositeTy);
            } else {
                out << CompositeTy->getName();
            }
        } else if (CompositeTy->getTag() == dwarf::DW_TAG_union_type) {
            out << "union ";
            if (CompositeTy->getName().empty()) {
                out << "anon." << GetAnonymousID(CompositeTy);
            } else {
                out << CompositeTy->getName();
            }
        } else if (CompositeTy->getTag() == dwarf::DW_TAG_enumeration_type) {
            out << "enum " << CompositeTy->getName();
        } else {
            CommonHAKCAnalysis::getWriter() << "Unhandled DICompositeType tag\n";
            printDIType(CompositeTy, 0);
            CommonHAKCAnalysis::getWriter() << "\n";
            throw std::exception();
        }
    } else if (auto *BaseTy = dyn_cast<DIBasicType>(type)) {
        out << BaseTy->getName();
    } else {
        CommonHAKCAnalysis::getWriter() << "Unhandled DIType\n";
        printDIType(type, 0);
        CommonHAKCAnalysis::getWriter() << "\n";
        throw std::exception();
    }

    return Name;
}

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::HandleType(const DIType *type) {
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Analyzing DIType " << *type << "\n";
    }
    auto TypeP = FindType(type);
    if (TypeP) {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Already created " << *type << "\n";
        }
        return TypeP;
    }
    if (isa<DICompositeType>(type) || isa<DISubroutineType>(type) || isa<DIBasicType>(type)) {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Creating HAKCTypeInfo for\n";
            printDIType(type, 0);
            CommonHAKCAnalysis::getWriter() << "\n";
        }
        auto TypeName = GetTypeName(type);
        TypeP = std::make_shared<HAKCTypeInfo>(TypeName, debug);
        AddTypeMapping(type, TypeP);
    } else if (auto *DerivedTy = dyn_cast<DIDerivedType>(type)) {
        std::set<unsigned> TagsToConsider = {
                dwarf::DW_TAG_pointer_type,
                dwarf::DW_TAG_array_type,
                dwarf::DW_TAG_const_type,
                dwarf::DW_TAG_typedef,
        };
        if (TagsToConsider.find(DerivedTy->getTag()) != TagsToConsider.end()) {
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Creating HAKCTypeInfo for\n";
                printDIType(type, 0);
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            auto TypeName = GetTypeName(type);
            TypeP = std::make_shared<HAKCTypeInfo>(TypeName, debug);
            AddTypeMapping(type, TypeP);
        }
    } else {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Not handling DITYpe ";
            printDIType(type, 0);
            CommonHAKCAnalysis::getWriter() << "\n";
        }
    }
    return TypeP;
}

GlobalVariable *hakc::HAKCTypeIdentifier::FindGlobal(const DIGlobalVariable *DIGV) {
    auto *Scope = DIGV->getScope();
    std::string Name;
    llvm::raw_string_ostream sstream(Name);
    if (auto *SubProg = dyn_cast<DISubprogram>(Scope)) {
        sstream << SubProg->getName() << ".";
    }
    sstream << DIGV->getName();

    return M.getGlobalVariable(Name, true);
}

//void hakc::HAKCTypeIdentifier::HandleGlobalsWithNoDebugInfo() {
//    for (auto &GV: M.globals()) {
//        if (CommonHAKCAnalysis::IsStringType(GV.getValueType())) {
//            if (debug) {
//                CommonHAKCAnalysis::getWriter() << "Global " << GV << " is a constant string\n";
//            }
//            continue;
//        }
//        if (GV.getNumUses() == 0) {
//            if (debug) {
//                CommonHAKCAnalysis::getWriter() << "Global " << GV.getName() << " has 0 uses\n";
//            }
//            continue;
//        }
//        auto Symbol = FindSymbol(&GV);
//        if (!Symbol) {
//            if (debug) {
//                CommonHAKCAnalysis::getWriter() << "Global " << GV << " has no debug info\n";
//            }
//            auto GVTy = FindType(GV.getValueType());
//            if (!GVTy) {
//                if(auto *StructTy = dyn_cast<StructType>(GV.getValueType())) {
//                    if(StructTy->hasName()) {
//                        auto StructName = ConstructStructName(StructTy);
//                        GVTy = std::make_shared<HAKCTypeInfo>(StructName, debug);
//
//                    }
//                }
//                if(!GVTy) {
//                    M.print(CommonHAKCAnalysis::getWriter(), nullptr);
//                    CommonHAKCAnalysis::getWriter() << "\nCould not find HAKCTYpe for Global " << GV << " with ValueType "
//                                                    << *GV.getValueType() << "\n";
//                    throw std::exception();
//                }
//            }
//            auto GlobalSymbol = std::make_shared<HAKCGlobalInfo>(GV.getName(), debug);
//            GlobalSymbol->SetType(GVTy);
//            GlobalSymbol->SetGlobalVariable(&GV);
//            AddNoDebugGlobalMapping(GlobalSymbol->GetGlobalVariable(), GlobalSymbol);
//        }
//    }
//}

//void hakc::HAKCTypeIdentifier::AddNoDebugGlobalMapping(GlobalVariable *GV,
//                                                       const std::shared_ptr<HAKCGlobalInfo> &HAKCSymbol) {
//    if (debug) {
//        CommonHAKCAnalysis::getWriter() << "Adding mapping " << *GV << " -> " << *HAKCSymbol << "\n";
//    }
//    NoDebugGlobals[GV] = HAKCSymbol;
//}

std::shared_ptr<hakc::HAKCGlobalInfo> hakc::HAKCTypeIdentifier::HandleGlobal(const DIGlobalVariable *DIGV) {
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Analyzing Global " << *DIGV << "\n";
    }

    auto *GV = FindGlobal(DIGV);
    if (!GV) {
        M.print(CommonHAKCAnalysis::getWriter(), nullptr);
        CommonHAKCAnalysis::getWriter() << "\nCould not find Global " << DIGV->getName() << "\n";
        throw std::exception();
    }
    if (GV->getNumUses() == 0) {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Global " << DIGV->getName() << " has 0 uses\n";
        }
        return nullptr;
    }

    auto DIGVTy = FindType(DIGV->getType());
    if (!DIGVTy) {
        M.print(CommonHAKCAnalysis::getWriter(), nullptr);
        CommonHAKCAnalysis::getWriter() << "Could not find HAKCType of " << *GV << " with DIType " << *DIGV->getType()
                                        << "\n";
        throw std::exception();
    }

    auto GVP = std::make_shared<HAKCGlobalInfo>(DIGV->getName(), debug);
    GVP->SetType(DIGVTy);
    GVP->SetGlobalVariable(GV);
    AddGlobalMapping(DIGV, GVP);

    return GVP;
}

void hakc::HAKCTypeIdentifier::AddGlobalMapping(const DIGlobalVariable *DIGV,
                                                const std::shared_ptr<HAKCGlobalInfo> &HAKCSymbol) {
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Adding mapping " << *DIGV << " -> " << *HAKCSymbol << "\n";
    }
    globals[DIGV] = HAKCSymbol;
    AddLLVMTypeMapping(HAKCSymbol->GetType(), HAKCSymbol->GetGlobalVariable()->getValueType());
}

std::shared_ptr<hakc::HAKCFunctionInfo> hakc::HAKCTypeIdentifier::HandleFunction(const DISubprogram *SubProg) {
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Handling DISubprogram " << *SubProg << "\n";
    }

    auto *F = M.getFunction(SubProg->getName());
    if (!F) {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "\nCould not find Function " << SubProg->getName() << "\n";
        }
        return nullptr;
    }
    if (F->getNumUses() == 0) {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Global " << SubProg->getName() << " has 0 uses\n";
        }
        return nullptr;
    }
    auto DIGVTy = FindType(SubProg->getType());
    if (!DIGVTy) {
        M.print(CommonHAKCAnalysis::getWriter(), nullptr);
        CommonHAKCAnalysis::getWriter() << "Could not find HAKCType of " << F->getName() << " with DIType "
                                        << *SubProg->getType() << "\n";
        throw std::exception();
    }

    auto FP = std::make_shared<HAKCFunctionInfo>(SubProg->getName(), debug);
    FP->SetType(DIGVTy);
    FP->SetFunction(F);
    AddFunctionMapping(SubProg, FP);

    return FP;
}

void hakc::HAKCTypeIdentifier::AddFunctionMapping(const DISubprogram *SubProg,
                                                  const std::shared_ptr<HAKCFunctionInfo> &HAKCFunction) {
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Adding mapping " << *SubProg << " -> " << *HAKCFunction << "\n";
    }
    functions[SubProg] = HAKCFunction;
    AddLLVMTypeMapping(HAKCFunction->GetType(), HAKCFunction->GetFunction()->getFunctionType());
}

void hakc::HAKCTypeIdentifier::FindAllGlobalsUsed(Value *V, std::set<GlobalObject *> &GlobalSet) {
    if (auto *ConstStruct = dyn_cast<ConstantStruct>(V)) {
        for (auto &Member: ConstStruct->operands()) {
            auto *MemberDef = AnalysisHelper->getDef(Member.get(), false, debug);
            if (auto *GlobalObj = dyn_cast<GlobalObject>(MemberDef)) {
                GlobalSet.insert(GlobalObj);
            } else {
                FindAllGlobalsUsed(MemberDef, GlobalSet);
            }
        }
    } else if (auto *GlobalObj = dyn_cast<GlobalObject>(V)) {
        GlobalSet.insert(GlobalObj);
    } else if (auto *ConstArray = dyn_cast<ConstantArray>(V)) {
        for (auto &Member: ConstArray->operands()) {
            auto MemberDef = AnalysisHelper->getDef(Member.get(), false, debug);
            if (auto *GlobalMember = dyn_cast<GlobalObject>(MemberDef)) {
                GlobalSet.insert(GlobalMember);
            } else {
                FindAllGlobalsUsed(MemberDef, GlobalSet);
            }
        }
    } else if (auto *I = dyn_cast<Instruction>(V)) {
        for (auto &Op: I->operands()) {
            auto MemberDef = AnalysisHelper->getDef(Op.get(), false, debug);
            if (auto *GlobalMember = dyn_cast<GlobalObject>(MemberDef)) {
                GlobalSet.insert(GlobalMember);
            } else {
                FindAllGlobalsUsed(MemberDef, GlobalSet);
            }
        }
    }
}

void hakc::HAKCTypeIdentifier::FindUsesInGlobals() {
    for (auto &it: globals) {
        auto *GV = it.second->GetGlobalVariable();

        if (GV->hasInitializer()) {
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Searching for globals in " << *GV << "\n";
            }
            std::set<GlobalObject *> GlobalsUsed;
            FindAllGlobalsUsed(GV->getInitializer(), GlobalsUsed);
            for (auto *UsedGlobal: GlobalsUsed) {
                auto Symbol = FindSymbol(UsedGlobal);
                if (!Symbol) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "\nGlobal " << UsedGlobal->getName() << " is used in " << *GV
                                                        << " but the Symbol could not be found\n";
                    }
                    continue;
                }
                if (debug) {
                    CommonHAKCAnalysis::getWriter() << "Found Symbol " << Symbol->GetName() << "\n";
                }
                it.second->AddSymbolUse(Symbol);
            }
        }
    }
}

void hakc::HAKCTypeIdentifier::FindUsesInFunctions() {
    for (auto &it: functions) {
        auto *F = it.second->GetFunction();
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Searching for globals in Function " << F->getName() << "\n";
        }

        std::set<GlobalObject *> GlobalsUsed;
        for (auto InstIt = inst_begin(F); InstIt != inst_end(F); ++InstIt) {
            auto *I = &(*InstIt);
            FindAllGlobalsUsed(I, GlobalsUsed);
        }
        for (auto *UsedGlobal: GlobalsUsed) {
            auto Symbol = FindSymbol(UsedGlobal);
            if (!Symbol) {
                if (debug) {
                    CommonHAKCAnalysis::getWriter() << "\nGlobal " << UsedGlobal->getName() << " is used in function "
                                                    << F->getName() << " but the Symbol could not be found\n";
                }
                continue;
            }
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Found Symbol " << Symbol->GetName() << "\n";
            }
            it.second->AddSymbolUse(Symbol);
        }
    }
}

std::shared_ptr<hakc::HAKCSymbolInfo> hakc::HAKCTypeIdentifier::FindSymbol(Value *V) {
    for (auto &it: globals) {
        if (it.second->GetGlobalVariable() == V) {
            return it.second;
        }
    }
    for (auto &it: functions) {
        if (it.second->GetFunction() == V) {
            return it.second;
        }
    }
    return nullptr;
}

void hakc::HAKCTypeIdentifier::FinalizeTypes() {
    std::set<unsigned> StructTypeTags = {
            dwarf::DW_TAG_structure_type,
            dwarf::DW_TAG_union_type,
    };

    std::set<unsigned> TagsToCopy = {
            dwarf::DW_TAG_typedef,
            dwarf::DW_TAG_const_type,
    };

    bool LLVMTypeUpdated = true;
    while (LLVMTypeUpdated) {
        std::map<std::shared_ptr<HAKCTypeInfo>, std::set<Type *>> NewAdditions;
        for (auto &it: LLVMTypeMapping) {
            auto HAKCType = it.first;
            auto TypeMapping = it.second;
            if (TagsToCopy.find(HAKCType->GetDbgType()->getTag()) != TagsToCopy.end()) {
                if (auto *DIDerivedTy = dyn_cast<DIDerivedType>(HAKCType->GetDbgType())) {
                    if (DIDerivedTy->getBaseType()) {
                        auto DerivedHAKCType = FindType(DIDerivedTy->getBaseType());
                        if (DerivedHAKCType) {
                            if (LLVMTypeMapping.find(DerivedHAKCType) != LLVMTypeMapping.end()) {
                                auto ExistingMapping = LLVMTypeMapping[DerivedHAKCType];
                                if (ExistingMapping.size() != TypeMapping.size()) {
                                    if (debug) {
                                        CommonHAKCAnalysis::getWriter() << "Missing Type from " << HAKCType->GetName()
                                                                        << " to " << DerivedHAKCType->GetName() << "\n";
                                    }
                                    NewAdditions[DerivedHAKCType].insert(TypeMapping.begin(), TypeMapping.end());
                                    NewAdditions[HAKCType].insert(ExistingMapping.begin(), ExistingMapping.end());
                                }
                            }
                        }
                    }
                }
            }
        }
        LLVMTypeUpdated = !NewAdditions.empty();
        for (auto &it: NewAdditions) {
            for (auto *Ty: it.second) {
                AddLLVMTypeMapping(it.first, Ty);
            }
        }
    }

    for (auto &it: types) {
        auto *BaseTy = it.first;
        if (StructTypeTags.find(BaseTy->getTag()) != StructTypeTags.end()) {
            auto *StructTy = dyn_cast<DICompositeType>(BaseTy);
            for (auto *E: StructTy->getElements()) {
                auto *Member = dyn_cast<DIDerivedType>(E);
                auto HAKCType = FindType(Member->getBaseType());
                if (HAKCType) {
                    it.second->AddMember(HAKCType, Member->getOffsetInBits());
                }
            }
        }
    }
}

void hakc::HAKCTypeIdentifier::FindTypesInFunctions() {
    for (auto &it: functions) {
        auto *F = it.second->GetFunction();
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Finding types in Function " << F->getName() << "\n";
        }
        for (auto InstIt = inst_begin(F); InstIt != inst_end(F); ++InstIt) {
            auto *I = &(*InstIt);
            if (auto *DbgIntrinsic = dyn_cast<DbgVariableIntrinsic>(I)) {
                auto *V = DbgIntrinsic->getVariableLocation();
                auto *DebugV = DbgIntrinsic->getVariable();
                if (debug) {
                    CommonHAKCAnalysis::getWriter() << "Found " << *V << " to " << *DebugV
                                                    << " mapping from Instruction " << *I << "\n";
                }
                auto HAKCType = FindType(DebugV->getType());
                if (!HAKCType) {
                    CommonHAKCAnalysis::getWriter() << "Could not find HAKCType for DIType " << *DebugV->getType()
                                                    << "\n";
                    throw std::exception();
                }
                auto *LLVMTy = V->getType();
                if (auto *Alloca = dyn_cast<AllocaInst>(V)) {
                    LLVMTy = Alloca->getAllocatedType();
                } else if (auto *CallI = dyn_cast<CallInst>(V)) {
                    if (CallI->isInlineAsm()) {
                        /* Inline assembly causes too much type confusion, so skip these mappings */
                        if (debug) {
                            CommonHAKCAnalysis::getWriter() << "Skipping inline assembly\n";
                        }
                        continue;
                    }
                }
                AddLLVMTypeMapping(HAKCType, LLVMTy);
            }
        }
    }
}

bool hakc::HAKCTypeIdentifier::LLVMTypeMappingSanityCheck(const DIType *type, Type *Ty) {
    if (!type) {
        return false;
    }

    if (auto *BasicTy = dyn_cast<DIBasicType>(type)) {
        return BasicTy->getSizeInBits() == Ty->getScalarSizeInBits();
    } else if (auto *DerivedTy = dyn_cast<DIDerivedType>(type)) {
        std::set<unsigned> BaseTypeCheckTags = {
                dwarf::DW_TAG_const_type,
                dwarf::DW_TAG_typedef,
                dwarf::DW_TAG_enumeration_type,
        };
        if (BaseTypeCheckTags.find(type->getTag()) != BaseTypeCheckTags.end()) {
            if (!DerivedTy->getBaseType()) {
                return type->getTag() == dwarf::DW_TAG_const_type && Ty->isPointerTy() &&
                       Ty->getPointerElementType()->isIntegerTy(8);
            }
            return LLVMTypeMappingSanityCheck(DerivedTy->getBaseType(), Ty);
        } else if (type->getTag() == dwarf::DW_TAG_pointer_type) {
            if (!Ty->isPointerTy()) {
                return false;
            }
            if (DerivedTy->getBaseType()) {
                return LLVMTypeMappingSanityCheck(DerivedTy->getBaseType(), Ty->getPointerElementType());
            } else {
                return Ty->getPointerElementType()->isIntegerTy(8);
            }
        } else if (type->getTag() == dwarf::DW_TAG_array_type) {
            if (!Ty->isArrayTy()) {
                return false;
            }
            return LLVMTypeMappingSanityCheck(DerivedTy->getBaseType(), Ty->getArrayElementType());
        }
    } else if (auto *SubProgTy = dyn_cast<DISubroutineType>(type)) {
        return Ty->isFunctionTy();
    } else if (auto *CompositeTy = dyn_cast<DICompositeType>(type)) {
        if (type->getTag() == dwarf::DW_TAG_structure_type || type->getTag() == dwarf::DW_TAG_union_type) {
            if (!Ty->isStructTy()) {
                return false;
            }
            auto *StructTy = dyn_cast<StructType>(Ty);
            if (!type->getName().empty()) {
                auto StructName = ConstructStructName(StructTy);
                return type->getName() != StructName;
            }
        }
    }

    return true;
}

void hakc::HAKCTypeIdentifier::AddLLVMTypeMapping(const std::shared_ptr<HAKCTypeInfo> &HAKCType, Type *Ty) {
    if (!LLVMTypeMappingSanityCheck(HAKCType->GetDbgType(), Ty)) {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Mapping \"" << HAKCType->GetName() << "\" -> " << *Ty
                                            << " did not pass sanity check\n";
        }
        return;
    }

    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Adding LLVM Type Mapping \"" << HAKCType->GetName() << "\" -> " << *Ty
                                        << "\n";
    }

    auto it = LLVMTypeMapping.find(HAKCType);
    if (it == LLVMTypeMapping.end()) {
        LLVMTypeMapping[HAKCType].insert(Ty);
    } else {
        auto *ExistingTy = *it->second.begin();
        if (ExistingTy->getTypeID() != Ty->getTypeID()) {
            CommonHAKCAnalysis::getWriter() << "Trying to change LLVM Type Mapping from " << *ExistingTy << " to "
                                            << *Ty << "\n";
            throw std::exception();
        }
        LLVMTypeMapping[HAKCType].insert(Ty);
    }
}

hakc::HAKCTypeIdentifier::HAKCTypeIdentifier(Module &M, CommonHAKCAnalysis *AnalysisHelper)
        : HAKCDebugInfoProcessor(M), AnalysisHelper(AnalysisHelper), types(), globals(), functions(),
          LLVMTypeMapping(), CurrentAnonID(0) {
    debug = true;

    if (debug) {
        M.print(CommonHAKCAnalysis::getWriter(), nullptr);
        CommonHAKCAnalysis::getWriter() << "\n";
    }
    for (auto *DITy: infoFinder.types()) {
        auto TypeP = HandleType(DITy);
    }

    for (auto *DIGlobal: infoFinder.global_variables()) {
        auto GlobalP = HandleGlobal(DIGlobal->getVariable());
    }

    for (auto *DISubProg: infoFinder.subprograms()) {
        auto SubProgP = HandleFunction(DISubProg);
    }
    FindTypesInFunctions();
    FinalizeTypes();

    FindUsesInGlobals();
//    FindUsesInFunctions();
//    for (auto &Global: M.getGlobalList()) {
//        debug = (Global.getName() == CommonHAKCAnalysis::getHAKCDebugName());
//        if (GlobalShouldBeSkipped(&Global)) {
//            if (debug) {
//                CommonHAKCAnalysis::getWriter() << "Skipping " << Global.getName();
//                if (Global.getNumUses() == 0) {
//                    if (debug) {
//                        CommonHAKCAnalysis::getWriter() << ": Zero uses";
//                    }
//                }
//                CommonHAKCAnalysis::getWriter() << "\n";
//            }
//            continue;
//        }
//        if (debug) {
//            CommonHAKCAnalysis::getWriter() << "Users of ";
//            Global.print(CommonHAKCAnalysis::getWriter());
//            CommonHAKCAnalysis::getWriter() << ":\n";
//            for (auto *User: Global.users()) {
//                CommonHAKCAnalysis::getWriter() << "\t";
//                for (auto *link: GetDefChain(User)) {
//                    link->print(CommonHAKCAnalysis::getWriter());
//                    for (auto *linkuser: link->users()) {
//                        CommonHAKCAnalysis::getWriter() << "\n\t\t";
//                        linkuser->print(CommonHAKCAnalysis::getWriter());
//                    }
//                    CommonHAKCAnalysis::getWriter() << "\n";
//                }
//                CommonHAKCAnalysis::getWriter() << "\n";
//            }
//        }
//        SmallVector<DIGlobalVariableExpression *, 5> DebugInfo;
//        Global.getDebugInfo(DebugInfo);
//        DIGlobalVariable *diGlobal = nullptr;
//        if (!DebugInfo.empty()) {
//            diGlobal = DebugInfo[0]->getVariable();
//        }
//        if (debug) {
//            CommonHAKCAnalysis::getWriter() << "Found " << std::to_string(DebugInfo.size()) << " DI objects\n";
//        }
//        std::shared_ptr<HAKCTypeInfo> HakcType = nullptr;
//        std::shared_ptr<HAKCGlobalInfoCommon> symbol = nullptr;
//        if (!diGlobal || !DiTypeShouldBeAnalyzed(diGlobal->getType())) {
//            if (debug) {
//                CommonHAKCAnalysis::getWriter() << Global.getName() << ": ";
//                if (!diGlobal) {
//                    CommonHAKCAnalysis::getWriter() << "Could not find debug info.\n";
//                    Global.print(CommonHAKCAnalysis::getWriter());
//                    CommonHAKCAnalysis::getWriter() << "\n";
//                } else {
//                    CommonHAKCAnalysis::getWriter() << "Type should not be Analyzed. Skipping...\n";
//                }
//            }
//            if (diGlobal && !DiTypeShouldBeAnalyzed(diGlobal->getType())) {
//                continue;
//            }
//            HakcType = addNoDebugType(&Global);
//            if (diGlobal) {
//                symbol = std::make_shared<HAKCGlobalInfo>(&Global, diGlobal, debug);
//            } else {
//                symbol = std::make_shared<HAKCNoDebugGlobalInfo>(&Global, debug);
//            }
//        } else {
//            auto *UnwrappedDIType = unwrapDIType(diGlobal->getType());
//            HakcType = addType(UnwrappedDIType, &Global);
//            symbol = std::make_shared<HAKCGlobalInfo>(&Global, diGlobal, debug);
//        }
//        if (!HakcType || !symbol) {
//            CommonHAKCAnalysis::getWriter() << "Did not create all needed objects for " << Global << "\n";
//            M.print(CommonHAKCAnalysis::getWriter(), nullptr);
//            CommonHAKCAnalysis::getWriter() << "\n";
//            throw std::exception();
//        }
//
//        symbols.insert(symbol);
//        for (auto *User: Global.users()) {
//            AddUserToType(HakcType, User);
//        }
//    }
//
//    for (auto &F: M.getFunctionList()) {
//        auto *subprogram = F.getSubprogram();
//        debug = F.getName() == CommonHAKCAnalysis::getHAKCDebugName();
//        if (functionShouldBeSkipped(&F) || !subprogram) {
//            if (debug) {
//                CommonHAKCAnalysis::getWriter() << "Skipping " << F.getName() << "\n";
//            }
//            continue;
//        }
//        if (debug) {
//            CommonHAKCAnalysis::getWriter() << "Analyzing " << F.getName() << "\n";
//        }
//        if (debug) {
//            M.print(CommonHAKCAnalysis::getWriter(), nullptr);
//            CommonHAKCAnalysis::getWriter() << "\n";
//        }
//        for (auto it = inst_begin(F); it != inst_end(F); ++it) {
//            Instruction *inst = &*it;
//            StructType *StructTy = nullptr;
//            if (auto *Store = dyn_cast<StoreInst>(inst)) {
//                auto *Def = GetDef(Store->getPointerOperand());
//                if (Def->getType()->isPointerTy()) {
//                    StructTy = dyn_cast<StructType>(Def->getType()->getPointerElementType());
//                }
//                if (debug && !StructTy) {
//                    CommonHAKCAnalysis::getWriter() << "Def: ";
//                    Def->print(CommonHAKCAnalysis::getWriter());
//                    CommonHAKCAnalysis::getWriter() << "\n";
//                }
//            } else if (auto *Load = dyn_cast<LoadInst>(inst)) {
//                auto *Def = GetDef(Load->getPointerOperand());
//                if (Def->getType()->isPointerTy()) {
//                    StructTy = dyn_cast<StructType>(Def->getType()->getPointerElementType());
//                }
//                if (debug && !StructTy) {
//                    CommonHAKCAnalysis::getWriter() << "Def: ";
//                    Def->print(CommonHAKCAnalysis::getWriter());
//                    CommonHAKCAnalysis::getWriter() << "\n";
//                }
//            }
//            if (StructTy && !StructTy->isOpaque()) {
//                auto *DiType = findDiType(StructTy);
//                if (DiType) {
//                    auto HakcType = getHAKCType(DiType);
//                    if (!HakcType) {
//                        HakcType = addType(DiType, StructTy);
//                    }
//                    HakcType->addUser(&F);
//                } else {
//                    if (debug) {
//                        CommonHAKCAnalysis::getWriter() << "Could not find DITYpe for ";
//                        StructTy->print(CommonHAKCAnalysis::getWriter());
//                        CommonHAKCAnalysis::getWriter() << " for Instruction ";
//                        inst->print(CommonHAKCAnalysis::getWriter());
//                        CommonHAKCAnalysis::getWriter() << "\n";
//                    }
//                }
//            } else {
//                if (debug && (isa<StoreInst>(inst) || isa<LoadInst>(inst))) {
//                    inst->print(CommonHAKCAnalysis::getWriter());
//                    CommonHAKCAnalysis::getWriter() << " does not involve a StructType\n";
//                }
//            }
//        }
//        auto symbol = std::make_shared<HAKCFunctionInfo>(&F, debug);
//        symbols.insert(symbol);
//    }

//    findEscapes();
}

//void hakc::HAKCTypeIdentifier::AddUserToType(std::shared_ptr<HAKCTypeInfo> &HakcType, User *User) {
//    if (auto *I = dyn_cast<Instruction>(User)) {
//        HakcType->addUser(I->getFunction());
//    } else if (auto *GV = dyn_cast<GlobalVariable>(User)) {
//        HakcType->addUser(GV);
//    } else if (auto *O = dyn_cast<Operator>(User)) {
//        for (auto *OUser: O->users()) {
//            AddUserToType(HakcType, OUser);
//        }
//    } else if (auto *C = dyn_cast<Constant>(User)) {
//        for (auto *CUser: C->users()) {
//            AddUserToType(HakcType, CUser);
//        }
//    } else {
//        CommonHAKCAnalysis::getWriter() << "Unhandled User: ";
//        User->print(CommonHAKCAnalysis::getWriter());
//        CommonHAKCAnalysis::getWriter() << "\n";
//        throw std::exception();
//    }
//}
//
//void hakc::HAKCTypeIdentifier::addEscapingSymbol(Function *F,
//                                                 std::string &escapingSymbol) {
//    bool found = false;
//    for (auto &f: symbols) {
//        if (f->getValue() == F) {
//            f->addEscapingSymbol(escapingSymbol);
//            found = true;
//            break;
//        }
//    }
//
//    if (!found && debug) {
//        CommonHAKCAnalysis::getWriter() << "Could not find function " << F->getName() << "\n";
//    }
//}
//
//std::shared_ptr<hakc::HAKCTypeInfo>
//hakc::HAKCTypeIdentifier::getHAKCType(const DIType *type) {
//    type = unwrapDIType(type);
//    for (auto p: types) {
//        if (p->getDiType() == type) {
//            return p;
//        }
//    }
//
//    if (debug) {
//        CommonHAKCAnalysis::getWriter() << "Could not get HAKCTypeInfo for type ";
//        printDIType(type, 0);
//        CommonHAKCAnalysis::getWriter() << "\n";
//    }
//
//    return nullptr;
//}
//
std::string hakc::HAKCTypeIdentifier::GetTransformedPath(StringRef Path) {
    if (Path.empty()) {
        return Path.str();
    }

    auto *SourcePath = std::getenv(hakc::HAKC_SOURCE_PATH.str().c_str());
    if (!SourcePath || std::strlen(SourcePath) == 0) {
        CommonHAKCAnalysis::getWriter() << "Invalid " << hakc::HAKC_SOURCE_PATH << "!\n";
        throw std::exception();
    }

    auto *BuildPath = std::getenv(hakc::HAKC_BUILD_PATH.str().c_str());
    if (!BuildPath || std::strlen(BuildPath) == 0) {
        CommonHAKCAnalysis::getWriter() << "Invalid " << hakc::HAKC_BUILD_PATH << "!\n";
        throw std::exception();
    }

    unsigned length = 0;
    std::string Replacement;
    if (Path.startswith(BuildPath)) {
        length = std::strlen(BuildPath);
        Replacement = HAKC_BUILD_PATH_REPLACEMENT.str();
    } else if (Path.startswith(SourcePath)) {
        length = std::strlen(SourcePath);
        Replacement = HAKC_SOURCE_PATH_REPLACEMENT.str();
    } else {
        CommonHAKCAnalysis::getWriter() << "Path " << Path << " does not start with either "
                                        << BuildPath << " or " << SourcePath << "!\n";
        throw std::exception();
    }

    if (!sys::path::is_separator(Path[length])) {
        Replacement += sys::path::get_separator();
    }

    auto Result = Path.str();
    Result.replace(0, length, Replacement);
    return Result;
}

//
void hakc::HAKCTypeIdentifier::OutputYAML(raw_ostream &out) {
    std::error_code err;
    auto RealPath = CommonHAKCAnalysis::GetModuleFullPath(M);

    out << "---\n";
    out << "CU: ";
    out << GetTransformedPath(RealPath);
    out << "\n";

    out << "types:\n";
    for (auto &it: types) {
        out << *it.second << "\n";
    }
    out << "symbols:\n";
    for (auto &it: globals) {
        out << *it.second << "\n";
    }
    for (auto &it: functions) {
        out << *it.second << "\n";
    }
//    for (auto &t: types) {
//        if (!t->getType()->isStructTy()) {
//            continue;
//        }
//        std::string yml = t->getYaml();
//        if (yml.empty()) {
//            continue;
//        }
//        StringRef yaml(yml);
//        SmallVector<StringRef> lines;
//        yaml.split(lines, "\n");
//        for (auto line: lines) {
//            out << "  " << line << "\n";
//        }
//    }
//
//    out << "symbols:\n";
//    for (auto &s: symbols) {
//        debug = s->getName() == CommonHAKCAnalysis::getHAKCDebugName();
//        if (debug) {
//            CommonHAKCAnalysis::getWriter() << "Outputting YAML for " << s->getValue()->getName() << "\n";
//            CommonHAKCAnalysis::getWriter() << "typeString = " << s-> << "\n";
//        }
//        std::string yml = s->getYaml();
//        if (yml.empty()) {
//            if (debug) {
//                CommonHAKCAnalysis::getWriter() << "\tYAML empty for " << s->getValue()->getName() << "\n";
//            }
//            continue;
//        }
//        StringRef yaml(yml);
//        SmallVector<StringRef> lines;
//        yaml.split(lines, "\n");
//        for (auto line: lines) {
//            out << "  " << line << "\n";
//        }
//    }
}
//
//std::shared_ptr<hakc::HAKCTypeInfo>
//hakc::HAKCTypeIdentifier::addType(const DIType *diType, GlobalObject *GO) {
//    std::shared_ptr<hakc::HAKCTypeInfo> result;
//    Type *Ty;
//    if (auto *F = dyn_cast<Function>(GO)) {
//        Ty = F->getFunctionType();
//    } else {
//        Ty = GO->getType()->getPointerElementType();
//    }
//    if (auto *StructTy = dyn_cast<StructType>(Ty)) {
//        if (StructTy->isOpaque()) {
//            GO->print(CommonHAKCAnalysis::getWriter());
//            CommonHAKCAnalysis::getWriter() << " is an opaque struct\n";
//            throw std::exception();
//        }
//    }
//    result = addType(diType, Ty);
//    result->addUser(GO);
//    return result;
//}
//
//std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::addType(const DIType *diType, Type *Ty) {
//    std::shared_ptr<hakc::HAKCDebugTypeInfo> result;
//    if (!diType) {
//        CommonHAKCAnalysis::getWriter() << "Null diType!\n";
//        throw std::exception();
//    }
//    if (!diType->getScope()) {
//        if (debug) {
//            CommonHAKCAnalysis::getWriter() << "No DIType Scope for Type " << *Ty << " and DIType " << *diType << "\n";
//        }
//    }
//
//    if (debug) {
//        CommonHAKCAnalysis::getWriter() << "Adding ";
//        printDIType(diType, 0);
//        CommonHAKCAnalysis::getWriter() << "\n";
//    }
//    diType = unwrapDIType(diType);
//
//    result = getHAKCType(diType);
//    if (result) {
//        if (debug) {
//            printDIType(diType, 0);
//            CommonHAKCAnalysis::getWriter() << " already added\n";
//        }
//    } else {
//        result = std::make_shared<HAKCDebugTypeInfo>(diType, Ty, debug);
//        if (auto *StructTy = dyn_cast<StructType>(Ty)) {
//            if (StructTy->isOpaque()) {
//                CommonHAKCAnalysis::getWriter() << "Tried to add Opaque StructType: ";
//                Ty->print(CommonHAKCAnalysis::getWriter());
//                CommonHAKCAnalysis::getWriter() << "\n";
//                throw std::exception();
//            }
//        }
//        types.insert(result);
//    }
//    return result;
//}
//
//std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::addNoDebugType(GlobalObject *GO) {
//    if (debug) {
//        CommonHAKCAnalysis::getWriter() << "Adding GlobalObject with no debug information: ";
//        GO->print(CommonHAKCAnalysis::getWriter());
//        CommonHAKCAnalysis::getWriter() << "\n";
//    }
//    Type *Ty = GO->getType()->getPointerElementType();
//    while (Ty->isArrayTy()) {
//        Ty = Ty->getArrayElementType();
//    }
//
//    auto result = std::make_shared<hakc::HAKCNoDebugTypeInfo>(Ty, debug);
//    types.insert(result);
//    result->addUser(GO);
//    return result;
//}
//
//Module &hakc::HAKCTypeIdentifier::GetModule() {
//    return M;
//}
