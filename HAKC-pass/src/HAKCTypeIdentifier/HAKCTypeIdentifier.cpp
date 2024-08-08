//
// Created by derrick on 9/8/21.
//
#include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"
#include "HAKC-defs.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "HAKCTypeIdentifier/HAKCFunctionInfo.h"
#include "HAKCTypeIdentifier/HAKCGlobalInfo.h"
#include "HAKCTypeIdentifier/HAKCTypeInfo.h"

#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"

#include "llvm/Support/Path.h"
#include "llvm/IR/DerivedTypes.h"

#include <sstream>
#include <algorithm>

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

void hakc::HAKCTypeIdentifier::AddTypeMapping(const DIType *type, const std::shared_ptr<HAKCTypeInfo> &HAKCType) {
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Adding mapping " << *type << " -> " << HAKCType->GetName() << "\n";
    }
    std::set<unsigned> TagsToSize = {
            dwarf::DW_TAG_structure_type,
            dwarf::DW_TAG_union_type,
    };
    if (isa<DIBasicType>(type) || TagsToSize.find(type->getTag()) != TagsToSize.end()) {
        HAKCType->SetSizeInBits(type->getSizeInBits());
    }
    HAKCType->SetDbgType(type);
    auto DbgTypeName = GetTypeName(type);
    HAKCType->SetDbgTypeName(DbgTypeName);
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
            out << GetTypeName(DerivedTy->getBaseType());
        } else if (DerivedTy->getTag() == dwarf::DW_TAG_volatile_type) {
            out << "volatile ";
            if (!DerivedTy->getBaseType()) {
                out << "void";
            } else {
                out << GetTypeName(DerivedTy->getBaseType());
            }
        } else if (DerivedTy->getTag() == dwarf::DW_TAG_const_type) {
            out << "const ";
            if (!DerivedTy->getBaseType()) {
                out << "void";
            } else {
                out << GetTypeName(DerivedTy->getBaseType());
            }
        } else if (DerivedTy->getTag() == dwarf::DW_TAG_restrict_type) {
            out << "restrict ";
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
        if (BaseTy->getEncoding() == dwarf::DW_ATE_boolean) {
            out << "bool";
        } else {
            out << BaseTy->getName();
        }
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
        if (auto *BasicType = dyn_cast<DIBasicType>(type)) {
            auto *IntTy = IntegerType::get(M.getContext(), BasicType->getSizeInBits());
            TypeP->SetLLVMType(IntTy);
        }
        AddTypeMapping(type, TypeP);
    } else if (auto *DerivedTy = dyn_cast<DIDerivedType>(type)) {
        std::set<unsigned> TagsToConsider = {
                dwarf::DW_TAG_pointer_type,
                dwarf::DW_TAG_array_type,
                dwarf::DW_TAG_const_type,
                dwarf::DW_TAG_typedef,
                dwarf::DW_TAG_volatile_type,
                dwarf::DW_TAG_restrict_type,
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

std::shared_ptr<hakc::HAKCGlobalInfo> hakc::HAKCTypeIdentifier::HandleGlobal(const DIGlobalVariable *DIGV) {
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Analyzing Global " << *DIGV << "\n";
    }

    auto *GV = FindGlobal(DIGV);
    if (!GV) {
        if (debug) {
            M.print(CommonHAKCAnalysis::getWriter(), nullptr);
            CommonHAKCAnalysis::getWriter() << "\nCould not find Global " << DIGV->getName() << "\n";
        }
        return nullptr;
    }
    auto DIGVTy = FindType(DIGV->getType());
    if (!DIGVTy) {
        DIGVTy = HandleType(DIGV->getType());
        if (!DIGVTy) {
            CommonHAKCAnalysis::getWriter() << "Unable to handle DIType " << *DIGV->getType() << " for Global " << *DIGV
                                            << "\n";
            throw std::exception();
        }
    }
    if (!DIGVTy->GetLLVMType()) {
        DIGVTy->SetLLVMType(GV->getValueType());
    }

    auto GVP = std::make_shared<HAKCGlobalInfo>(DIGV->getName(), debug);
    GVP->SetType(DIGVTy);
    GVP->SetGlobalVariable(GV);
    GVP->SetDefiningLocation(DIGV->getFile(), DIGV->getLine());
    if (DIGV->isLocalToUnit()) {
        GVP->SetLocalScope(DIGV->getScope());
    }
    AddGlobalMapping(DIGV, GVP);

    return GVP;
}

void hakc::HAKCTypeIdentifier::AddGlobalMapping(const DIGlobalVariable *DIGV,
                                                const std::shared_ptr<HAKCGlobalInfo> &HAKCSymbol) {
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Adding mapping " << *DIGV << " -> " << HAKCSymbol->GetName() << "\n";
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
        for (auto &FM: M.functions()) {
            if (FM.getSubprogram() == SubProg) {
                F = &FM;
                break;
            }
        }
    }

    if (!F) {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "\nCould not find Function " << SubProg->getName() << "\n";
        }
        return nullptr;
    }
    if (CommonHAKCAnalysis::isOutsideTransferFunc(F) || F->isIntrinsic()) {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << SubProg->getName() << " is a HAKC Transfer function\n";
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

    if (!DIGVTy->GetLLVMType()) {
        DIGVTy->SetLLVMType(F->getFunctionType());
    }
    auto FP = std::make_shared<HAKCFunctionInfo>(SubProg->getName(), debug);
    FP->SetType(DIGVTy);
    FP->SetFunction(F);
    FP->SetDefiningLocation(SubProg->getFile(), SubProg->getLine());
    if (SubProg->isLocalToUnit()) {
        FP->SetLocalScope(SubProg->getScope());
    }
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
            }
        }
    }
}

std::shared_ptr<hakc::HAKCFunctionInfo> hakc::HAKCTypeIdentifier::AddUnmappedFunction(Function *F) {
    for (const auto &UnmappedFunc: UnmappedFunctions) {
        if (UnmappedFunc->GetFunction() == F) {
            return UnmappedFunc;
        }
    }
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Adding unmapped Function " << F->getName() << "\n";
    }
    auto FuncInfo = std::make_shared<HAKCFunctionInfo>(F->getName(), debug);
    FuncInfo->SetFunction(F);
    auto HAKCType = FindCalledFunctionType(F->getFunctionType());
    if (!HAKCType) {
        HAKCType = CreateNoDebugType(F->getFunctionType());
        AddLLVMTypeMapping(HAKCType, HAKCType->GetLLVMType());
    } else if (debug) {
        CommonHAKCAnalysis::getWriter() << "HAKCType exists for " << F->getName() << "\n";
    }
    if (!HAKCType->GetLLVMType()) {
        HAKCType->SetLLVMType(F->getFunctionType());
    }
    FuncInfo->SetType(HAKCType);
    UnmappedFunctions.insert(FuncInfo);
    return FuncInfo;
}

std::shared_ptr<hakc::HAKCSymbolInfo> hakc::HAKCTypeIdentifier::AddUnmappedGlobal(GlobalObject *GlobalObj) {
    if (CommonHAKCAnalysis::IsStringType(GlobalObj->getValueType())) {
        return nullptr;
    }
    if (auto *F = dyn_cast<Function>(GlobalObj)) {
        return AddUnmappedFunction(F);
    } else if (auto *GV = dyn_cast<GlobalVariable>(GlobalObj)) {
        for (auto UnmappedGlobal: UnmappedGlobals) {
            if (UnmappedGlobal->GetGlobalVariable() == GV) {
                return UnmappedGlobal;
            }
        }
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Adding unmapped Global Variable " << GV->getName() << "\n";
        }
        auto GlobalInfo = std::make_shared<HAKCGlobalInfo>(GlobalObj->getName(), debug);
        auto HAKCType = FindType(GlobalObj->getValueType());
        if (!HAKCType) {
            HAKCType = CreateNoDebugType(GlobalObj->getValueType());
            AddLLVMTypeMapping(HAKCType, HAKCType->GetLLVMType());
        } else if (debug) {
            CommonHAKCAnalysis::getWriter() << "HAKCType exists for " << GV->getName() << "\n";
        }
        if (!HAKCType->GetLLVMType()) {
            HAKCType->SetLLVMType(GV->getValueType());
        }
        GlobalInfo->SetGlobalVariable(GV);
        GlobalInfo->SetType(HAKCType);
        UnmappedGlobals.insert(GlobalInfo);
        return GlobalInfo;
    } else {
        CommonHAKCAnalysis::getWriter() << "Unsupported GlobalObj: " << *GlobalObj << "\n";
        throw std::exception();
    }
}

void hakc::HAKCTypeIdentifier::AddUsedGlobals(std::set<GlobalObject *> &GlobalObjects,
                                              const std::shared_ptr<hakc::HAKCSymbolInfo> &UserSymbol) {
    for (auto *UsedGlobal: GlobalObjects) {
        auto Symbol = FindSymbol(UsedGlobal);
        if (!Symbol) {
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "\nGlobal " << UsedGlobal->getName() << " is used in "
                                                << UserSymbol->GetGlobalObj()->getName()
                                                << " but the Symbol could not be found\n";
            }
            Symbol = AddUnmappedGlobal(UsedGlobal);
        } else if (debug) {
            CommonHAKCAnalysis::getWriter() << "Found Symbol " << Symbol->GetName() << "\n";
        }
        if (Symbol) {
            UserSymbol->AddSymbolUse(Symbol);
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
            AddUsedGlobals(GlobalsUsed, it.second);
        }
    }
}

void hakc::HAKCTypeIdentifier::CreateIndirectCallSourceLink(Value *V,
                                                            std::vector<std::shared_ptr<HAKCIndirectCallSourceLink>> &Path) {
    if (auto *Arg = dyn_cast<Argument>(V)) {
        auto HAKCType = FindType(Arg->getType());
        if (!HAKCType) {
            CommonHAKCAnalysis::getWriter() << "Could not find HAKCType for Argument " << Arg->getArgNo()
                                            << " of Function " << Arg->getParent()->getName() << "\n";
            return;
        }
        if (!HAKCType->GetLLVMType()) {
            HAKCType->SetLLVMType(Arg->getType());
        }
        auto Link = std::make_shared<HAKCIndirectCallSourceLink>(Arg, HAKCType, debug);
        Path.push_back(Link);
    } else if (auto *GV = dyn_cast<GlobalVariable>(V)) {
        auto HAKCSymbol = FindGlobal(GV, true);
        if (!HAKCSymbol) {
            CommonHAKCAnalysis::getWriter() << "Could not find HAKC Symbol for " << *GV << "\n";
            return;
        }
        auto Link = std::make_shared<HAKCIndirectCallSourceLink>(HAKCSymbol, debug);
        Path.push_back(Link);
    } else if (auto *Load = dyn_cast<LoadInst>(V)) {
        auto *Pointer = Load->getPointerOperand();
        if (auto *GEP = dyn_cast<GEPOperator>(Pointer)) {
            auto *TyToCheck = GEP->getSourceElementType();
            auto HAKCType = FindType(TyToCheck);
            if (HAKCType /*&& GEP->hasAllConstantIndices()*/) {
                APInt Offset(64, 0);
                GEP->stripAndAccumulateInBoundsConstantOffsets(M.getDataLayout(), Offset);
                if (debug) {
                    CommonHAKCAnalysis::getWriter() << "Offset in bits for " << *GEP << ": "
                                                    << Offset.getSExtValue() << "\n";
                }
                if (!HAKCType->GetLLVMType()) {
                    HAKCType->SetLLVMType(TyToCheck);
                }
                auto Link = std::make_shared<HAKCIndirectCallSourceLink>(HAKCType, Offset.getSExtValue(), debug);
                Path.push_back(Link);
                CreateIndirectCallSourceLink(GEP->getPointerOperand(), Path);
            } else if (debug) {
                if (!GEP->hasAllConstantIndices()) {
                    CommonHAKCAnalysis::getWriter() << "GEP does not have all constant indices: " << *GEP << "\n";
                } else {
                    CommonHAKCAnalysis::getWriter() << "Could not find Load Pointer HAKC Type for " << *TyToCheck
                                                    << "\n";
                }
            }
        } else if (auto *GVal = dyn_cast<GlobalValue>(Pointer)) {
            auto HAKCSymbol = FindSymbol(GVal, true);
            if (!HAKCSymbol) {
                CommonHAKCAnalysis::getWriter() << "Unable to find Global " << GVal->getName() << "\n";
                return;
            }
            auto Link = std::make_shared<HAKCIndirectCallSourceLink>(HAKCSymbol, debug);
            Path.push_back(Link);
        } else {
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Unhandled Load Pointer Operand type: " << *Pointer << "\n";
            }
            auto *LoadTy = Load->getPointerOperand()->getType();
            auto HAKCType = FindType(LoadTy);
            if (HAKCType) {
                auto Link = std::make_shared<HAKCIndirectCallSourceLink>(HAKCType, 0, debug);
                Path.push_back(Link);
            }
        }
    } else if (auto *IntToPtr = dyn_cast<IntToPtrInst>(V)) {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Adding IntToPtr Link\n";
        }
        CreateIndirectCallSourceLink(IntToPtr->getOperand(0), Path);
    } else if (auto *CallI = dyn_cast<CallInst>(V)) {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "Adding Call Link\n";
        }
        CreateIndirectCallSourceLink(CallI->getCalledOperand(), Path);
    } else {
        CommonHAKCAnalysis::getWriter() << "Unhandled Link type: " << *V << "\n";
    }
}

void hakc::HAKCTypeIdentifier::FindIndirectCallSource(CallInst *CallI,
                                                      std::vector<std::shared_ptr<HAKCIndirectCallSourceLink>> &Path) {
    if (!CallI->isIndirectCall()) {
        CommonHAKCAnalysis::getWriter() << *CallI << " is not an indirect call\n";
        throw std::exception();
    }

    CreateIndirectCallSourceLink(CallI->getCalledOperand(), Path);
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Found " << Path.size() << " path links for " << *CallI->getCalledOperand()
                                        << "\n";
    }
}

FunctionType *hakc::HAKCTypeIdentifier::GetIndirectCallFunctionType(CallInst *CallI) {
    if (!CallI->isIndirectCall()) {
        CommonHAKCAnalysis::getWriter() << "Trying to get type from a Call that is not an indirect call\n";
        throw std::exception();
    }
    return dyn_cast<FunctionType>(CallI->getCalledOperand()->getType()->getPointerElementType());
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
            if (I->isDebugOrPseudoInst() || isa<IntrinsicInst>(I)) {
                continue;
            }
            FindAllGlobalsUsed(I, GlobalsUsed);
        }
        AddUsedGlobals(GlobalsUsed, it.second);

        for (auto InstIt = inst_begin(F); InstIt != inst_end(F); ++InstIt) {
            auto *I = &(*InstIt);
            if (I->isDebugOrPseudoInst() || isa<IntrinsicInst>(I)) {
                continue;
            }
            if (auto *Call = dyn_cast<CallInst>(I)) {
                if (Call->getCalledFunction()) {
                    auto FoundFunction = FindFunction(Call->getCalledFunction(), true);
                    if (!FoundFunction) {
                        if (debug) {
                            CommonHAKCAnalysis::getWriter() << "Could not find HAKC Symbol for Function "
                                                            << Call->getCalledFunction()->getName() << "\n";
                        }
                        FoundFunction = AddUnmappedFunction(Call->getCalledFunction());
                    }
                    it.second->AddDirectCall(FoundFunction);
                } else if (Call->isIndirectCall()) {
                    auto *FunctionTy = GetIndirectCallFunctionType(Call);
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Source of indirect call operand in Function "
                                                        << F->getName() << ": ";
                        CommonHAKCAnalysis::PrettyPrintValue(
                                AnalysisHelper->getDef(Call->getCalledOperand(), true, false),
                                CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                    }
                    auto HAKCType = FindCalledFunctionType(FunctionTy);
                    if (!HAKCType) {
                        CommonHAKCAnalysis::getWriter() << "Could not find called HAKCType for " << *Call
                                                        << " with Searched Type " << *FunctionTy
                                                        << " in Function " << F->getName() << "\n";
                        HAKCType = FindType(Call->getCalledOperand()->getType());
                        if (HAKCType) {
                            CommonHAKCAnalysis::getWriter() << "But the Pointer HAKCType exists: "
                                                            << HAKCType->GetName() << "\n";
                        }
                        throw std::exception();
                    }
                    std::vector<std::shared_ptr<HAKCIndirectCallSourceLink>> SourcePath;
                    if (!HAKCType->GetLLVMType()) {
                        HAKCType->SetLLVMType(FunctionTy);
                    }
                    FindIndirectCallSource(Call, SourcePath);
                    auto Source = std::make_shared<HAKCIndirectCallSource>(SourcePath, HAKCType, debug);
                    it.second->AddIndirectCall(Source);
                }
            }
        }
    }
}

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::FindType(Type *Ty) {
    auto Search = [Ty](Type *T) {
        return Ty == T;
    };

    for (auto &it: LLVMTypeMapping) {
        if (std::any_of(it.second.begin(), it.second.end(), Search)) {
            if (Ty->isIntegerTy()) {
                if (it.first->GetDbgType() && !isa<DIBasicType>(it.first->GetDbgType())) {
                    continue;
                }
            }
            return it.first;
        }
    }

    return nullptr;
}

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::FindCalledFunctionType(FunctionType *FunctionTy) {
    if (!FunctionTy) {
        CommonHAKCAnalysis::getWriter() << "Trying to find null FunctionTy\n";
        throw std::exception();
    }

    return FindType(FunctionTy);
}

std::shared_ptr<hakc::HAKCFunctionInfo> hakc::HAKCTypeIdentifier::FindFunction(Function *F, bool SearchUnmapped) {
    for (auto &it: functions) {
        if (it.second->GetFunction() == F) {
            return it.second;
        }
    }
    if (SearchUnmapped) {
        for (auto UnmappedFunction: UnmappedFunctions) {
            if (UnmappedFunction->GetFunction() == F) {
                return UnmappedFunction;
            }
        }
    }
    return nullptr;
}

std::shared_ptr<hakc::HAKCGlobalInfo> hakc::HAKCTypeIdentifier::FindGlobal(GlobalVariable *GV, bool SearchUnmapped) {
    for (auto &it: globals) {
        if (it.second->GetGlobalVariable() == GV) {
            return it.second;
        }
    }
    if (SearchUnmapped) {
        for (auto UnmappedGlobal: UnmappedGlobals) {
            if (UnmappedGlobal->GetGlobalVariable() == GV) {
                return UnmappedGlobal;
            }
        }
    }
    return nullptr;
}

std::shared_ptr<hakc::HAKCSymbolInfo> hakc::HAKCTypeIdentifier::FindSymbol(Value *V, bool SearchUnmapped) {
    if (auto *GV = dyn_cast<GlobalVariable>(V)) {
        return FindGlobal(GV, SearchUnmapped);
    }
    if (auto *F = dyn_cast<Function>(V)) {
        return FindFunction(F, SearchUnmapped);
    }
    return nullptr;
}

hakc::HAKCSymbolP hakc::HAKCTypeIdentifier::FindYamlSymbol(const hakc::HAKCYamlSymbol &YamlSymbol) {
    for (auto &it: globals) {
        if (YamlSymbol == it.second) {
            return it.second;
        }
    }
    for (auto &Unmapped: UnmappedGlobals) {
        if (YamlSymbol == Unmapped) {
            return Unmapped;
        }
    }
    for (auto &it: functions) {
        if (YamlSymbol == it.second) {
            return it.second;
        }
    }
    for (auto &Unmapped: UnmappedFunctions) {
        if (YamlSymbol == Unmapped) {
            return Unmapped;
        }
    }
    return nullptr;
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
                if (isa<UndefValue>(V)) {
                    if (debug) {
                        CommonHAKCAnalysis::getWriter() << "Skipping undef Value in Instruction " << *I << "\n";
                    }
                    continue;
                }
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
            } else if (auto *CallI = dyn_cast<CallInst>(I)) {
                if (CallI->isIndirectCall()) {
                    auto *FunctionTy = GetIndirectCallFunctionType(CallI);
                    auto HAKCType = FindCalledFunctionType(FunctionTy);
                    if (!HAKCType) {
                        HAKCType = CreateNoDebugType(FunctionTy);
                        AddLLVMTypeMapping(HAKCType, FunctionTy);
                    }
                }
            }
        }
    }
}

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::CreateNoDebugType(Type *Ty) {
    std::string Name;
    llvm::raw_string_ostream sstream(Name);
    if (auto *StructTy = dyn_cast<StructType>(Ty)) {
        if (StructTy->hasName()) {
            sstream << StructTy->getName();
        } else {
            sstream << *Ty;
        }
    } else {
        sstream << *Ty;
    }

    auto HAKCType = std::make_shared<HAKCTypeInfo>(Name, debug);
    HAKCType->SetLLVMType(Ty);
    return HAKCType;
}

bool hakc::HAKCTypeIdentifier::LLVMTypeMappingSanityCheck(const DIType *type, Type *Ty) {
    if (!type) {
        CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " type is null\n";
        throw std::exception();
    }
    if (!Ty) {
        CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " Ty is null\n";
        throw std::exception();
    }

    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Running sanity check on DIType " << GetTypeName(type) << " and LLVM Type "
                                        << *Ty << "\n";
    }
    if (auto *BasicTy = dyn_cast<DIBasicType>(type)) {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "DIBasicType Sanity Checks\n";
        }
        return BasicTy->getSizeInBits() == Ty->getScalarSizeInBits();
    } else if (auto *DerivedTy = dyn_cast<DIDerivedType>(type)) {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "DIDerivedType Sanity Checks\n";
        }
        std::set<unsigned> BaseTypeCheckTags = {
                dwarf::DW_TAG_const_type,
                dwarf::DW_TAG_typedef,
                dwarf::DW_TAG_volatile_type,
                dwarf::DW_TAG_restrict_type,
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
        }
    } else if (isa<DISubroutineType>(type)) {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "DISubroutineType Sanity Checks\n";
        }
        return Ty->isFunctionTy();
    } else if (auto *CompositeTy = dyn_cast<DICompositeType>(type)) {
        if (debug) {
            CommonHAKCAnalysis::getWriter() << "DICompositeType Sanity Checks\n";
            printDIType(type, 0);
            CommonHAKCAnalysis::getWriter() << "\n";
        }
        if (type->getTag() == dwarf::DW_TAG_structure_type || type->getTag() == dwarf::DW_TAG_union_type) {
            if (!Ty->isStructTy()) {
                return false;
            }
            auto *StructTy = dyn_cast<StructType>(Ty);
            if (!type->getName().empty()) {
                auto StructName = ConstructStructName(StructTy);
                return type->getName() != StructName;
            }
        } else if (type->getTag() == dwarf::DW_TAG_array_type) {
            if (!Ty->isArrayTy()) {
                return false;
            }
            if (CompositeTy->getBaseType()) {
                return LLVMTypeMappingSanityCheck(CompositeTy->getBaseType(), Ty->getArrayElementType());
            }
        } else if (type->getTag() == dwarf::DW_TAG_enumeration_type) {
            return Ty->isIntegerTy();
        }
    }

    return true;
}

void hakc::HAKCTypeIdentifier::AddLLVMTypeMapping(const std::shared_ptr<HAKCTypeInfo> &HAKCType, Type *Ty) {
    if (!Ty) {
        CommonHAKCAnalysis::getWriter() << "Trying to add null LLVM Type mapping to " << HAKCType->GetName() << "\n";
        throw std::exception();
    }
    if (!HAKCType) {
        CommonHAKCAnalysis::getWriter() << "Trying to add null HAKCType mapping for LLVM Type " << *Ty << "\n";
        throw std::exception();
    }

    if (HAKCType->GetDbgType()) {
        if (!LLVMTypeMappingSanityCheck(HAKCType->GetDbgType(), Ty)) {
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Mapping \"" << HAKCType->GetName() << "\" -> " << *Ty
                                                << " did not pass sanity check\n";
            }
            return;
        }
    }

    if (debug) {
        CommonHAKCAnalysis::getWriter() << "Adding LLVM Type Mapping \"" << HAKCType->GetName() << "\" -> " << *Ty
                                        << "\n";
    }

    auto it = LLVMTypeMapping.find(HAKCType);
    if (it == LLVMTypeMapping.end()) {
        LLVMTypeMapping[HAKCType].push_back(Ty);
    } else {
        auto *ExistingTy = *it->second.begin();
        if (ExistingTy->getTypeID() != Ty->getTypeID()) {
            CommonHAKCAnalysis::getWriter() << "Trying to change LLVM Type Mapping from " << *ExistingTy << " to "
                                            << *Ty << "\n";
            throw std::exception();
        }
        LLVMTypeMapping[HAKCType].push_back(Ty);
    }
    if (HAKCType->GetDbgType()) {
        if (auto *DerivedTy = dyn_cast<DIDerivedType>(HAKCType->GetDbgType())) {
            if (!DerivedTy->getBaseType()) {
                return;
            }
            auto DerivedHAKCType = FindType(DerivedTy->getBaseType());
            if (DerivedHAKCType) {
                if (DerivedTy->getTag() == dwarf::DW_TAG_typedef ||
                    DerivedTy->getTag() == dwarf::DW_TAG_const_type ||
                    DerivedTy->getTag() == dwarf::DW_TAG_volatile_type ||
                    DerivedTy->getTag() == dwarf::DW_TAG_restrict_type) {
                    AddLLVMTypeMapping(DerivedHAKCType, Ty);
                } else if (DerivedTy->getTag() == dwarf::DW_TAG_pointer_type) {
                    AddLLVMTypeMapping(DerivedHAKCType, Ty->getPointerElementType());
                } else if (DerivedTy->getTag() == dwarf::DW_TAG_array_type) {
                    AddLLVMTypeMapping(DerivedHAKCType, Ty->getArrayElementType());
                }
            }
        }
    }
}

hakc::HAKCTypeIdentifier::HAKCTypeIdentifier(Module &M, CommonHAKCAnalysis *AnalysisHelper)
        : HAKCDebugInfoProcessor(M), AnalysisHelper(AnalysisHelper), types(), globals(), functions(),
          LLVMTypeMapping(), CurrentAnonID(0) {
//    debug = true;
    if (debug) {
        M.print(CommonHAKCAnalysis::getWriter(), nullptr);
        CommonHAKCAnalysis::getWriter() << "\n";
    }

    if (debug) {
        CommonHAKCAnalysis::getWriter() << "!!!! Starting Type Handling !!!!\n";
    }
    for (auto *DITy: infoFinder.types()) {
        auto TypeP = HandleType(DITy);
    }
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "!!!! Finished Type Handling !!!!\n";
    }

    if (debug) {
        CommonHAKCAnalysis::getWriter() << "!!!! Starting Global Handling !!!!\n";
    }
    for (auto *DIGlobal: infoFinder.global_variables()) {
        auto GlobalP = HandleGlobal(DIGlobal->getVariable());
    }
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "!!!! Finished Global Handling !!!!\n";
    }

    if (debug) {
        CommonHAKCAnalysis::getWriter() << "!!!! Starting Function Handling !!!!\n";
    }
    for (auto *DISubProg: infoFinder.subprograms()) {
        auto SubProgP = HandleFunction(DISubProg);
    }
    if (debug) {
        CommonHAKCAnalysis::getWriter() << "!!!! Finished Function Handling !!!!\n";
    }
    FindTypesInFunctions();

    FindUsesInGlobals();
    FindUsesInFunctions();
}

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

    unsigned length;
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

void hakc::HAKCTypeIdentifier::OutputYAML(raw_ostream &out) {
    auto RealPath = CommonHAKCAnalysis::GetModuleFullPath(M);

    out << "---\n";
    out << "CU: ";
    out << GetTransformedPath(RealPath);
    out << "\n";

    auto GlobalCount = globals.size() + UnmappedGlobals.size();
    if (GlobalCount > 0) {
        out << "globals:\n";
        std::vector<std::shared_ptr<HAKCGlobalInfo>> SortedGlobals;
        SortedGlobals.reserve(GlobalCount);
        for (auto &it: globals) {
            SortedGlobals.push_back(it.second);
        }
        for (const auto &Unmapped: UnmappedGlobals) {
            SortedGlobals.push_back(Unmapped);
        }
        llvm::sort(SortedGlobals.begin(), SortedGlobals.end(),
                   [](const std::shared_ptr<HAKCGlobalInfo> &LHS, const std::shared_ptr<HAKCGlobalInfo> &RHS) {
                       return LHS->GetName() < RHS->GetName();
                   });
        for (auto &it: SortedGlobals) {
            out.indent(HAKCInfo::IndentSpaces()) << "- " << it->GetYaml(HAKCInfo::IndentSpaces()) << "\n";
        }
    }

    auto FunctionCount = functions.size() + UnmappedFunctions.size();
    if (FunctionCount > 0) {
        out << "functions:\n";
        std::vector<std::shared_ptr<HAKCFunctionInfo>> SortedFunctions;
        SortedFunctions.reserve(FunctionCount);
        for (auto &it: functions) {
            SortedFunctions.push_back(it.second);
        }
        for (const auto &Unmapped: UnmappedFunctions) {
            SortedFunctions.push_back(Unmapped);
        }
        llvm::sort(SortedFunctions.begin(), SortedFunctions.end(),
                   [](const std::shared_ptr<HAKCFunctionInfo> &LHS, const std::shared_ptr<HAKCFunctionInfo> &RHS) {
                       return LHS->GetName() < RHS->GetName();
                   });
        for (auto &it: SortedFunctions) {
            out.indent(HAKCInfo::IndentSpaces()) << "- " << it->GetYaml(HAKCInfo::IndentSpaces()) << "\n";
        }
    }
}
