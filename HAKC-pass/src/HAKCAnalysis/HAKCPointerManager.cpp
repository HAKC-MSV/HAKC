//
// Created by de29664 on 11/14/23.
//

#include "HAKCAnalysis/HAKCPointerManager.h"
#include "HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "HAKCAnalysis/ManagedHAKCPointer.h"

namespace hakc {
    HAKCPointerManager::HAKCPointerManager(HAKCFunctionAnalysis *Analysis) :
            ManagedPointers(),
            AuthenticatedValues(),
            ProtectedValues(),
            HAKCAnalysis(Analysis),
            DataAuthenticationsAdded(0),
            CodeAuthenticationsAdded(0),
            SafePointersAdded(0),
            ClonesAdded(0),
            IsCompartmentalized(false) {
    }

    bool HAKCPointerManager::PointerIsEligibleForManagement(Value *Pointer, bool Debug) {
        /* The HAKCPointerManager::GetDef method performs some analysis to find a definition that could
        * be different from the "true" definition. Use the true definition to check if we are managing
        * constant strings.
        */
        auto *Definition = GetFunctionAnalysis()->getDef(Pointer, false, Debug);
        if (isa<ConstantPointerNull>(Definition)) {
            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "Pointer Manager ignores null pointers\n";
            }
            return false;
        } else if (isa<ConstantInt>(Pointer)) {
            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "Pointer Manager ignores Constant Ints\n";
            }
            return false;
        } else if (!CommonHAKCAnalysis::IsPointerLikeType(Pointer->getType())) {
            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "Pointer Manager ignores non-pointers\n";
            }
            return false;
        }

        if (auto *GV = dyn_cast<GlobalVariable>(Definition)) {
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
        if (!PointerIsEligibleForManagement(V, debug)) {
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Value ";
                V->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " is not eligible for management\n";
            }
            return result;
        }
        auto ManagedPointer = GetManagedPointer(V);
        if (!ManagedPointer) {
            ManagedPointer = std::make_shared<ManagedHAKCPointer>(V, this, debug);
            ManagedPointers.insert(ManagedPointer);
            result = true;
        } else {
            if (auto *CallI = dyn_cast<CallInst>(V)) {
                if (GetFunctionAnalysis()->IsHAKCTransferFunction(CallI->getCalledFunction())) {
                    ManagedPointer->RegisterManualHAKCTransfer(CallI);
                    return result;
                }
            }
            if (debug) {
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

    bool HAKCPointerManager::empty() {
        return ManagedPointers.empty();
    }

    Value *HAKCPointerManager::GetDef(Value *V) {
        return GetDef(V, false);
    }

    Value *HAKCPointerManager::GetDef(Value *V, bool DebugActive) {
        auto *BaseDefinition = GetFunctionAnalysis()->getDef(V, false, DebugActive);

        if (isa<GlobalVariable>(BaseDefinition) &&
            !CommonHAKCAnalysis::IsStringType(BaseDefinition->getType())) {
            Value *NewBaseDefinition = nullptr;
            for (auto *Link: GetFunctionAnalysis()->findDefChain(V, false, DebugActive)) {
                if (isa<CallInst>(Link)) {
                    NewBaseDefinition = Link;
                    break;
                }
            }

            if (NewBaseDefinition) {
                if (DebugActive) {
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

    Instruction *
    HAKCPointerManager::CloneInstruction(Instruction *I) {
        auto *Clone = I->clone();
        Clone->insertBefore(I);
        ClonesAdded++;
        return Clone;
    }

    bool HAKCPointerManager::CloneableManagedPointer(Value *V) {
        bool IsUnclonable = isa<LoadInst>(V);
        if (auto *Call = dyn_cast<CallInst>(V)) {
            IsUnclonable = !GetFunctionAnalysis()->IsIntrinsicsNeedingCloning(Call);
        }

        return !IsUnclonable;
    }

    Value *HAKCPointerManager::CreateProtectedValue(Value *Pointer, bool Debug) {
        if (!CloneableManagedPointer(Pointer)) {
            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "Called CreateProtectedValue for Uncloneable Pointer ";
                Pointer->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            return nullptr;
        }

        auto *ProtectedValue = FindProtectedValue(Pointer);
        if (ProtectedValue) {
            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "Returning Protected Version ";
                ProtectedValue->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " for ";
                Pointer->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            return ProtectedValue;
        }
        auto ManagedPtr = GetManagedPointer(Pointer);
        if (ManagedPtr && ManagedPtr->GetBaseDefinition() == Pointer) {
            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "Returning ProtectedPointer\n";
            }
            return ManagedPtr->GetProtectedPointer();
        }

        if (auto *I = dyn_cast<Instruction>(Pointer)) {
            auto Clone = CloneInstruction(I);
            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "Created Protected Version of ";
                I->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << ": ";
                Clone->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            AddProtectedPointer(Pointer, Clone);
            return Clone;
        }
        return nullptr;
    }

    Value *HAKCPointerManager::CreateAuthenticatedValue(Value *Pointer, bool Debug) {
        if (!CloneableManagedPointer(Pointer)) {
            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "Called CreateAuthenticatedValue for Uncloneable Pointer ";
                Pointer->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            return nullptr;
        }

        auto *AuthenticatedCopy = FindAuthenticatedValue(Pointer);
        if (AuthenticatedCopy) {
            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "Returning Authenticated Copy ";
                AuthenticatedCopy->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " for ";
                Pointer->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            return AuthenticatedCopy;
        }

        if (auto *I = dyn_cast<Instruction>(Pointer)) {
            auto Clone = CloneInstruction(I);
            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "Created Authenticated Copy of ";
                I->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << ": ";
                Clone->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            AddAuthenticatedPointer(Pointer, Clone);
            auto ManagedPtr = GetManagedPointer(Pointer);
            if (ManagedPtr) {
                if (!ManagedPtr->GetAuthenticatedPointer()) {
                    if (Debug) {
                        CommonHAKCAnalysis::getWriter() << "Requested Authenticated Value of ";
                        Pointer->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << " in function " << I->getFunction()->getName();
                        CommonHAKCAnalysis::getWriter() << " but authenticated pointer has not been created.\n";
                    }
                    ManagedPtr->SetPointerRefreshNeeded(true);
                }
            }
            return Clone;
        }
        return nullptr;
    }

    void HAKCPointerManager::CreateAuthenticatedPointersAndAllClones(bool Debug) {
        if (Debug) {
            CommonHAKCAnalysis::getWriter() << "Starting Base Pointer Authentication determination\n";
        }
        for (auto &ManagedPtr: GetManagedPointers()) {
            ManagedPtr->DetermineIfBasePointerIsAuthenticated();
        }
        bool AuthenticationChanged = true;
        /* Incoming values to PHINodes might have changed authentication, so redo computation */
        while (AuthenticationChanged) {
            AuthenticationChanged = false;
            for (auto &ManagedPtr: GetManagedPointers()) {
                if (Debug) {
                    CommonHAKCAnalysis::getWriter() << "Checking Base Authenticated for " << ManagedPtr << "\n";
                }
                auto OriginalAuthentication = ManagedPtr->BaseIsAuthenticatedPointer();
                ManagedPtr->DetermineIfBasePointerIsAuthenticated();
                if (ManagedPtr->BaseIsAuthenticatedPointer() != OriginalAuthentication) {
                    if (Debug) {
                        CommonHAKCAnalysis::getWriter() << ManagedPtr << " changed base authentication value\n";
                    }
                    AuthenticationChanged = true;
                }
            }
        }
        if (Debug) {
            CommonHAKCAnalysis::getWriter() << "Base Pointer Authentication determination completed\n";
        }

        bool RefreshNeeded;
        do {
            RefreshNeeded = false;
            for (auto &ManagedPtr: GetManagedPointers()) {
                if (ManagedPtr->NeedsPointerReplacementRefresh()) {
                    ManagedPtr->CreateBaseAuthenticatedPointer();
                    if (Debug && ManagedPtr->GetAuthenticatedPointer()) {
                        CommonHAKCAnalysis::getWriter() << "Authenticated Pointer for " << ManagedPtr << ": ";
                        ManagedPtr->GetAuthenticatedPointer()->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                    }
                    ManagedPtr->CreatePointerUseClones();
                    if (Debug) {
                        CommonHAKCAnalysis::getWriter() << "Created Authenticated and Protected Copies for "
                                                        << ManagedPtr
                                                        << "\n";
                    }
                    ManagedPtr->SetPointerRefreshNeeded(false);
                    RefreshNeeded = true;
                }
            }
        } while (RefreshNeeded);
    }

    Value *HAKCPointerManager::FindManagedValue(std::map<Value *, Value *> &Storage, Value *Target) {
        for (auto &it: Storage) {
            if (it.first == Target) {
                return it.second;
            }
        }
        return nullptr;
    }


    Value *HAKCPointerManager::FindAuthenticatedValue(Value *V) {
        return FindManagedValue(AuthenticatedValues, V);
    }

    Value *HAKCPointerManager::FindProtectedValue(Value *V) {
        return FindManagedValue(ProtectedValues, V);
    }

    bool HAKCPointerManager::ManagedPointerFinder(Value *V,
                                                  std::function<bool(
                                                          const std::shared_ptr<ManagedHAKCPointer> &)> const &Search) {
        if (!V) {
            return false;
        }

        return std::any_of(ManagedPointers.begin(), ManagedPointers.end(), Search);
    }

    bool HAKCPointerManager::ValueIsAuthenticatedPointer(Value *V) {
        auto Search = [V](const std::shared_ptr<ManagedHAKCPointer> &Ptr) {
            return V == Ptr->GetAuthenticatedPointer();
        };
        return ManagedPointerFinder(V, Search);
    }

    bool HAKCPointerManager::ValueIsProtectedPointer(Value *V) {
        auto Search = [V](const std::shared_ptr<ManagedHAKCPointer> &Ptr) { return V == Ptr->GetProtectedPointer(); };
        return ManagedPointerFinder(V, Search);
    }

    void
    HAKCPointerManager::AddHAKCPointerReplacement(std::map<Value *, Value *> &Storage, Value *Ptr, Value *Replacement,
                                                  bool Debug) {
        bool CreatingAuthenticatedReplacements = (&Storage == &AuthenticatedValues);
        StringRef StorageName = CreatingAuthenticatedReplacements ? "Authenticated" : "Protected";

        if (!Ptr) {
            CommonHAKCAnalysis::getWriter() << "Trying to add null " << StorageName << " Pointer Replacement\n";
            throw std::exception();
        }
        if (Storage.find(Ptr) == Storage.end()) {
            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "Adding New " << StorageName << "Pointer Replacement: ";
                Ptr->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " -> ";
                if (Replacement) {
                    Replacement->print(CommonHAKCAnalysis::getWriter());
                } else {
                    CommonHAKCAnalysis::getWriter() << "nullptr";
                }
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            Storage[Ptr] = Replacement;
        } else {
            auto *ExistingPointer = Storage[Ptr];
            if (ExistingPointer && ExistingPointer != Ptr && Replacement && ExistingPointer != Replacement) {
                CommonHAKCAnalysis::getWriter() << "Trying to replace existing " << StorageName << " Replacement ";
                ExistingPointer->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " with ";
                Replacement->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
                GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
                throw std::exception();
            }
            if (Replacement) {
                if (Debug) {
                    CommonHAKCAnalysis::getWriter() << "Setting " << StorageName << "Pointer Replacement: ";
                    Ptr->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << " -> ";
                    Replacement->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << "\n";
                }
                Storage[Ptr] = Replacement;
            } else {
                if (Debug) {
                    CommonHAKCAnalysis::getWriter() << "Tried to add null to " << StorageName
                                                    << "Pointer Replacement for " << *Ptr << "\n";
                }
            }
        }
    }

    void HAKCPointerManager::AddAuthenticatedPointer(Value *Pointer, Value *Replacement, bool Debug) {
        AddHAKCPointerReplacement(AuthenticatedValues, Pointer, Replacement, Debug);
    }

    void HAKCPointerManager::AddProtectedPointer(Value *Pointer, Value *Replacement, bool Debug) {
        if (!FunctionIsCompartmentalized()) {
            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "Protected Pointer is not set for uncompartmentalized functions\n";
            }
            return;
        }
        AddHAKCPointerReplacement(ProtectedValues, Pointer, Replacement, Debug);
    }

    void HAKCPointerManager::AddAuthenticatedPointer(Value *Ptr, Value *Replacement) {
        AddAuthenticatedPointer(Ptr, Replacement, FunctionIsCompartmentalized());
    }

    void HAKCPointerManager::AddProtectedPointer(Value *Ptr, Value *Replacement) {
        AddProtectedPointer(Ptr, Replacement, FunctionIsCompartmentalized());
    }

    bool HAKCPointerManager::FunctionIsCompartmentalized() const {
        return IsCompartmentalized;
    }

    void HAKCPointerManager::SetFunctionIsCompartmentalized(bool FunctionIsCompartmentalized) {
        IsCompartmentalized = FunctionIsCompartmentalized;
    }

    void HAKCPointerManager::PrintManagedValues(const std::map<Value *, Value *> &Storage) {
        for (auto &it: Storage) {
            it.first->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " -> ";
            if (it.second) {
                it.second->print(CommonHAKCAnalysis::getWriter());
            } else {
                CommonHAKCAnalysis::getWriter() << "nullptr";
            }
            CommonHAKCAnalysis::getWriter() << "\n\n";
        }
    }

    void HAKCPointerManager::PrintProtectedValues() const {
        PrintManagedValues(ProtectedValues);
    }

    void HAKCPointerManager::PrintAuthenticatedValues() const {
        PrintManagedValues(AuthenticatedValues);
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

    void HAKCPointerManager::TransformPointers(bool Debug) {
        for (auto &ManagedPointer: ManagedPointers) {
            ManagedPointer->TransformUses();
        }
    }

    bool HAKCPointerManager::ValueWillBeAuthenticated(Value *V) {
        auto *Def = GetDef(V);
        return AuthenticatedValues.find(Def) != AuthenticatedValues.end();
    }

    Value *HAKCPointerManager::CreateSafePointerAtLocation(Value *Pointer, Instruction *InsertLocation) {
        auto *Managed = FindAuthenticatedValue(Pointer);
        if (Managed) {
            return Managed;
        }

        SafePointersAdded++;
        return GetFunctionAnalysis()->AddSafePointerCreationAtLocation(Pointer, InsertLocation);
    }

    Value *HAKCPointerManager::CreateAuthenticationAtLocation(Value *Pointer, Instruction *InsertLocation) {
        auto *Managed = FindAuthenticatedValue(Pointer);
        if (Managed) {
            return Managed;
        }

        if (HAKCAnalysis->PointerShouldBeConsideredCode(Pointer)) {
            CodeAuthenticationsAdded++;
            return GetFunctionAnalysis()->AddCodeAuthCheckAtLocation(Pointer, InsertLocation);
        } else {
            DataAuthenticationsAdded++;
            return GetFunctionAnalysis()->AddDataAuthCheckAtLocation(Pointer, InsertLocation);
        }
    }
} // hakc
