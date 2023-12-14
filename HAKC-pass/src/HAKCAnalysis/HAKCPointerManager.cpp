//
// Created by de29664 on 11/14/23.
//

#include "HAKCAnalysis/HAKCPointerManager.h"
#include "HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "HAKCAnalysis/ManagedHAKCPointer.h"

namespace hakc {
    HAKCPointerManager::HAKCPointerManager(HAKCFunctionAnalysis *Analysis) :
            ManagedPointers(),
            AuthenticatedCopies(),
            ProtectedCopies(),
            HAKCAnalysis(Analysis),
            DataAuthenticationsAdded(0),
            CodeAuthenticationsAdded(0),
            SafePointersAdded(0),
            ClonesAdded(0) {

    }

    bool HAKCPointerManager::PointerIsEligableForManagement(Value *Pointer, bool Debug) {
        /* The HAKCPointerManager::GetDef method performs some analysis to find a definition that could
        * be different from the "true" definition. Use the true definition to check if we are managing
        * constant strings.
        */
        auto *Definition = GetFunctionAnalysis()->getDef(Pointer, false, Debug);
        if(isa<ConstantPointerNull>(Definition)) {
            if(Debug) {
                CommonHAKCAnalysis::getWriter() << "Pointer Manager ignores null pointers\n";
            }
            return false;
        } else if(isa<ConstantInt>(Pointer)) {
            if(Debug) {
                CommonHAKCAnalysis::getWriter() << "Pointer Manager ignores Constant Ints\n";
            }
            return false;
        } else if(!CommonHAKCAnalysis::IsPointerLikeType(Pointer->getType())) {
            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "Pointer Manager ignores non-pointers\n";
            }
            return false;
        }

        if(auto *GV = dyn_cast<GlobalVariable>(Definition)) {
            if (CommonHAKCAnalysis::IsStringType(GV->getValueType())) {
                if (Debug) {
                    CommonHAKCAnalysis::getWriter() << "Pointer Manager is ignoring constant string ";
                    Definition->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << "\n";
                }
                return false;
            }
        }

        return true;
    }

    bool HAKCPointerManager::ManagePointer(Value *V, bool debug) {
        bool result = false;
        if(!PointerIsEligableForManagement(V, debug)) {
            if(debug) {
                CommonHAKCAnalysis::getWriter() << "Value ";
                V->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " is not eligable for management\n";
            }
            return result;
        }
        auto ManagedPointer = GetManagedPointer(V);
        if (!ManagedPointer) {
            ManagedPointer = std::make_shared<ManagedHAKCPointer>(V, this, debug);
            ManagedPointers.insert(ManagedPointer);
            result = true;
        } else {
            if(auto *CallI = dyn_cast<CallInst>(V)) {
                if(GetFunctionAnalysis()->IsHAKCTransferFunction(CallI->getCalledFunction())) {
                    ManagedPointer->RegisterManualHAKCTransfer(CallI);
                    return result;
                }
            }
            if(debug) {
                CommonHAKCAnalysis::getWriter() << "Pointer ";
                V->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " is already managed: " << ManagedPointer << "\n";
            }
        }
        return result;
    }

    HAKCFunctionAnalysis *HAKCPointerManager::GetFunctionAnalysis() {
        return HAKCAnalysis;
    }

    std::set<std::shared_ptr<ManagedHAKCPointer>> HAKCPointerManager::GetManagedPointers() {
        return ManagedPointers;
    }

    std::shared_ptr<ManagedHAKCPointer> HAKCPointerManager::GetManagedPointer(Value *V) {
        for (auto &ManagedPointer: ManagedPointers) {
            if (ManagedPointer == V) {
                return ManagedPointer;
            }
        }

        return nullptr;
    }

    std::shared_ptr<ManagedHAKCPointer> HAKCPointerManager::GetManagedPointerByBaseDefinition(Value *V) {
        for (auto &ManagedPointer: ManagedPointers) {
            if (ManagedPointer->GetBaseDefinition() == V) {
                return ManagedPointer;
            }
        }

        return nullptr;
    }

    bool HAKCPointerManager::empty() {
        return ManagedPointers.empty();
    }

    Value *HAKCPointerManager::GetDef(Value *V) {
        return GetDef(V, false);
    }

    Value *HAKCPointerManager::GetDef(Value *V, bool DebugActive) {
        auto *BaseDefinition = GetFunctionAnalysis()->getDef(V, false, DebugActive);

        if(isa<GlobalVariable>(BaseDefinition) &&
           !CommonHAKCAnalysis::IsStringType(BaseDefinition->getType())) {
            Value *NewBaseDefinition = nullptr;
            for(auto *Link : GetFunctionAnalysis()->findDefChain(V, false, DebugActive)) {
                if(isa<CallInst>(Link)) {
                    NewBaseDefinition = Link;
                    break;
                }
            }

            if(NewBaseDefinition) {
                if(DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Changing BaseDefinition from ";
                    BaseDefinition->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << " to ";
                    NewBaseDefinition->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << "\n";
                }
                BaseDefinition = NewBaseDefinition;
            }
        }

        return BaseDefinition;
    }

    Instruction *HAKCPointerManager::FindAuthenticatedCopy(Value *V) {
        if (ValueIsAuthenticatedCopy(V) || ValueIsAuthenticatedPointer(V)) {
            return dyn_cast<Instruction>(V);
        }
        return FindCopy(V, AuthenticatedCopies);
    }

    Instruction *HAKCPointerManager::FindProtectedCopy(Value *V) {
        if (ValueIsProtectedCopy(V) || ValueIsProtectedPointer(V)) {
            return dyn_cast<Instruction>(V);
        }
        return FindCopy(V, ProtectedCopies);
    }

    Instruction *HAKCPointerManager::FindCopy(Value *V, std::map<Instruction *, Instruction *> &CopyStorage) {
        for (auto &it: CopyStorage) {
            if (it.first == V) {
                return it.second;
            }
        }
        return nullptr;
    }

    Instruction *
    HAKCPointerManager::CloneInstruction(Instruction *I, std::map<Instruction *, Instruction *> &CopyStorage) {
        auto *Clone = I->clone();
        Clone->insertBefore(I);
        CopyStorage[I] = Clone;
        ClonesAdded++;
        return Clone;
    }

    Value *HAKCPointerManager::CreateProtectedInstruction(Value *Pointer, bool debug) {
        if (ValueIsProtectedCopy(Pointer) || ValueIsProtectedPointer(Pointer)) {
            return Pointer;
        }
        auto *ProtectedCopy = FindProtectedCopy(Pointer);
        if (ProtectedCopy) {
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Returning Protected Copy ";
                ProtectedCopy->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " for ";
                Pointer->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            return ProtectedCopy;
        }
        auto ManagedPointer = GetManagedPointerByBaseDefinition(Pointer);
        if (ManagedPointer && ManagedPointer->GetProtectedPointer()) {
            return ManagedPointer->GetProtectedPointer();
        }
        if (auto *I = dyn_cast<Instruction>(Pointer)) {
            auto Clone = CloneInstruction(I, ProtectedCopies);
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Created Protected Copy of ";
                I->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << ": ";
                Clone->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            return Clone;
        }
        return nullptr;
    }

    Value *HAKCPointerManager::CreateAuthenticatedInstruction(Value *Pointer, bool debug) {
        if (ValueIsAuthenticatedCopy(Pointer) || ValueIsAuthenticatedPointer(Pointer)) {
            if (debug) {
                if (ValueIsAuthenticatedCopy(Pointer)) {
                    CommonHAKCAnalysis::getWriter() << "Pointer is Authenticated Copy\n";
                } else if (ValueIsAuthenticatedPointer(Pointer)) {
                    CommonHAKCAnalysis::getWriter() << "Pointer is AuthenticatedPointer\n";
                }
            }
            return Pointer;
        }

        auto *AuthenticatedCopy = FindAuthenticatedCopy(Pointer);
        if (AuthenticatedCopy) {
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Returning Authenticated Copy ";
                AuthenticatedCopy->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " for ";
                Pointer->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            return AuthenticatedCopy;
        }
        auto ManagedPointer = GetManagedPointerByBaseDefinition(Pointer);
        if (ManagedPointer && ManagedPointer->GetAuthenticatedPointer()) {
            return ManagedPointer->GetAuthenticatedPointer();
        }
        if (auto *I = dyn_cast<Instruction>(Pointer)) {
            auto Clone = CloneInstruction(I, AuthenticatedCopies);
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Created Authenticated Copy of ";
                I->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << ": ";
                Clone->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            return Clone;
        }
        return nullptr;
    }

    bool HAKCPointerManager::ValueIsAuthenticated(Value *V) {
        return ValueIsAuthenticatedCopy(V) || ValueIsAuthenticatedPointer(V);
    }

    bool HAKCPointerManager::ValueIsAuthenticatedCopy(Value *V) {
        return ValueIsCopy(V, AuthenticatedCopies);
    }

    bool HAKCPointerManager::ValueIsProtectedCopy(Value *V) {
        return ValueIsCopy(V, ProtectedCopies);
    }

    bool HAKCPointerManager::ValueIsCopy(Value *V, std::map<Instruction *, Instruction *> &CopyStorage) {
        for (auto &it: CopyStorage) {
            if (it.second == V) {
                return true;
            }
        }

        return false;
    }

    bool HAKCPointerManager::ValueIsAuthenticatedPointer(Value *V) {
        for (auto &it: ManagedPointers) {
            if (it->GetAuthenticatedPointer() == V) {
                return true;
            }
        }
        return false;
    }

    bool HAKCPointerManager::ValueIsProtectedPointer(Value *V) {
        for (auto &it: ManagedPointers) {
            if (it->GetProtectedPointer() == V) {
                return true;
            }
        }
        return false;
    }

    void HAKCPointerManager::CreateAuthenticatedPointersAndAllClones(bool debug) {
        for (auto &ManagedPtr: GetManagedPointers()) {
            ManagedPtr->CreateBaseAuthenticatedPointer();
            if (debug && ManagedPtr->GetAuthenticatedPointer()) {
                CommonHAKCAnalysis::getWriter() << "Authenticated Pointer for " << ManagedPtr << ": ";
                ManagedPtr->GetAuthenticatedPointer()->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            ManagedPtr->CreatePointerUseClones();
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Created Authenticated and Protected Copies for " << ManagedPtr <<
                                                "\n";
            }
        }
    }

    void
    HAKCPointerManager::RegisterInstructionAsCopy(Instruction *I, std::map<Instruction *, Instruction *> &CopyStorage) {
        if (CopyStorage.find(I) != CopyStorage.end()) {
            StringRef StorageType = &CopyStorage == &AuthenticatedCopies ? "Authenticated" : "Protected";
            CommonHAKCAnalysis::getWriter() << "Already added ";
            I->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " to " << StorageType << " Copies\n";
            return;
        }
        CopyStorage[I] = I;
    }

    unsigned HAKCPointerManager::GetDataAuthenticationsAdded() {
        return DataAuthenticationsAdded;
    }

    unsigned HAKCPointerManager::GetCodeAuthenticationsAdded() {
        return CodeAuthenticationsAdded;
    }

    unsigned HAKCPointerManager::GetSafePointersAdded() {
        return SafePointersAdded;
    }

    unsigned HAKCPointerManager::GetClonesAdded() {
        return ClonesAdded;
    }

    unsigned HAKCPointerManager::GetTotalAdditions() {
        return GetClonesAdded() + GetSafePointersAdded() + GetCodeAuthenticationsAdded() +
               GetDataAuthenticationsAdded();
    }

    void HAKCPointerManager::RegisterInstructionAsProtectedCopy(Instruction *I) {
        RegisterInstructionAsCopy(I, ProtectedCopies);
    }

    void HAKCPointerManager::TransformClones(std::map<Instruction *, Instruction *> &CloneStorage, bool Debug) {
        bool TransformingAuthenticatedClones = (&CloneStorage == &AuthenticatedCopies);

        for (auto &it: CloneStorage) {
            auto *ReplacementI = it.second;
            for (unsigned i = 0; i < ReplacementI->getNumOperands(); i++) {
                Value *ReplacementOperand;
                if (TransformingAuthenticatedClones) {
                    ReplacementOperand = FindAuthenticatedCopy(ReplacementI->getOperand(i));
                } else {
                    ReplacementOperand = FindProtectedCopy(ReplacementI->getOperand(i));
                }
                if (!ReplacementOperand || ReplacementOperand == ReplacementI) {
                    if (Debug) {
                        if (!ReplacementOperand) {
                            CommonHAKCAnalysis::getWriter() << "Could not find ReplacementOperand\n";
                        } else if (ReplacementOperand == ReplacementI) {
                            CommonHAKCAnalysis::getWriter() << "ReplacementOperand ";
                            ReplacementOperand->print(CommonHAKCAnalysis::getWriter());
                            CommonHAKCAnalysis::getWriter() << " == ReplacementI\n";
                        }
                    }
                    continue;
                }
                if (Debug) {
                    CommonHAKCAnalysis::getWriter() << "Setting Argument " << std::to_string(i)
                                                    << " of ReplacementI ";
                    ReplacementI->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << " to be ";
                    ReplacementOperand->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << "\n";
                }
                ReplacementI->setOperand(i, ReplacementOperand);
            }
        }
    }

    void HAKCPointerManager::TransformPointers(bool Debug) {
        TransformClones(AuthenticatedCopies, Debug);
        TransformClones(ProtectedCopies, Debug);

        for (auto &ManagedPointer: ManagedPointers) {
            ManagedPointer->TransformUses();
        }
    }

    Value *HAKCPointerManager::CreateSafePointerAtLocation(Value *Pointer, Instruction *InsertLocation) {
        SafePointersAdded++;
        return GetFunctionAnalysis()->AddSafePointerCreationAtLocation(Pointer, InsertLocation);
    }

    Value *HAKCPointerManager::CreateAuthenticationAtLocation(Value *Pointer, Instruction *InsertLocation) {
        if (HAKCAnalysis->PointerShouldBeConsideredCode(Pointer)) {
            CodeAuthenticationsAdded++;
            return GetFunctionAnalysis()->AddCodeAuthCheckAtLocation(Pointer, InsertLocation);
        } else {
            DataAuthenticationsAdded++;
            return GetFunctionAnalysis()->AddDataAuthCheckAtLocation(Pointer, InsertLocation);
        }
    }
} // hakc