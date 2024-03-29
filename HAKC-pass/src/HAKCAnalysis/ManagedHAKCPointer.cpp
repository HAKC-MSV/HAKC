//
// Created by de29664 on 9/18/23.
//

#include "HAKCAnalysis/ManagedHAKCPointer.h"
#include "HAKCAnalysis/HAKCFunctionAnalysis.h"

namespace hakc {
    ManagedHAKCPointerUse::ManagedHAKCPointerUse(ManagedHAKCPointerP &P, User *User, unsigned OperandNo) :
            ManagedPtr(P),
            UserP(User),
            OperandNo(OperandNo) {

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
            CommonHAKCAnalysis::getWriter() << "Trying to set a null user for ";
            UserP->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            throw std::exception();
        }
        UserP = U;
    }

    ManagedHAKCPointerP &ManagedHAKCPointerUse::getManagedPtr() {
        return ManagedPtr;
    }

    raw_ostream &operator<<(raw_ostream &os, const ManagedHAKCPointerUseP &HAKCPointerUse) {
        os << "Argument " << std::to_string(HAKCPointerUse->getOperandNo()) << " of "
           << HAKCPointerUse->getManagedPtr();
        return os;
    }


    ManagedHAKCPointer::ManagedHAKCPointer(Value *Pointer, HAKCPointerManager *Manager, unsigned ID) :
            BaseDefinition(nullptr),
            AuthenticatedPointer(nullptr),
            ProtectedPointer(nullptr),
            DebugActive(Manager->FunctionIsCompartmentalized()),
            Manager(Manager),
            BaseIsAuthenticated(false),
            ManuallyTransferred(false),
            NeedsReplacementUpdates(true),
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

        return Result;
    }

    void ManagedHAKCPointer::InitializeUses() {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Initializing Uses of ";
            BaseDefinition->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }

        if (BaseDefinitionShouldBeTransferred()) {
            for (auto &UPtr: GetAllUses()) {
                if (auto *Call = dyn_cast<CallInst>(UPtr->getUser())) {
                    if (Manager->GetFunctionAnalysis()->IsHAKCTransferFunction(Call->getCalledFunction())) {
                        SetProtectedPointer(Call);
                        if (DebugActive) {
                            CommonHAKCAnalysis::getWriter() << "Found Compartment transfer of ";
                            BaseDefinition->print(CommonHAKCAnalysis::getWriter());
                            CommonHAKCAnalysis::getWriter() << ": ";
                            ProtectedPointer->print(CommonHAKCAnalysis::getWriter());
                            CommonHAKCAnalysis::getWriter() << "\n";
                        }
                        break;
                    }
                }
            }
        } else if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Base Definition ";
            BaseDefinition->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " should not be transferred\n";
        }
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Finished Initializing Uses of ";
            BaseDefinition->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            CommonHAKCAnalysis::getWriter() << "AuthenticatedUses:\n";
            for (auto &P: AuthenticatedUses) {
                CommonHAKCAnalysis::getWriter() << "\t" << P << "\n";
            }
            CommonHAKCAnalysis::getWriter() << "Protected Uses:\n";
            for (auto &P: ProtectedUses) {
                CommonHAKCAnalysis::getWriter() << "\t" << P << "\n";
            }
            CommonHAKCAnalysis::getWriter() << "Clone Uses:\n";
            for (auto &P: CloneUses) {
                CommonHAKCAnalysis::getWriter() << "\t" << P << "\n";
            }
        }
    }

    void ManagedHAKCPointer::InitBaseDefinition(Value *Pointer) {
        BaseDefinition = Pointer;

        if (isa<Argument>(BaseDefinition)) {
            SetProtectedPointer(BaseDefinition);
        }
    }

    void ManagedHAKCPointer::CheckPointerReplacement(Value *Old, Value *New, StringRef TypeName) const {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Setting " << TypeName << "Pointer of " << *this << " to be ";
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
    }

    void ManagedHAKCPointer::SetAuthenticatedPointer(Value *NewAuthenticatedPointer) {
        CheckPointerReplacement(this->AuthenticatedPointer, NewAuthenticatedPointer, "Authenticated");
    }

    void ManagedHAKCPointer::RegisterManualHAKCTransfer(CallBase *CallI) {
        if (!Manager->GetFunctionAnalysis()->IsHAKCTransferFunction(CallI->getCalledFunction())) {
            CallI->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " is not a HAKC Transfer function!\n";
            throw std::exception();
        }
        auto *TransferTypeCast = Manager->GetFunctionAnalysis()->GetTargetTypeCast(dyn_cast<CallInst>(CallI),
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

        SetProtectedPointer(TransferTypeCast);
        ManuallyTransferred = true;
        if (!ProtectedPointer) {
            CommonHAKCAnalysis::getWriter() << "Could not find correct ProtectedPointer Type cast from " << *CallI
                                            << " to Type " << *BaseDefinition->getType() << "\n"
            throw std::exception();
        }
    }

    void ManagedHAKCPointer::AddAuthenticatedUse(ManagedHAKCPointerUseP &UPtr) {
        auto Copy = std::make_shared<ManagedHAKCPointerUse>(UPtr->getManagedPtr(), UPtr->getUser(),
                                                            UPtr->getOperandNo());
        bool found = false;
        for (auto &UseP: AuthenticatedUses) {
            if (UseP == UPtr) {
                found = true;
                break;
            }
        }
        if (!found) {
            AuthenticatedUses.insert(Copy);
        }
        Manager->AddAuthenticatedPointer(Copy, nullptr);
    }

    void ManagedHAKCPointer::AddProtectedUse(ManagedHAKCPointerUseP &UPtr) {
        if (!Manager->FunctionIsCompartmentalized()) {
            return;
        }
        auto Copy = std::make_shared<ManagedHAKCPointerUse>(UPtr->getManagedPtr(), UPtr->getUser(),
                                                            UPtr->getOperandNo());
        ProtectedUses.insert(Copy);
        Manager->AddProtectedPointer(Copy, nullptr);
    }

    void ManagedHAKCPointer::AddCloneUse(ManagedHAKCPointerUseP &UPtr) {
        auto Copy = std::make_shared<ManagedHAKCPointerUse>(UPtr->getManagedPtr(), UPtr->getUser(),
                                                            UPtr->getOperandNo());
        CloneUses.insert(Copy);
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
        if (BaseIsAuthenticated) {
            SetAuthenticatedPointer(BaseDefinition);
            for (auto UPtr: AuthenticatedUses) {
                Manager->AddAuthenticatedPointer(UPtr, UPtr->get());
            }
            for (auto UPtr: CloneUses) {
                Manager->AddAuthenticatedPointer(UPtr, UPtr->get());
            }
        }
        return BaseIsAuthenticated;
    }

    bool ManagedHAKCPointer::ComputeBasePointerAuthenticated() {
        // stack pointers are the "authenticated" pointer
        bool AlreadyAuthenticated = isa<AllocaInst>(BaseDefinition) ||
                                    isa<GlobalVariable>(BaseDefinition);
        if (auto *Call = dyn_cast<CallInst>(BaseDefinition)) {
            if (Call->getCalledFunction()) {
                bool PointerIsTransferred = Manager->GetFunctionAnalysis()->IsHAKCTransferFunction(
                        Call->getCalledFunction());
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
                    AlreadyAuthenticated = Manager->GetFunctionAnalysis()->IsKernelFunction(Call->getCalledFunction()
                    ) || Manager->GetFunctionAnalysis()->IsIntrinsicsNeedingCloning(Call);
                }
            } else if (Call->isInlineAsm()) {
                AlreadyAuthenticated = true;
            }
        }

        if (auto *SelectI = dyn_cast<SelectInst>(BaseDefinition)) {
            if (Manager->ValueWillBeAuthenticated(SelectI->getTrueValue()) &&
                Manager->ValueWillBeAuthenticated(SelectI->getFalseValue())) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "All resultant values of ";
                    SelectI->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << " consists of authenticated pointers, so use it as the "
                                                    << "authenticated pointer\n";
                }
                AlreadyAuthenticated = true;
            }
        } else if (auto *PHI = dyn_cast<PHINode>(BaseDefinition)) {
            bool AllValuesAuthenticated = Manager->GetFunctionAnalysis()->IsPHIOfGlobalsOnly(PHI);
            if (!AllValuesAuthenticated) {
                for (auto &IncomingValue: PHI->incoming_values()) {
                    if (!Manager->ValueWillBeAuthenticated(IncomingValue.get())) {
                        if (DebugActive) {
                            CommonHAKCAnalysis::getWriter() << "Incoming Value ";
                            IncomingValue.get()->print(CommonHAKCAnalysis::getWriter());
                            CommonHAKCAnalysis::getWriter() << " of ";
                            PHI->print(CommonHAKCAnalysis::getWriter());
                            CommonHAKCAnalysis::getWriter() << " is not authenticated\n";
                        }
                        AllValuesAuthenticated = false;
                        break;
                    }
                }
            }
            if (AllValuesAuthenticated) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "All incoming Values of ";
                    PHI->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << " are authenticated, so use it as the authenticated pointer\n";
                }
                AlreadyAuthenticated = true;
            }
        } else if (auto *Load = dyn_cast<LoadInst>(BaseDefinition)) {
            auto *LoadedPointer = Manager->GetDef(Load->getPointerOperand());
            if (isa<AllocaInst>(LoadedPointer)) {
                AlreadyAuthenticated = true;
            }
        } else if (auto *BinOp = dyn_cast<BinaryOperator>(BaseDefinition)) {
            auto *LHS = Manager->GetDef(BinOp->getOperand(0));
            auto *RHS = Manager->GetDef(BinOp->getOperand(1));
            AlreadyAuthenticated = isa<GlobalVariable>(LHS) || isa<GlobalVariable>(RHS);
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Base Definition of " << *this << " is a BinaryOperator that ";
                if (!AlreadyAuthenticated) {
                    CommonHAKCAnalysis::getWriter() << "does not use ";
                } else {
                    CommonHAKCAnalysis::getWriter() << "uses ";
                }
                CommonHAKCAnalysis::getWriter() << "a GlobalVariable\n";
            }
        }

        if (!AlreadyAuthenticated) {
            AlreadyAuthenticated = Manager->GetFunctionAnalysis()->PointerIsAuthenticated_Arch(BaseDefinition);
        }

        return AlreadyAuthenticated;
    }

    void ManagedHAKCPointer::CreateBaseAuthenticatedPointer() {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " called for " << *this << "\n";
        }
        DetermineIfBasePointerIsAuthenticated();

        if (AuthenticatedPointer) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "AuthenticatedPointer already created\n";
            }
            return;
        }

        if (BaseIsAuthenticatedPointer()) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "BaseDefinition of " << *this << " is the authenticated pointer\n";
            }
            SetAuthenticatedPointer(BaseDefinition);
            return;
        }

        if (GetAuthenticatedUserCount() == 0) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "No authenticated pointer uses of " << *this
                                                << ", so creation is not needed\n";
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
            CommonHAKCAnalysis::getWriter() << "Creating Base Authenticated Pointer of " << *this << " with Users\n";
            for (auto *I: Users) {
                I->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
        }
        std::set<Instruction *> UserI;
        for (auto &User: Users) {
            if (auto *I = dyn_cast<Instruction>(User->getUser())) {
                UserI.insert(I);
            }
        }
        auto *AuthenticationInsertPoint = Manager->GetFunctionAnalysis()->FindUseInsertionPoint(BaseDefinition, UserI);
        if (Manager->GetFunctionAnalysis()->isIgnoredType(BaseDefinition->getType()) ||
            isa<CallInst>(BaseDefinition)) {
            /* Pointers from the kernel are handled in BaseIsAuthenticatedPointer above */
            auto *I = Manager->CreateSafePointerAtLocation(BaseDefinition, AuthenticationInsertPoint);
            if (I) {
                SetAuthenticatedPointer(I);
            }
        } else if (auto *PHI = dyn_cast<PHINode>(BaseDefinition)) {
            auto *I = Manager->CreateAuthenticatedValue(PHI);
            if (I) {
                SetAuthenticatedPointer(I);
                for (auto &IncomingUse: PHI->incoming_values()) {
                    auto ManagedPtr = Manager->GetManagedPointer(IncomingUse.get());
                    if (ManagedPtr) {
                        auto ManagedPtrUse = std::make_shared<ManagedHAKCPointerUse>(dyn_cast<User>(I),
                                                                                     IncomingUse.getOperandNo());
                        if (DebugActive) {
                            CommonHAKCAnalysis::getWriter() << "Adding new Authenticated Use from newly created PHI: "
                                                            << ManagedPtrUse << "\n";
                        }
                        ManagedPtr->AddAuthenticatedUse(ManagedPtrUse);
                        ManagedPtr->SetPointerRefreshNeeded(true);
                    }
                }
            }
        } else {
            Value *I;
            if (!Manager->GetFunctionAnalysis()->isCompartmentalizedFunction()) {
                I = Manager->CreateSafePointerAtLocation(BaseDefinition, AuthenticationInsertPoint);
            } else {
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
        std::set<User *> ReplacementsToCreate;
        for (auto &it: CloneUses) {
            ReplacementsToCreate.insert(it->getUser());
        }

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
            for (auto *V: ReplacementsToCreate) {
                CommonHAKCAnalysis::getWriter() << "\t" << *V << "\n";
            }
        }

        for (auto *V: ReplacementsToCreate) {
            Value *AuthenticatedCopy;
            Value *ProtectedCopy;
            if (CreateAuthenticatedCopies) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Creating authenticated copy of " << *V << "\n";
                }

                AuthenticatedCopy = CreateAuthenticatedValue(V);
                if (AuthenticatedCopy) {
                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "Authenticated copy:  " << *AuthenticatedCopy << "\n";
                    }
                    Manager->AddAuthenticatedPointer(V, AuthenticatedCopy);
                    Manager->AddAuthenticatedPointer(AuthenticatedCopy, AuthenticatedCopy);
                } else if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "CreateAuthenticatedValue returned null\n";
                }
            }

            if (CreateProtectedCopies) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Creating Protected copy of " << *V << "\n";
                }

                ProtectedCopy = CreateProtectedValue(V);
                if (ProtectedCopy) {
                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "Protected copy:  " << *ProtectedCopy << "\n";
                    }
                    Manager->AddProtectedPointer(V, ProtectedCopy);
                    Manager->AddProtectedPointer(ProtectedCopy, ProtectedCopy);
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
                CommonHAKCAnalysis::getWriter() << UPtr << ": ";
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
                CommonHAKCAnalysis::getWriter() << UPtr << ": ";
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
        if (!Manager->GetFunctionAnalysis()->isCompartmentalizedFunction()) {
            return false;
        } else if (ManuallyTransferred) {
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
            for (auto &UPtr: ProtectedUses) {
                if (isa<CallInst>(UPtr->getUser())) {
                    return true;
                }
            }
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
        if (BaseDefinitionShouldBeTransferred()) {
            if (GetProtectedUserCount() > 0) {
                TransformUseSet(ProtectedUses);
            } else if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Not transforming Protected Pointer Replacements since user "
                                                   "count is 0\n";
            }
        }
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Function after pointer transformation:\n";
            Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }
    }

    void ManagedHAKCPointer::TransformClones() {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Transforming clones created for " << *this << "\n";
            for (auto &CloneUse: CloneUses) {
                CommonHAKCAnalysis::getWriter() << "\t" << CloneUse << "\n";
            }
        }

        for (auto &CloneUse: CloneUses) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Handling Clone ";
                CloneUse->getUser()->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            if (GetAuthenticatedUserCount() > 0 && !BaseIsAuthenticatedPointer()) {
                auto *AuthenticatedVersion = Manager->FindAuthenticatedValue(CloneUse->getUser());
                if (!AuthenticatedVersion) {
                    CommonHAKCAnalysis::getWriter() << "Unable to find Authenticated Version of "
                                                    << *CloneUse->getUser() << " in\n";
                    Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter(), nullptr);
                    CommonHAKCAnalysis::getWriter() << "\n";
                    throw std::exception();
                }
                if (!Manager->ValueIsAuthenticatedPointer(AuthenticatedVersion)) {
                    auto *AuthenticatedUser = dyn_cast<User>(AuthenticatedVersion);
                    auto *Replacement = Manager->FindAuthenticatedValue(CloneUse->get());
                    if (!Replacement) {
                        CommonHAKCAnalysis::getWriter() << "Unable to find Authenticated replacement of "
                                                        << CloneUse << "\n";
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

                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "Setting Operand " <<
                                                        std::to_string(CloneUse->getOperandNo())
                                                        << " of Authenticated User ";
                        AuthenticatedUser->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << " to be ";
                        Replacement->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << " in function "
                                                        << Manager->GetFunctionAnalysis()->getFunction().getName()
                                                        << "\n";
                    }
                    AuthenticatedUser->setOperand(CloneUse->getOperandNo(), Replacement);
                } else if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "AutheticatedVersion ";
                    AuthenticatedVersion->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << " is a Managed Authenticated Pointer, so no change is needed\n";
                }
            }
            if (GetProtectedUserCount() > 0) {
                auto *ProtectedVersion = Manager->FindProtectedValue(CloneUse->getUser());
                if (!ProtectedVersion) {
                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "No protected value created for ";
                        CloneUse->getUser()->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                    }
                    continue;
                }
                if (!Manager->ValueIsProtectedPointer(ProtectedVersion)) {
                    auto *ProtectedUser = dyn_cast<User>(ProtectedVersion);
                    auto *Replacement = Manager->FindProtectedValue(CloneUse->get());
                    if (!Replacement) {
                        CommonHAKCAnalysis::getWriter() << "Unable to find Protected replacement of "
                                                        << CloneUse << "\n";
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

                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "Setting Operand " <<
                                                        std::to_string(CloneUse->getOperandNo())
                                                        << " of Protected User ";
                        ProtectedUser->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << " to be ";
                        Replacement->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                    }
                    ProtectedUser->setOperand(CloneUse->getOperandNo(), Replacement);
                } else if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "ProtectedVersion ";
                    ProtectedVersion->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << " is a Managed Protected Pointer, so no change is needed\n";
                }
            }
        }
    }

    void
    ManagedHAKCPointer::TransformUseSet(std::set<ManagedHAKCPointerUseP> &UseSet) {
        bool UseAuthenticatedValue = (&UseSet == &AuthenticatedUses);
        StringRef ReplacementSource = UseAuthenticatedValue ? "Authenticated" :
                                      "Protected";

        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Replacing the following operands with "
                                            << ReplacementSource << " values\n";
            for (auto &Use: UseSet) {
                CommonHAKCAnalysis::getWriter() << "\t" << Use << "\n";
            }
        }

        for (auto &Use: UseSet) {
            Value *Replacement;
            if (UseAuthenticatedValue) {
                Replacement = Manager->FindAuthenticatedValue(Use->get());
            } else {
                Replacement = Manager->FindProtectedValue(Use->get());
            }

            if (!Replacement) {
                CommonHAKCAnalysis::getWriter() << "Unable to find " << ReplacementSource << " replacement of "
                                                << Use << "\n";
                Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
                if (UseAuthenticatedValue) {
                    Manager->PrintAuthenticatedValues();
                } else {
                    Manager->PrintProtectedValues();
                }
                throw std::exception();
            }

            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Setting " << Use << " to be ";
                Replacement->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }

            Use->getUser()->setOperand(Use->getOperandNo(), Replacement);
        }
    }

    Value *ManagedHAKCPointer::CreateAuthenticatedValue(Value *Operand) {
        if (Operand == ProtectedPointer) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Operand is Protected Pointer, returning AuthenticatedPointer\n";
            }
            return AuthenticatedPointer;
        } else if (BaseIsAuthenticatedPointer()) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Base is Authenticated Pointer, returning Operand\n";
            }
            return Operand;
        }

        auto Authenticated = Manager->CreateAuthenticatedValue(Operand);
        if (!Authenticated) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "CreateAuthenticatedValue returned null for ";
                Operand->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
        }

        return Authenticated;
    }

    Value *ManagedHAKCPointer::CreateProtectedValue(Value *Operand) {
        if (Operand == AuthenticatedPointer) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Operand is Authenticated Pointer, returning ProtectedPointer\n";
            }
            return ProtectedPointer;
        }

        auto Protected = Manager->CreateProtectedValue(Operand);
        if (!Protected) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "CreateProtectedValue returned null for ";
                Operand->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
        }

        /* No need to check arguments because the creation of a protected pointer
         * means a transfer, and all original uses of BaseDefinition will use ProtectedPointer
         * instead */

        return Protected;
    }

    void ManagedHAKCPointer::MaybeCreateMissingTransfer() {
        if (GetProtectedUserCount() == 0) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "No protected pointer use of " << *this
                                                << ", so creation is not needed\n";
            }
            return;
        }

        if (BaseDefinitionShouldBeTransferred()) {
            if (ProtectedPointer == nullptr) {
                auto *BaseToTransfer = dyn_cast<Instruction>(BaseDefinition);

                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Creating Transfer of BaseDefinition of " << *this << "\n";
                }
                auto *Transfer = Manager->GetFunctionAnalysis()->CreateMissingTransfer(BaseToTransfer);
                SetProtectedPointer(Transfer);
            } else if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Transfer not needed for " << *this
                                                << " because ProtectedPointer is already set to be "
                                                << *ProtectedPointer << "\n"
            }
        } else if (!BaseDefinitionShouldBeTransferred() && ProtectedPointer == nullptr) {
            SetProtectedPointer(BaseDefinition);
        }
    }

    unsigned ManagedHAKCPointer::GetAuthenticatedUserCount() {
        return AuthenticatedUses.size();
    }

    unsigned ManagedHAKCPointer::GetProtectedUserCount() {
        return ProtectedUses.size();
    }

    bool ManagedHAKCPointer::NeedsPointerReplacementRefresh() const {
        return NeedsReplacementUpdates;
    }

    void ManagedHAKCPointer::SetPointerRefreshNeeded(bool RefreshNeeded) {
        NeedsReplacementUpdates = RefreshNeeded;
    }
} // hakc
