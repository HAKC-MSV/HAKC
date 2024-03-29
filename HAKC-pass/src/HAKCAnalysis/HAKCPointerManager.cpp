//
// Created by de29664 on 11/14/23.
//

#include "HAKCAnalysis/HAKCPointerManager.h"
#include "HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "HAKCAnalysis/ManagedHAKCPointer.h"

namespace hakc {
    HAKCPointerManager::HAKCPointerManager(HAKCFunctionAnalysis *Analysis, bool DebugActive) :
            ManagedPointers(),
            AuthenticatedValues(),
            ProtectedValues(),
            HAKCAnalysis(Analysis),
            DataAuthenticationsAdded(0),
            CodeAuthenticationsAdded(0),
            SafePointersAdded(0),
            ClonesAdded(0),
            IsCompartmentalized(false),
            DebugActive(DebugActive),
            CurrentPointerID(0) {
    }

    bool HAKCPointerManager::PointerIsEligibleForManagement(Value *Pointer) {
        /* The HAKCPointerManager::GetDef method performs some analysis to find a definition that could
        * be different from the "true" definition. Use the true definition to check if we are managing
        * constant strings.
        */
        auto *Definition = GetFunctionAnalysis()->getDef(Pointer, false, DebugActive);
        if (isa<ConstantPointerNull>(Definition)) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Pointer Manager ignores null pointers\n";
            }
            return false;
        } else if (isa<ConstantInt>(Pointer)) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Pointer Manager ignores Constant Ints\n";
            }
            return false;
        } else if (!CommonHAKCAnalysis::IsPointerLikeType(Pointer->getType())) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Pointer Manager ignores non-pointers\n";
            }
            return false;
        }

        if (auto *GV = dyn_cast<GlobalVariable>(Definition)) {
            if (CommonHAKCAnalysis::IsStringType(GV->getValueType())) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Pointer Manager is ignoring constant string " << *Definition
                                                    << "\n";
                }
                return false;
            }
        }

        return true;
    }

    void HAKCPointerManager::ManageNewPointer(Value *V) {
        auto *BaseDefinition = GetDef(V);
        if (!BaseDefinition) {
            CommonHAKCAnalysis::getWriter() << "Could not find BaseDefinition for " << *V << "\n";
            throw std::exception();
        }

        auto NextID = CurrentPointerID++;
        if(DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Starting the management of pointer " << std::to_string(NextID) << "\n";
        }

        ManagedHAKCPointerP ManagedPointer = std::make_shared<ManagedHAKCPointer>(BaseDefinition, this,NextID);
        ManagedPointers.insert(ManagedPointer);
        ClassifyAllUsesOfDefinition(V, ManagedPointer);
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Managing " << ManagedPointer << "\n";
        }
    }

    bool HAKCPointerManager::UseIsAnalyzed(ManagedHAKCPointerUseP &UseP) {
        auto Search = [UseP](const ManagedHAKCPointerUseP &UPtr) {
            return UPtr == UseP;
        };

        return std::any_of(AnalyzedUses.begin(), AnalyzedUses.end(), Search);
    }

    bool HAKCPointerManager::UseShouldBeIgnored(Use &U) {
        auto *UserP = U.getUser();
        bool UseShouldBeIgnored = isa<GlobalVariable>(UserP)
                                  || !isa<Instruction>(UserP);
        if (auto *Cmp = dyn_cast<CmpInst>(UserP)) {
            for (auto &Op: Cmp->operands()) {
                if (isa<ConstantPointerNull>(Op.get())) {
                    return true;
                }
            }
        }

        return UseShouldBeIgnored;
    }

    bool HAKCPointerManager::UseShouldBeCloned(Use &U) {
        auto *UserP = U.getUser();
        bool CloneUse = isa<GetElementPtrInst>(UserP) ||
                        isa<BitCastInst>(UserP) ||
                        isa<PtrToIntInst>(UserP) ||
                        isa<SelectInst>(UserP) ||
                        isa<SExtInst>(UserP) ||
                        isa<IntToPtrInst>(UserP) ||
                        isa<PHINode>(UserP) ||
                        isa<FreezeInst>(UserP) ||
                        isa<BinaryOperator>(UserP);

        return CloneUse;
    }

    bool HAKCPointerManager::UseShouldUtilizeAuthenticatedPointer(Use &U) {
        auto *UserP = U.getUser();
        bool UseAuthenticatedPointer = isa<CmpInst>(UserP) ||
                                       isa<LoadInst>(UserP) ||
                                       isa<SubOperator>(UserP) ||
                                       isa<TruncInst>(UserP);
        if (auto *Call = dyn_cast<CallBase>(UserP)) {
            if (
                    GetFunctionAnalysis()->callIsSafeTransition(Call) ||
                    Call->isInlineAsm() ||
                    Call->getCalledOperandUse().getOperandNo() == U.getOperandNo() ||
                    Call->getCalledFunction() == nullptr ||
                    GetFunctionAnalysis()->IsHAKCTransferFunction(Call->getCalledFunction()) ||
                    GetFunctionAnalysis()->IsIntrinsicsNeedingCloning(Call) ||
                    GetFunctionAnalysis()->IsIntrinsicNeedingAuthentication(Call)) {
                UseAuthenticatedPointer = true;
            }
        } else if (isa<StoreInst>(UserP)) {
            if (U.getOperandNo() == StoreInst::getPointerOperandIndex()) {
                UseAuthenticatedPointer = true;
            }
        } else if (isa<AtomicCmpXchgInst>(UserP)) {
            if (U.getOperandNo() == AtomicCmpXchgInst::getPointerOperandIndex()) {
                UseAuthenticatedPointer = true;
            }
        } else if (isa<AtomicRMWInst>(UserP)) {
            if (U.getOperandNo() == AtomicRMWInst::getPointerOperandIndex()) {
                UseAuthenticatedPointer = true;
            }
        }
        return UseAuthenticatedPointer;
    }

    bool HAKCPointerManager::UseShouldUtilizeSignedBasePointer(Use &U) {
        auto *UserP = U.getUser();
        bool UseSignedPointer = isa<AddrSpaceCastOperator>(UserP) ||
                                isa<BitCastOperator>(UserP) ||
                                isa<GEPOperator>(UserP) ||
                                isa<PtrToIntOperator>(UserP) ||
                                isa<ZExtOperator>(UserP) ||
                                isa<ReturnInst>(UserP) ||
                                isa<SwitchInst>(UserP) ||
                                isa<InsertValueInst>(UserP);

        if (isa<StoreInst>(UserP)) {
            if (U.getOperandNo() != StoreInst::getPointerOperandIndex()) {
                UseSignedPointer = true;
            }
        } else if (isa<AtomicCmpXchgInst>(UserP)) {
            if (U.getOperandNo() != AtomicCmpXchgInst::getPointerOperandIndex()) {
                UseSignedPointer = true;
            }
        } else if (auto *Call = dyn_cast<CallInst>(UserP)) {
            if (!GetFunctionAnalysis()->callIsSafeTransition(Call) || Call->getCalledFunction() != nullptr) {
                UseSignedPointer = true;
            } else if (Call->isInlineAsm() ||
                       GetFunctionAnalysis()->IsHAKCTransferFunction(Call->getCalledFunction())) {
                UseSignedPointer = false;
            }
        } else if (isa<AtomicRMWInst>(UserP)) {
            if (U.getOperandNo() != AtomicRMWInst::getPointerOperandIndex()) {
                UseSignedPointer = true;
            }
        }

        return UseSignedPointer;
    }

    bool HAKCPointerManager::IsClonedUseNeedingAdditionalClassification(Use &U) {
        bool NeedsAdditionalClassification = !isa<PHINode>(U.getUser());
        auto ManagedPointer = GetManagedPointer(U.getUser());
        if (ManagedPointer && U.getUser() == ManagedPointer->GetBaseDefinition()) {
            NeedsAdditionalClassification = false;
        }

        return NeedsAdditionalClassification;
    }

    bool HAKCPointerManager::IsAuthenticatedVersionOfItself(Use &U) {
        auto *UserP = U.getUser();
        bool IsAuthenticatedVersion = isa<OverflowingBinaryOperator>(UserP) ||
                                      isa<BinaryOperator>(UserP) || isa<TruncInst>(UserP);
        return IsAuthenticatedVersion;
    }

    void HAKCPointerManager::ClassifyAllUsesOfDefinition(Value *Definition, ManagedHAKCPointerP &ManagedPointer) {
        for (auto &U: Definition->uses()) {
            auto *User = U.getUser();
            auto UPtr = std::make_shared<ManagedHAKCPointerUse>(ManagedPointer, User, U.getOperandNo());
            if (UseIsAnalyzed(UPtr)) {
                continue;
            }
            AnalyzedUses.insert(UPtr);
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Classifying " << UPtr << "\n";
            }
            if (UseShouldBeIgnored(U)) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << UPtr << " is being ignored\n";
                }
                continue;
            }
            if (UseShouldBeCloned(U)) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << *User << " should be cloned\n";
                }
                if (IsClonedUseNeedingAdditionalClassification(U)) {
                    ClassifyAllUsesOfDefinition(User, ManagedPointer);
                }
                ManagedPointer->AddCloneUse(UPtr);
            } else if (UseShouldUtilizeAuthenticatedPointer(U)) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << UPtr << " should use authenticated Base Definition\n";
                }
                ManagedPointer->AddAuthenticatedUse(UPtr);
                if (auto *Call = dyn_cast<CallBase>(User)) {
                    if (GetFunctionAnalysis()->IsHAKCTransferFunction(Call->getCalledFunction())) {
                        ManagedPointer->RegisterManualHAKCTransfer(Call);
                        if (DebugActive) {
                            CommonHAKCAnalysis::getWriter() << "Registered " << *Call << " as the protected pointer of "
                                                            << ManagedPointer << ".  Classifying uses...\n";
                        }
                        ClassifyAllUsesOfDefinition(Call, ManagedPointer);
                    }
                }
                if (IsAuthenticatedVersionOfItself(U)) {
                    AddAuthenticatedPointer(UPtr, UPtr->get());
                }
            } else if (UseShouldUtilizeSignedBasePointer(U)) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << UPtr << " should use signed Base Definition\n";
                }
                ManagedPointer->AddProtectedUse(UPtr);
                AddProtectedPointer(UPtr, UPtr->get());
            } else {
                CommonHAKCAnalysis::getWriter() << "Unexpected use of " << UPtr << " with Managed Pointer "
                                                << ManagedPointer << " in \n";
                GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
                throw std::exception();
            }
        }
    }

    bool HAKCPointerManager::ManagePointer(Value *V) {
        bool result = false;
        if (!PointerIsEligibleForManagement(V)) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Value " << *V << " is not eligible for management\n";
            }
            return result;
        }
        auto ManagedPointer = GetManagedPointer(V);
        if (!ManagedPointer) {
            ManageNewPointer(V);
            result = true;
        } else {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Pointer " << *V << " is already managed: " << ManagedPointer
                                                << "\n";
            }
        }
        return result;
    }

    HAKCFunctionAnalysis *HAKCPointerManager::GetFunctionAnalysis() {
        return HAKCAnalysis;
    }

    std::set<ManagedHAKCPointerP> HAKCPointerManager::GetManagedPointers() {
        return ManagedPointers;
    }

    ManagedHAKCPointerP HAKCPointerManager::GetManagedPointer(Value *V) {
        auto *Def = GetDef(V);
        for (auto &ManagedPointer: ManagedPointers) {
            if (ManagedPointer == Def) {
                return ManagedPointer;
            }
        }

        return nullptr;
    }

    bool HAKCPointerManager::empty() {
        return ManagedPointers.empty();
    }

    Value *HAKCPointerManager::GetDef(Value *V) {
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
                    CommonHAKCAnalysis::getWriter() << "Changing BaseDefinition from " << *BaseDefinition << " to "
                                                    << *NewBaseDefinition << "\n";
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

    Value *HAKCPointerManager::CreateProtectedValue(ManagedHAKCPointerUseP &PointerUse) {
        auto *Pointer = PointerUse->get();
        if (!CloneableManagedPointer(Pointer)) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Called CreateProtectedValue for Uncloneable Pointer " << *Pointer
                                                << "\n";
            }
            return nullptr;
        }

        auto *ProtectedValue = FindProtectedValue(Pointer);
        if (ProtectedValue) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Returning Protected Version " << *ProtectedValue << " for "
                                                << *Pointer << "\n";
            }
            return ProtectedValue;
        }
        auto ManagedPtr = GetManagedPointer(Pointer);
        if (ManagedPtr && ManagedPtr->GetBaseDefinition() == Pointer) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Returning ProtectedPointer\n";
            }
            return ManagedPtr->GetProtectedPointer();
        }

        if (auto *I = dyn_cast<Instruction>(Pointer)) {
            auto Clone = CloneInstruction(I);
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Created Protected Version of " << *I << ": " << *Clone << "\n";
            }
            AddProtectedPointer(PointerUse, Clone);
            return Clone;
        }
        return nullptr;
    }

    Value *HAKCPointerManager::CreateAuthenticatedValue(ManagedHAKCPointerUseP &PointerUse) {
        auto *Pointer = PointerUse->get();
        if (!CloneableManagedPointer(Pointer)) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Called CreateAuthenticatedValue for Uncloneable Pointer "
                                                << *Pointer << "\n";
            }
            return nullptr;
        }

        auto *AuthenticatedCopy = FindAuthenticatedValue(Pointer);
        if (AuthenticatedCopy) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Returning Authenticated Copy " << *AuthenticatedCopy << " for "
                                                << *Pointer << "\n";
            }
            return AuthenticatedCopy;
        }

        if (auto *I = dyn_cast<Instruction>(Pointer)) {
            auto Clone = CloneInstruction(I);
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Created Authenticated Copy of " << *I << ": " << *Clone << "\n";
            }
            AddAuthenticatedPointer(PointerUse, Clone);
            return Clone;
        }
        return nullptr;
    }

    void HAKCPointerManager::CreateAuthenticatedPointersAndAllClones() {
        for (auto &ManagedPtr: GetManagedPointers()) {
            ManagedPtr->CreateBaseAuthenticatedPointer();
            if (DebugActive && ManagedPtr->GetAuthenticatedPointer()) {
                CommonHAKCAnalysis::getWriter() << "Authenticated Pointer for " << ManagedPtr << ": "
                                                << *ManagedPtr->GetAuthenticatedPointer() << "\n";
            }
            ManagedPtr->CreatePointerUseClones();
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Created Authenticated and Protected Copies for "
                                                << ManagedPtr << "\n";
            }
        }
    }

    Value *HAKCPointerManager::FindManagedValue(std::map<ManagedHAKCPointerUseP, Value *> &Storage, Value *Target) {
        for (auto &it: Storage) {
            if (it.first->get() == Target) {
                return it.first->get();
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

    bool
    HAKCPointerManager::ManagedPointerFinder(Value *V, std::function<bool(const ManagedHAKCPointerP &)> const &Search) {
        if (!V) {
            return false;
        }

        return std::any_of(ManagedPointers.begin(), ManagedPointers.end(), Search);
    }

    bool HAKCPointerManager::ValueIsAuthenticatedPointer(Value *V) {
        auto Search = [V](const ManagedHAKCPointerP &Ptr) { return V == Ptr->GetAuthenticatedPointer(); };
        return ManagedPointerFinder(V, Search);
    }

    bool HAKCPointerManager::ValueIsProtectedPointer(Value *V) {
        auto Search = [V](const ManagedHAKCPointerP &Ptr) { return V == Ptr->GetProtectedPointer(); };
        return ManagedPointerFinder(V, Search);
    }

    void
    HAKCPointerManager::AddHAKCPointerReplacement(std::map<ManagedHAKCPointerUseP, Value *> &Storage,
                                                  ManagedHAKCPointerUseP &PtrUse, Value *Replacement) {
        bool CreatingAuthenticatedReplacements = (&Storage == &AuthenticatedValues);
        StringRef StorageName = CreatingAuthenticatedReplacements ? "Authenticated" : "Protected";

        if (!PtrUse) {
            CommonHAKCAnalysis::getWriter() << "Trying to add null " << StorageName << " Pointer Replacement\n";
            throw std::exception();
        }
        if (Storage.find(PtrUse) == Storage.end()) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Adding New " << StorageName << "Pointer Replacement: " << PtrUse
                                                << " -> ";
                if (Replacement) {
                    CommonHAKCAnalysis::getWriter() << *Replacement;
                } else {
                    CommonHAKCAnalysis::getWriter() << "nullptr";
                }
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            Storage[PtrUse] = Replacement;
        } else {
            auto *ExistingPointer = Storage[PtrUse];
            if (ExistingPointer && Replacement && ExistingPointer != Replacement) {
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
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Setting " << StorageName << "Pointer Replacement: " << PtrUse
                                                    << " -> " << *Replacement << "\n";
                }
                Storage[PtrUse] = Replacement;
            } else {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Tried to add null to " << StorageName
                                                    << "Pointer Replacement for " << PtrUse << "\n";
                }
            }
        }
    }

    void HAKCPointerManager::AddAuthenticatedPointer(ManagedHAKCPointerUseP &PointerUse, Value *Replacement) {
        AddHAKCPointerReplacement(AuthenticatedValues, PointerUse, Replacement);
    }

    void HAKCPointerManager::AddProtectedPointer(ManagedHAKCPointerUseP &PointerUse, Value *Replacement) {
        if (!FunctionIsCompartmentalized()) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Protected Pointer is not set for uncompartmentalized functions\n";
            }
            return;
        }
        AddHAKCPointerReplacement(ProtectedValues, PointerUse, Replacement);
    }

    bool HAKCPointerManager::FunctionIsCompartmentalized() const {
        return IsCompartmentalized;
    }

    void HAKCPointerManager::SetFunctionIsCompartmentalized(bool FunctionIsCompartmentalized) {
        IsCompartmentalized = FunctionIsCompartmentalized;
    }

    void HAKCPointerManager::PrintManagedValues(const std::map<ManagedHAKCPointerUseP, Value *> &Storage) {
        for (auto &it: Storage) {
            CommonHAKCAnalysis::getWriter() << it.first << " -> ";
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

    unsigned HAKCPointerManager::GetDataAuthenticationsAdded() const {
        return DataAuthenticationsAdded;
    }

    unsigned HAKCPointerManager::GetCodeAuthenticationsAdded() const {
        return CodeAuthenticationsAdded;
    }

    unsigned HAKCPointerManager::GetSafePointersAdded() const {
        return SafePointersAdded;
    }

    unsigned HAKCPointerManager::GetClonesAdded() const {
        return ClonesAdded;
    }

    unsigned HAKCPointerManager::GetTotalAdditions() const {
        return GetClonesAdded() + GetSafePointersAdded() + GetCodeAuthenticationsAdded() +
               GetDataAuthenticationsAdded();
    }

    void HAKCPointerManager::TransformPointers() {
        for (auto &ManagedPointer: ManagedPointers) {
            ManagedPointer->TransformUses();
        }
    }

    bool HAKCPointerManager::ValueWillBeAuthenticated(Value *V) {
        if (!FunctionIsCompartmentalized()) {
            return true;
        }
        auto ManagedPointer = GetManagedPointer(V);

        return ManagedPointer->BaseIsAuthenticatedPointer() || ManagedPointer->GetAuthenticatedUserCount() > 0;
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
