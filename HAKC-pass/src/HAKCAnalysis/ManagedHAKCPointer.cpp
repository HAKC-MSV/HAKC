//
// Created by de29664 on 9/18/23.
//

#include "HAKCAnalysis/ManagedHAKCPointer.h"

#include <utility>
#include "HAKCAnalysis/HAKCFunctionAnalysis.h"

namespace hakc {
    ManagedHAKCPointerUse::ManagedHAKCPointerUse(ManagedHAKCPointerP P, User *User, unsigned OperandNo, unsigned ID) :
            ManagedPtr(std::move(P)),
            UserP(User),
            OperandNo(OperandNo),
            ID(ID) {

    }

    User *ManagedHAKCPointerUse::getUser() const {
        return UserP;
    }

    unsigned ManagedHAKCPointerUse::getOperandNo() const {
        return OperandNo;
    }

    Value *ManagedHAKCPointerUse::get() const {
        return UserP->getOperand(OperandNo);
    }

    void ManagedHAKCPointerUse::setUser(User *U) {
        if (!U) {
            CommonHAKCAnalysis::getWriter() << "Trying to set a null user for " << *UserP << "\n";
            throw std::exception();
        }
        UserP = U;
    }

    ManagedHAKCPointerP ManagedHAKCPointerUse::getManagedPtr() const {
        return ManagedPtr;
    }

    unsigned ManagedHAKCPointerUse::getID() const {
        return ID;
    }

    void ManagedHAKCPointerUse::SortUses(SmallVector<ManagedHAKCPointerUseP> &ManagedUses) {
        llvm::sort(ManagedUses.begin(), ManagedUses.end(),
                   [](const ManagedHAKCPointerUseP &LHS, const ManagedHAKCPointerUseP &RHS) { return LHS->getID() < RHS->getID(); });
    }

    raw_ostream &operator<<(raw_ostream &os, const ManagedHAKCPointerUse &HAKCPointerUse) {
        os << "Argument " << std::to_string(HAKCPointerUse.getOperandNo()) << " of " << *HAKCPointerUse.getUser()
           << " for " << *HAKCPointerUse.getManagedPtr();
        return os;
    }


    ManagedHAKCPointer::ManagedHAKCPointer(Value *Pointer, HAKCPointerManager *Manager, unsigned ID) :
            BaseDefinition(nullptr),
            AuthenticatedPointer(nullptr),
            ProtectedPointer(nullptr),
            DebugActive(Manager->DebugActive),
            Manager(Manager),
            BaseIsAuthenticated(false),
            ManuallyTransferred(false),
            ID(ID),
            AuthenticatedUses(),
            ProtectedUses(),
            CloneUses() {
        InitBaseDefinition(Pointer);
    }

    unsigned ManagedHAKCPointer::GetID() const {
        return ID;
    }

    std::set<ManagedHAKCPointerUseP> ManagedHAKCPointer::GetAllUses() {
        std::set<ManagedHAKCPointerUseP> Result;
        for (auto &UPtr: AuthenticatedUses) {
            Result.insert(UPtr);
        }
        for (auto &UPtr: ProtectedUses) {
            Result.insert(UPtr);
        }
        for (auto &UPtr: CloneUses) {
            Result.insert(UPtr);
        }

        return Result;
    }

    void ManagedHAKCPointer::InitBaseDefinition(Value *Pointer) {
        BaseDefinition = Pointer;
    }

    void ManagedHAKCPointer::CheckPointerReplacement(Value *Old, Value *New, StringRef TypeName) const {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Setting " << TypeName << " Pointer of " << *this << " to be ";
            if (New) {
                CommonHAKCAnalysis::getWriter() << *New;
            } else {
                CommonHAKCAnalysis::getWriter() << "nullptr";
            }
            CommonHAKCAnalysis::getWriter() << "\n";
        }

        if (Old && Old != New) {
            CommonHAKCAnalysis::getWriter() << "Tried to replace " << TypeName << " Pointer " << *Old << " with ";
            if (New) {
                CommonHAKCAnalysis::getWriter() << *New;
            } else {
                CommonHAKCAnalysis::getWriter() << "nullptr";
            }
            CommonHAKCAnalysis::getWriter() << "\n";
            throw std::exception();
        }
    }

    void ManagedHAKCPointer::SetProtectedPointer(Value *NewProtectedPointer) {
        CheckPointerReplacement(this->ProtectedPointer, NewProtectedPointer, "Protected");
        this->ProtectedPointer = NewProtectedPointer;
        if (this->ProtectedPointer && this->ProtectedPointer == this->AuthenticatedPointer) {
            CommonHAKCAnalysis::getWriter() << "Authenticated and Protected pointers are the same for " << *this
                                            << " in function "
                                            << Manager->GetFunctionAnalysis()->getFunction().getName() << "\n";
            throw std::exception();
        }
    }

    void ManagedHAKCPointer::SetAuthenticatedPointer(Value *NewAuthenticatedPointer) {
        CheckPointerReplacement(this->AuthenticatedPointer, NewAuthenticatedPointer, "Authenticated");
        this->AuthenticatedPointer = NewAuthenticatedPointer;
        if (this->AuthenticatedPointer && this->ProtectedPointer == this->AuthenticatedPointer) {
            CommonHAKCAnalysis::getWriter() << "Authenticated and Protected pointers are the same for " << *this
                                            << " in function "
                                            << Manager->GetFunctionAnalysis()->getFunction().getName() << "\n";
            throw std::exception();
        }
    }

    void ManagedHAKCPointer::RegisterManualHAKCTransfer(CallBase *CallI) {
        if (!Manager->GetFunctionAnalysis()->IsHAKCTransferFunction(CallI->getCalledFunction())) {
            CallI->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " is not a HAKC Transfer function!\n";
            throw std::exception();
        }
        auto *TransferTypeCast = CommonHAKCAnalysis::GetTargetTypeCast(dyn_cast<CallInst>(CallI),
                                                                       BaseDefinition->getType());
        if (ProtectedPointer) {
            if (TransferTypeCast != ProtectedPointer) {
                CommonHAKCAnalysis::getWriter() << "Pointer already has a protected pointer: " << *ProtectedPointer
                                                << "\n";
                throw std::exception();
            }
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << *CallI << " is already registered as the protected pointer of "
                                                << *this << "\n";
            }
            return;
        }

        if (TransferTypeCast) {
            SetProtectedPointer(TransferTypeCast);
        } else {
            SetProtectedPointer(CallI);
        }
        ManuallyTransferred = true;
    }

    void ManagedHAKCPointer::AddAuthenticatedUse(const ManagedHAKCPointerUseP &UPtr) {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << *this << " adding Authenticated Use " << *UPtr << "\n";
        }
        AuthenticatedUses.insert(UPtr);
    }

    void ManagedHAKCPointer::AddProtectedUse(const ManagedHAKCPointerUseP &UPtr) {
        if (!Manager->FunctionIsCompartmentalized()) {
            return;
        }
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << *this << " adding Protected Use " << *UPtr << "\n";
        }
        ProtectedUses.insert(UPtr);
    }

    void ManagedHAKCPointer::AddCloneUse(const ManagedHAKCPointerUseP &UPtr) {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << *this << " adding Clone Use " << *UPtr << "\n";
        }
        CloneUses.insert(UPtr);
    }


    Value *ManagedHAKCPointer::GetBaseDefinition() const {
        return BaseDefinition;
    }

    Value *ManagedHAKCPointer::GetAuthenticatedPointer() {
        return AuthenticatedPointer;
    }

    Value *ManagedHAKCPointer::GetProtectedPointer() {
        return ProtectedPointer;
    }

    bool ManagedHAKCPointer::BaseIsAuthenticatedPointer() const {
        return BaseIsAuthenticated;
    }

    bool ManagedHAKCPointer::DetermineIfBasePointerIsAuthenticated() {
        BaseIsAuthenticated = ComputeBasePointerAuthenticated();
        return BaseIsAuthenticated;
    }

    bool ManagedHAKCPointer::AllIncomingValuesWillBeAuthenticated() {
        bool AllValuesAuthenticated = false;
        if (CommonHAKCAnalysis::IsMultiSSAUser(BaseDefinition)) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Checking incoming values of " << *this
                                                << " for authenticated values\n";
            }
            AllValuesAuthenticated = true;
            std::set<Value *> ValuesToCheck;
            if (auto *PHI = dyn_cast<PHINode>(BaseDefinition)) {
                for (auto &IncomingValue: PHI->incoming_values()) {
                    ValuesToCheck.insert(IncomingValue.get());
                }
            } else if (auto *SelectI = dyn_cast<SelectInst>(BaseDefinition)) {
                ValuesToCheck.insert(SelectI->getTrueValue());
                ValuesToCheck.insert(SelectI->getFalseValue());
            } else if(auto *BinOp = dyn_cast<BinaryOperator>(BaseDefinition)) {
                ValuesToCheck.insert(BinOp->getOperand(0));
                ValuesToCheck.insert(BinOp->getOperand(1));
            } else {
                CommonHAKCAnalysis::getWriter() << "Unexpected MultiSSA User: " << *BaseDefinition << "\n";
                throw std::exception();
            }
            for (auto *ValueToCheck: ValuesToCheck) {
                if (!Manager->ValueWillBeAuthenticated(ValueToCheck)) {
                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "Incoming Value " << *ValueToCheck << " of "
                                                        << *BaseDefinition << " is not authenticated\n";
                    }
                    AllValuesAuthenticated = false;
                    break;
                } else if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Incoming Value " << *ValueToCheck
                                                    << " will be authenticated\n";
                }
            }
        }
        return AllValuesAuthenticated;
    }

    bool ManagedHAKCPointer::ComputeBasePointerAuthenticated() {
        // stack pointers are the "authenticated" pointer
        bool AlreadyAuthenticated = isa<AllocaInst>(BaseDefinition) ||
                                    isa<GlobalVariable>(BaseDefinition);
        if (auto *Call = dyn_cast<CallInst>(BaseDefinition)) {
            if (Call->getCalledFunction()) {
                auto *Callee = Call->getCalledFunction();
                bool PointerIsTransferred = Manager->GetFunctionAnalysis()->IsHAKCTransferFunction(Callee);
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Base Definition is ";
                    if (!PointerIsTransferred) {
                        CommonHAKCAnalysis::getWriter() << "not ";
                    }
                    CommonHAKCAnalysis::getWriter() << "a HAKC Transferred function\n";
                }
                if (PointerIsTransferred) {
                    AlreadyAuthenticated = PointerIsTransferred;
                } else {
                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "IsKernelFunction(" << Callee->getName() << ") = "
                                                        << std::to_string(
                                                                Manager->GetFunctionAnalysis()->IsKernelFunction(
                                                                        Callee)) << "\n";
                        CommonHAKCAnalysis::getWriter() << "IsIntrinsicsNeedingCloning(" << *Call << ") = "
                                                        << std::to_string(
                                                                Manager->GetFunctionAnalysis()->IsIntrinsicsNeedingCloning(
                                                                        Call)) << "\n";
                    }
                    AlreadyAuthenticated = /*Manager->GetFunctionAnalysis()->IsKernelFunction(Callee) ||*/
                            Manager->GetFunctionAnalysis()->IsIntrinsicsNeedingCloning(Call);
                }
            } else if (Call->isInlineAsm()) {
                AlreadyAuthenticated = true;
            }
        } else if (auto *SelectI = dyn_cast<SelectInst>(BaseDefinition)) {
            auto IsTrueValidated = Manager->ValueWillBeAuthenticated(SelectI->getTrueValue());
            auto IsFalseValidated = Manager->ValueWillBeAuthenticated(SelectI->getFalseValue());
            if (IsTrueValidated && IsFalseValidated) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "All resultant values of " << *SelectI
                                                    << " consists of authenticated pointers, so use it as the "
                                                    << "authenticated pointer\n";
                }
                AlreadyAuthenticated = true;
            } else if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Validation results for " << *SelectI << ": IsTrueValidated = "
                                                << std::to_string(IsTrueValidated) << " IsFalseValidated: "
                                                << std::to_string(IsFalseValidated) << "\n";
            }
        } else if (auto *PHI = dyn_cast<PHINode>(BaseDefinition)) {
            bool AllValuesAuthenticated =
                    Manager->GetFunctionAnalysis()->IsPHIOfGlobalsOnly(PHI) || AllIncomingValuesWillBeAuthenticated();
            if (AllValuesAuthenticated /*&& GetProtectedUserCount() == 0*/) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "All incoming Values of " << *PHI
                                                    << " are authenticated, so use it as the authenticated pointer\n";
                }
                AlreadyAuthenticated = true;
            }
        } else if (auto *BinOp = dyn_cast<BinaryOperator>(BaseDefinition)) {
            auto *LHS = Manager->GetDef(BinOp->getOperand(0));
            auto *RHS = Manager->GetDef(BinOp->getOperand(1));
            bool HasGlobal = isa<GlobalVariable>(LHS) || isa<GlobalVariable>(RHS);
            AlreadyAuthenticated = HasGlobal || AllIncomingValuesWillBeAuthenticated();
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Base Definition of " << *this << " is a BinaryOperator that ";
                if (!HasGlobal) {
                    CommonHAKCAnalysis::getWriter() << "does not use ";
                } else {
                    CommonHAKCAnalysis::getWriter() << "uses ";
                }
                CommonHAKCAnalysis::getWriter() << "a GlobalVariable\n";
                if(AlreadyAuthenticated) {
                    CommonHAKCAnalysis::getWriter() << "But both values will be authenticated.\n";
                }
            }
        }

        if (!AlreadyAuthenticated) {
            AlreadyAuthenticated = Manager->GetFunctionAnalysis()->PointerIsAuthenticated_Arch(BaseDefinition);
        }

        return AlreadyAuthenticated;
    }

    void ManagedHAKCPointer::UpdateProtectedMultiValueUses(User *AuthenticatedMultiValueUser,
                                                           User *ProtectedMultiValueUser) {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " called for " << *this << "\n";
        }

        if (AuthenticatedMultiValueUser == GetAuthenticatedPointer()) {
            return;
        }

        auto KeepExistingProtectedUses = !BaseDefinitionShouldBeTransferred();

        SmallVector<ManagedHAKCPointerUseP> CloneUsesToRemove;
        for (auto &CloneUse: CloneUses) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Comparing " << *CloneUse << " with " << *AuthenticatedMultiValueUser
                                                << "\n";
            }
            if (CloneUse->getUser() == AuthenticatedMultiValueUser) {
                auto ProtectedUse = Manager->CreateManagedPointerUse(CloneUse->getManagedPtr(),
                                                                            ProtectedMultiValueUser,
                                                                            CloneUse->getOperandNo());
                AddProtectedUse(ProtectedUse);
                AddAuthenticatedUse(CloneUse);
                CloneUsesToRemove.push_back(CloneUse);
            }
        }

        if (!CloneUsesToRemove.empty()) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Protected PHI Use was added to " << *this << "\n";
            }

            ManagedHAKCPointerUse::SortUses(CloneUsesToRemove);
            for (auto &CloneUse: CloneUsesToRemove) {
                CloneUses.erase(CloneUse);
                Manager->RemoveProtectedUse(CloneUse);
            }
            if(KeepExistingProtectedUses) {
                SmallVector<ManagedHAKCPointerUseP> SortedUses(ProtectedUses.begin(), ProtectedUses.end());
                ManagedHAKCPointerUse::SortUses(SortedUses);
                for (const auto &UPtr: SortedUses) {
                    Manager->AddProtectedPointer(UPtr, UPtr->get());
                }
            }

            MaybeCreateMissingTransfer();
        }
    }

    void ManagedHAKCPointer::CreateBaseAuthenticatedPointer() {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " called for " << *this << "\n";
        }
        DetermineIfBasePointerIsAuthenticated();
        if (GetAuthenticatedUserCount() == 0) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "No authenticated pointer uses of " << *this
                                                << ", so authenticated pointer creation is not needed\n";
            }
            return;
        }

        if (BaseIsAuthenticatedPointer()) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter()
                        << "The Base Definition is authenticated, so setting uses to be authenticated\n";
            }
            if (AllIncomingValuesWillBeAuthenticated()) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "All incoming values of " << *this
                                                    << " will be authenticated, so use authenticated values\n";
                }
                auto *MultivalueUser = dyn_cast<User>(BaseDefinition);
                SetAuthenticatedPointer(MultivalueUser);
                if (GetProtectedUserCount() > 0) {
                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "A protected version of " << *BaseDefinition
                                                        << " is needed\n";
                    }
                    auto *MultivalueUserCopy = Manager->CloneInstruction(dyn_cast<Instruction>(MultivalueUser));
                    SetProtectedPointer(MultivalueUserCopy);
                    Manager->UpdateProtectedMultiUsers(MultivalueUser, MultivalueUserCopy);
                }
            } else {
                SetAuthenticatedPointer(BaseDefinition);
            }
            SmallVector<ManagedHAKCPointerUseP> SortedUses(AuthenticatedUses.begin(), AuthenticatedUses.end());
            SortedUses.append(CloneUses.begin(), CloneUses.end());
            ManagedHAKCPointerUse::SortUses(SortedUses);
            for (const auto &UPtr: SortedUses) {
                Manager->AddAuthenticatedPointer(UPtr, UPtr->get());
            }
            return;
        } else {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter()
                        << "The Base Definition of " << *this << " is protected\n";
            }
            if (!BaseDefinitionShouldBeTransferred()) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter()
                            << "The Base Definition should not be transferred,  so setting uses "
                            << "to be protected\n";
                }
                SetProtectedPointer(BaseDefinition);
                SmallVector<ManagedHAKCPointerUseP> SortedUses(ProtectedUses.begin(), ProtectedUses.end());
                SortedUses.append(CloneUses.begin(), CloneUses.end());
                ManagedHAKCPointerUse::SortUses(SortedUses);
                for (const auto &UPtr: SortedUses) {
                    Manager->AddProtectedPointer(UPtr, UPtr->get());
                }
            } else if (DebugActive) {
                CommonHAKCAnalysis::getWriter()
                        << "The Base Definition Should be transferred, so no use is set to be protected\n";
            }
        }

        if (AuthenticatedPointer) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "AuthenticatedPointer already created\n";
            }
            return;
        }

        auto Users = GetAllUses();
        if (Users.empty()) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "User count of " << *this
                                                << " is 0. No Authenticated Pointer needed\n";
            }
            return;
        }

        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Creating Base Authenticated Pointer of " << *this
                                            << " with Users\n";
            for (auto &User: Users) {
                CommonHAKCAnalysis::getWriter() << *User << "\n";
            }
        }
        std::set<Instruction *> UserI;
        for (auto &User: Users) {
            if (auto *I = dyn_cast<Instruction>(User->getUser())) {
                UserI.insert(I);
            }
        }
        auto *AuthenticationInsertPoint = Manager->GetFunctionAnalysis()->FindUseInsertionPoint(BaseDefinition,
                                                                                                UserI);
        if (Manager->GetFunctionAnalysis()->isIgnoredType(BaseDefinition->getType())) {
            auto *I = Manager->CreateSafePointerAtLocation(BaseDefinition, AuthenticationInsertPoint);
            if (I) {
                SetAuthenticatedPointer(I);
            }
        } else {
            Value *I = nullptr;
            if (!Manager->GetFunctionAnalysis()->isCompartmentalizedFunction() || isa<CallBase>(BaseDefinition)) {
                I = Manager->CreateSafePointerAtLocation(BaseDefinition, AuthenticationInsertPoint);
            }

            if (!I) {
                I = Manager->CreateAuthenticationAtLocation(BaseDefinition, AuthenticationInsertPoint);
            }
            if (I) {
                SetAuthenticatedPointer(I);
            }
        }

        if (!AuthenticatedPointer) {
            CommonHAKCAnalysis::getWriter() << "Failed to create authenticated pointer for " << *this
                                            << " in Function\n";
            Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            throw std::exception();
        }
    }

    void ManagedHAKCPointer::CreatePointerReplacements() {
        bool CreateAuthenticatedCopies = GetAuthenticatedUserCount() > 0;
        bool CreateProtectedCopies = GetProtectedUserCount() > 0;

        if (DebugActive && !CreateAuthenticatedCopies) {
            CommonHAKCAnalysis::getWriter() << "No Authenticated Users of " << *this
                                            << " so no authenticated clones will be created\n";
        }

        if (DebugActive && !CreateProtectedCopies) {
            CommonHAKCAnalysis::getWriter() << "No Protected Users of " << *this
                                            << " so no protected clones will be created\n";
        }

        if (!CreateAuthenticatedCopies && !CreateProtectedCopies) {
            return;
        }

        CreateAuthenticatedCopies = !BaseIsAuthenticatedPointer();
        if (DebugActive && !CreateAuthenticatedCopies) {
            CommonHAKCAnalysis::getWriter() << "Base is authenticated pointer, so existing uses are authenticated\n";
        }

        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "\n\nCreating Clones of uses of " << *this << ":\n";
            for (auto &Use: CloneUses) {
                CommonHAKCAnalysis::getWriter() << "\t" << *Use << "\n";
            }
        }

        for (const auto &Use: GetAllUses()) {
            Value *AuthenticatedCopy;
            Value *ProtectedCopy;
            if (CreateAuthenticatedCopies && ProtectedUses.find(Use) == ProtectedUses.end()) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Creating authenticated copy of " << *Use << "\n";
                }

                AuthenticatedCopy = CreateAuthenticatedValue(Use);
                if (AuthenticatedCopy) {
                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "Authenticated copy:  " << *AuthenticatedCopy << "\n";
                    }
                } else if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "CreateAuthenticatedValue returned null\n";
                }
            }

            if (CreateProtectedCopies && AuthenticatedUses.find(Use) == AuthenticatedUses.end()) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Creating Protected copy of " << *Use << "\n";
                }

                ProtectedCopy = CreateProtectedValue(Use);
                if (ProtectedCopy) {
                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "Protected copy:  " << *ProtectedCopy << "\n";
                    }
                } else if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "CreateProtectedValue returned null\n";
                }
            }
        }
    }

    void ManagedHAKCPointer::CreatePointerUseClones() {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " called for " << *this << "\n";
        }

        CreatePointerReplacements();

        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "\n";
            Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\nAuthenticatedUses:\n";
            for (auto &UPtr: AuthenticatedUses) {
                auto *Replacement = Manager->FindAuthenticatedValue(UPtr->get());
                CommonHAKCAnalysis::getWriter() << *UPtr << ": ";
                if (Replacement) {
                    Replacement->print(CommonHAKCAnalysis::getWriter());
                } else {
                    CommonHAKCAnalysis::getWriter() << "nullptr";
                }
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            CommonHAKCAnalysis::getWriter() << "\n\nProtectedUses:\n";
            for (auto &UPtr: ProtectedUses) {
                auto *Replacement = Manager->FindAuthenticatedValue(UPtr->get());
                CommonHAKCAnalysis::getWriter() << *UPtr << ": ";
                if (Replacement) {
                    Replacement->print(CommonHAKCAnalysis::getWriter());
                } else {
                    CommonHAKCAnalysis::getWriter() << "nullptr";
                }
                CommonHAKCAnalysis::getWriter() << "\n";
            }
        }
    }

    bool ManagedHAKCPointer::BaseDefinitionShouldBeTransferred() {
        if (!Manager->GetFunctionAnalysis()->isCompartmentalizedFunction() || ManuallyTransferred) {
            return false;
        }

        if (auto *Call = dyn_cast<CallInst>(BaseDefinition)) {
            if (!Call->getCalledFunction()) {
                return true;
            }
            auto *Callee = Call->getCalledFunction();
            return Manager->GetFunctionAnalysis()->IsKernelAllocation(BaseDefinition) ||
                   !Manager->GetFunctionAnalysis()->FunctionsAreInSameCompartment(
                           &Manager->GetFunctionAnalysis()->getFunction(), Callee);
        } else if (isa<AllocaInst>(BaseDefinition)) {
            return GetProtectedUserCount() > 0;
//            for (auto &UPtr: ProtectedUses) {
//                if (isa<CallInst>(UPtr->getUser()) ||
//                    (isa<StoreInst>(UPtr->getUser()) && UPtr->getOperandNo() != StoreInst::getPointerOperandIndex())) {
//                    return true;
//                }
//            }
        }

        return false;
    }

    void ManagedHAKCPointer::TransformUses() {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " called for " << *this << "\n";
        }

        TransformClones();

        if (GetAuthenticatedUserCount() > 0) {
            TransformUseSet(AuthenticatedUses);
        } else if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Not transforming Authenticated Pointer Replacements since user count "
                                               "of " << *this << " is 0\n";
        }
        if (GetProtectedUserCount() > 0) {
            TransformUseSet(ProtectedUses);
        } else if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Not transforming Protected Pointer Replacements since user "
                                               "count is 0\n";
        }
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Function after pointer transformation:\n";
            Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }
    }

    void ManagedHAKCPointer::SetUseOperand(User *U, Value *Replacement, const ManagedHAKCPointerUseP &PointerUse,
                                           bool IsAuthenticatedUse) {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Setting Operand " <<
                                            std::to_string(PointerUse->getOperandNo()) << " of ";
            if (IsAuthenticatedUse) {
                CommonHAKCAnalysis::getWriter() << "Authenticated";
            } else {
                CommonHAKCAnalysis::getWriter() << "Protected";
            }
            CommonHAKCAnalysis::getWriter() << " User " << *U << " to be " << *Replacement << " in function "
                                            << Manager->GetFunctionAnalysis()->getFunction().getName() << " for "
                                            << *this << "\n";
        }

        if (PointerUse->getUser()->getValueID() != U->getValueID()) {
            if (CommonHAKCAnalysis::IsMultiSSAUser(PointerUse->getUser())) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Not changing operand of MultiSSA User\n";
                }
                return;
            }
            CommonHAKCAnalysis::getWriter() << "Invalid PointerUse " << *PointerUse << " for User "
                                            << *U << " of " << *this << " in function\n";
            Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter(), nullptr);
            CommonHAKCAnalysis::getWriter() << "\n";
            throw std::exception();
        }

        U->setOperand(PointerUse->getOperandNo(), Replacement);
    }

    void ManagedHAKCPointer::TransformClones() {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Transforming clones created for " << *this << "\n";
            for (auto &CloneUse: CloneUses) {
                CommonHAKCAnalysis::getWriter() << "\t" << *CloneUse << "\n";
            }
        }

        SmallVector<ManagedHAKCPointerUseP> SortedUses(CloneUses.begin(), CloneUses.end());
        ManagedHAKCPointerUse::SortUses(SortedUses);
        for (const auto &CloneUse: SortedUses) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Handling Clone " << *CloneUse->getUser() << "\n";
            }
            if (GetAuthenticatedUserCount() > 0) {
                auto *AuthenticatedVersion = Manager->FindAuthenticatedValue(CloneUse->getUser());
                if (AuthenticatedVersion) {
                    auto *AuthenticatedUser = dyn_cast<User>(AuthenticatedVersion);
                    auto *Replacement = Manager->FindAuthenticatedValue(CloneUse);
                    if (!Replacement) {
                        CommonHAKCAnalysis::getWriter() << "Unable to find Authenticated replacement of "
                                                        << *CloneUse << "\n";
                        Manager->PrintAuthenticatedValues();
                        CommonHAKCAnalysis::getWriter() << "\n";
                        Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                        throw std::exception();
                    }
                    if (!AuthenticatedUser) {
                        CommonHAKCAnalysis::getWriter() << "AuthenticatedVersion is not a User: ";
                        AuthenticatedVersion->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                        Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                        throw std::exception();
                    }
                    SetUseOperand(AuthenticatedUser, Replacement, CloneUse, true);
                } else if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Unable to find Authenticated Version of "
                                                    << *CloneUse->getUser() << " for " << *this << " in\n";
                    Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter(), nullptr);
                    CommonHAKCAnalysis::getWriter() << "\n";
                }
            }
            if (GetProtectedUserCount() > 0) {
                auto *ProtectedVersion = Manager->FindProtectedValue(CloneUse->getUser());
                if (!ProtectedVersion) {
                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "No protected value created for " << *CloneUse << "\n";
                    }
                    continue;
                }
                auto *ProtectedUser = dyn_cast<User>(ProtectedVersion);
                auto *Replacement = Manager->FindProtectedValue(CloneUse);
                if (!Replacement) {
                    CommonHAKCAnalysis::getWriter() << "Unable to find Protected replacement of "
                                                    << *CloneUse << "\n";
                    Manager->PrintProtectedValues();
                    CommonHAKCAnalysis::getWriter() << "\n";
                    Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << "\n";
                    throw std::exception();
                }
                if (!ProtectedUser) {
                    CommonHAKCAnalysis::getWriter() << "ProtectedVersion is not a User: ";
                    ProtectedVersion->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << "\n";
                    Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << "\n";
                    throw std::exception();
                }

                SetUseOperand(ProtectedUser, Replacement, CloneUse, false);
            }
        }
    }

    void
    ManagedHAKCPointer::TransformUseSet(std::set<ManagedHAKCPointerUseP> &UseSet) {
        bool UseAuthenticatedValue = (&UseSet == &AuthenticatedUses);
        StringRef ReplacementSource = UseAuthenticatedValue ? "Authenticated" :
                                      "Protected";

        SmallVector<ManagedHAKCPointerUseP> SortedUses(UseSet.begin(), UseSet.end());
        ManagedHAKCPointerUse::SortUses(SortedUses);
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Replacing the following operands with "
                                            << ReplacementSource << " values\n";
            for (auto &Use: SortedUses) {
                CommonHAKCAnalysis::getWriter() << "\t" << *Use << "\n";
            }
        }

        for (const auto &Use: SortedUses) {
            Value *Replacement;
            if (UseAuthenticatedValue) {
                Replacement = Manager->FindAuthenticatedValue(Use->get());
            } else {
                Replacement = Manager->FindProtectedValue(Use->get());
            }

            if (!Replacement) {
                CommonHAKCAnalysis::getWriter() << "Unable to find " << ReplacementSource << " replacement of "
                                                << *Use << "\n";
                Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
                if (UseAuthenticatedValue) {
                    Manager->PrintAuthenticatedValues();
                } else {
                    Manager->PrintProtectedValues();
                }
                throw std::exception();
            }

            SetUseOperand(Use->getUser(), Replacement, Use, UseAuthenticatedValue);
        }
    }

    Value *ManagedHAKCPointer::CreateAuthenticatedValue(const ManagedHAKCPointerUseP &HAKCUse) {
        auto Authenticated = Manager->CreateAuthenticatedValue(HAKCUse);
        if (!Authenticated) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "CreateAuthenticatedValue returned null for " << *HAKCUse << "\n";
            }
        } else {
            auto *Pointer = HAKCUse->get();
            SmallVector<ManagedHAKCPointerUseP> SortedUses(AuthenticatedUses.begin(), AuthenticatedUses.end());
            SortedUses.append(CloneUses.begin(), CloneUses.end());
            ManagedHAKCPointerUse::SortUses(SortedUses);

            for (const auto &PointerUse: SortedUses) {
                if (PointerUse->get() == Pointer) {
                    Manager->AddAuthenticatedPointer(PointerUse, Authenticated);
                }
            }
        }

        return Authenticated;
    }

    Value *ManagedHAKCPointer::CreateProtectedValue(const ManagedHAKCPointerUseP &HAKCUse) {
        auto Protected = Manager->CreateProtectedValue(HAKCUse);
        if (!Protected) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "CreateProtectedValue returned null for " << *HAKCUse << "\n";
            }
        } else {
            auto *Pointer = HAKCUse->get();
            SmallVector<ManagedHAKCPointerUseP> SortedUses(ProtectedUses.begin(), ProtectedUses.end());
            SortedUses.append(CloneUses.begin(), CloneUses.end());
            ManagedHAKCPointerUse::SortUses(SortedUses);
            for (const auto &PointerUse: SortedUses) {
                if (PointerUse->get() == Pointer) {
                    Manager->AddProtectedPointer(PointerUse, Protected);
                }
            }
        }

        return Protected;
    }

    void ManagedHAKCPointer::MaybeCreateMissingTransfer() {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " called for " << *this << "\n";
        }

        if (GetProtectedUserCount() == 0) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "No protected pointer use of " << *this
                                                << ", so transfer creation is not needed\n";
            }
            return;
        }

        auto BaseShouldBeTransferred = BaseDefinitionShouldBeTransferred();
        if (BaseShouldBeTransferred) {
            if (ProtectedPointer == nullptr) {
                auto *BaseToTransfer = dyn_cast<Instruction>(BaseDefinition);

                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Creating Transfer of BaseDefinition of " << *this << "\n";
                }
                auto *Transfer = Manager->GetFunctionAnalysis()->CreateMissingTransfer(BaseToTransfer);
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Created Transfer " << *Transfer << "\n";
                }
                SetProtectedPointer(Transfer);
                SmallVector<ManagedHAKCPointerUseP> SortedUses(ProtectedUses.begin(), ProtectedUses.end());
                SortedUses.append(CloneUses.begin(), CloneUses.end());
                ManagedHAKCPointerUse::SortUses(SortedUses);
                for (const auto &ProtectedUse: SortedUses) {
                    if (ProtectedUse->get() == BaseToTransfer) {
                        Manager->AddProtectedPointer(ProtectedUse, Transfer);
                    }
                }
            } else if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Transfer not needed for " << *this
                                                << " because ProtectedPointer is already set to be "
                                                << *ProtectedPointer << "\n";
            }
        } else if (!ComputeBasePointerAuthenticated() && !ManuallyTransferred) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << *this << " base should not be transferred\n";
            }
            SetProtectedPointer(BaseDefinition);
        }
    }

    unsigned ManagedHAKCPointer::GetAuthenticatedUserCount() {
        return AuthenticatedUses.size();
    }

    unsigned ManagedHAKCPointer::GetProtectedUserCount() {
        return ProtectedUses.size();
    }
} // hakc
