//
// Created by de29664 on 9/18/23.
//

#include "HAKCAnalysis/ManagedHAKCPointer.h"
#include "HAKCAnalysis/HAKCFunctionAnalysis.h"

namespace hakc {
    ManagedHAKCPointerUse::ManagedHAKCPointerUse(User *UserP, unsigned int OperandNo) : UserP(UserP),
                                                                                        OperandNo(OperandNo),
                                                                                        AuthenticatedUse(false),
                                                                                        ProtectedUse(false) {

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

    bool ManagedHAKCPointerUse::ForAuthenticatedUse() const {
        return AuthenticatedUse;
    }

    bool ManagedHAKCPointerUse::ForProtectedUse() const {
        return ProtectedUse;
    }

    void ManagedHAKCPointerUse::SetForAuthenticatedUse(bool ForAuthUse) {
        this->AuthenticatedUse = ForAuthUse;
    }

    void ManagedHAKCPointerUse::SetForProtectedUse(bool ForProtUse) {
        this->ProtectedUse = ForProtUse;
    }


    ManagedHAKCPointer::ManagedHAKCPointer(Value *Pointer, HAKCPointerManager *Manager, bool debug) :
            BaseDefinition(nullptr),
            AuthenticatedPointer(nullptr),
            ProtectedPointer(nullptr),
            DebugActive(debug),
            Manager(Manager),
            AuthenticatedPointerReplacements(),
            ProtectedPointerReplacements() {
        InitBaseDefinition(Pointer);
    }

    void ManagedHAKCPointer::InitializeUses() {
        ClassifyAllUsesOfBaseDefinition(BaseDefinition);
        if (BaseDefinitionShouldBeTransferred()) {
            for (auto &it: ProtectedPointerReplacements) {
                if (auto *Call = dyn_cast<CallInst>(it.first->getUser())) {
                    if (Manager->GetFunctionAnalysis()->IsHAKCTransferFunction(Call->getCalledFunction())) {
                        ProtectedPointer = Call;
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
        }
    }

    void ManagedHAKCPointer::InitBaseDefinition(Value *Pointer) {
        auto *V = Manager->GetFunctionAnalysis()->getDef(Pointer, false, DebugActive);
        if (auto *Alloca = dyn_cast<AllocaInst>(V)) {
            BaseDefinition = Manager->GetFunctionAnalysis()->GetFinalAllocaDef(Alloca);
        } else {
            BaseDefinition = V;
        }
        if (!BaseDefinition) {
            CommonHAKCAnalysis::getWriter() << "ManagedHAKCPointer could not find BaseDefinition for ";
            Pointer->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            throw std::exception();
        }

        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "ManagedHAKCPointer found BaseDefinition: ";
            BaseDefinition->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }
    }

    bool ManagedHAKCPointer::UseShouldBeCloned(Use &U) {
        auto *UserP = U.getUser();
        bool CloneUse = isa<GetElementPtrInst>(UserP) ||
                        isa<BitCastInst>(UserP) ||
                        isa<PtrToIntInst>(UserP) ||
                        isa<SelectInst>(UserP) ||
                        isa<SExtInst>(UserP) ||
                        isa<IntToPtrInst>(UserP) ||
                        isa<PHINode>(UserP);
        return CloneUse;
    }

    bool ManagedHAKCPointer::UseShouldUtilizeAuthenticatedPointer(Use &U) {
        auto *UserP = U.getUser();
        bool UseAuthenticatedPointer = isa<CmpInst>(UserP) ||
                                       isa<LoadInst>(UserP) ||
                                       isa<SubOperator>(UserP) ||
                                       isa<BinaryOperator>(UserP) ||
                                       isa<TruncInst>(UserP);
        if (auto *Call = dyn_cast<CallInst>(UserP)) {
            if (Manager->GetFunctionAnalysis()->callIsSafeTransition(Call)) {
                UseAuthenticatedPointer = true;
            } else if (Call->isInlineAsm()) {
                UseAuthenticatedPointer = true;
            } else if (Manager->GetFunctionAnalysis()->isIntrinsicNeedingAuthentication(Call)) {
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
        }
        return UseAuthenticatedPointer;
    }

    bool ManagedHAKCPointer::UseShouldUtilizeSignedBasePointer(Use &U) {
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
            if (!Manager->GetFunctionAnalysis()->callIsSafeTransition(Call)) {
                UseSignedPointer = true;
            } else if (Call->isInlineAsm()) {
                UseSignedPointer = false;
            }
        }

        return UseSignedPointer;
    }

    bool ManagedHAKCPointer::UseShouldBeIgnored(Use &U) {
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

    void ManagedHAKCPointer::ClassifyAllUsesOfBaseDefinition(Value *Def) {

        for (auto &U: Def->uses()) {
            auto *User = U.getUser();
            auto UPtr = std::make_shared<ManagedHAKCPointerUse>(User, U.getOperandNo());
            if (UseShouldBeIgnored(U)) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << UPtr << " is being ignored\n";
                }
                continue;
            }
            if (UseShouldBeCloned(U)) {
                if (!UseIsAnalyzed(UPtr)) {
                    if (DebugActive) {
                        User->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << " should be cloned\n";
                    }
                    AuthenticatedPointerReplacements[UPtr] = nullptr;
                    ProtectedPointerReplacements[UPtr] = nullptr;
                    ClassifyAllUsesOfBaseDefinition(User);
                }
            } else if (UseShouldUtilizeAuthenticatedPointer(U)) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << UPtr << " should use authenticated Base Definition\n";
                }
                UPtr->SetForAuthenticatedUse(true);
                AuthenticatedPointerReplacements[UPtr] = nullptr;
            } else if (UseShouldUtilizeSignedBasePointer(U)) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << UPtr << " should use signed Base Definition\n";
                }
                UPtr->SetForProtectedUse(true);
                ProtectedPointerReplacements[UPtr] = nullptr;
            } else {
                CommonHAKCAnalysis::getWriter() << "Unexpected use of " << UPtr << " with Base Definition ";
                BaseDefinition->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " in \n";
                Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
                throw std::exception();
            }
        }
    }

    Value *ManagedHAKCPointer::GetBaseDefinition() const {
        return BaseDefinition;
    }

    bool ManagedHAKCPointer::UseIsAnalyzed(ManagedHAKCPointerUseP &UseP) {
        for (auto &it: AuthenticatedPointerReplacements) {
            if (it.first->getUser() == UseP->getUser() && it.first->getOperandNo() == UseP->getOperandNo()) {
                return true;
            }
        }

        return false;
    }

    Value *ManagedHAKCPointer::GetAuthenticatedPointer() {
        return AuthenticatedPointer;
    }

    Value *ManagedHAKCPointer::GetProtectedPointer() {
        return ProtectedPointer;
    }

    bool ManagedHAKCPointer::BaseIsAuthenticatedPointer() {
        // Pointers from the kernel are trusted, and stack pointers are the "authenticated" pointer
        bool BaseIsAuthenticated = BaseDefinitionIsFromKernel() || isa<AllocaInst>(BaseDefinition) ||
                                   isa<GlobalVariable>(BaseDefinition);

        if (auto *SelectI = dyn_cast<SelectInst>(BaseDefinition)) {
            if (Manager->ValueWillBeAuthenticated(SelectI->getTrueValue()) &&
                Manager->ValueWillBeAuthenticated(SelectI->getFalseValue())) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "All resultant values of ";
                    SelectI->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << " consists of authenticated pointers, so use it as the "
                                                    << "authenticated pointer\n";
                }
                BaseIsAuthenticated = true;
            }
        } else if (auto *PHI = dyn_cast<PHINode>(BaseDefinition)) {
            bool AllValuesAuthenticated = true;
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
            if (AllValuesAuthenticated) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "All incoming Values of ";
                    PHI->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << " are authenticated, so use it as the authenticated pointer\n";
                }
                BaseIsAuthenticated = true;
            }
        } else if (auto *Load = dyn_cast<LoadInst>(BaseDefinition)) {
            auto *LoadedPointer = Manager->GetDef(Load->getPointerOperand());
            if (isa<AllocaInst>(LoadedPointer)) {
                BaseIsAuthenticated = true;
            }
        }

        return BaseIsAuthenticated;
    }

    std::set<Instruction *> ManagedHAKCPointer::GetBaseDefinitionUsers() {
        std::set<Instruction *> Users;
        for (auto &U: BaseDefinition->uses()) {
            if (!UseShouldBeIgnored(U)) {
                if (auto *I = dyn_cast<Instruction>(U.getUser())) {
                    Users.insert(I);
                }
            }
        }
        return Users;
    }

    unsigned ManagedHAKCPointer::GetBaseDefinitionUserCount() {
        auto BaseDefinitionUsers = GetBaseDefinitionUsers();
        return BaseDefinitionUsers.size();
    }

    void ManagedHAKCPointer::CreateBaseAuthenticatedPointer() {
        if (BaseIsAuthenticatedPointer()) {
            AuthenticatedPointer = BaseDefinition;
            return;
        }

        auto Users = GetBaseDefinitionUsers();
        if (Users.empty()) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "User count of ";
                BaseDefinition->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " is 0. No Authenticated Pointer needed\n";
            }
            return;
        }

        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Creating Base Authenticated Pointer of ";
            BaseDefinition->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " with Users\n";
            for (auto *I: Users) {
                I->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
        }
        auto *AuthenticationInsertPoint = Manager->GetFunctionAnalysis()->FindUseInsertionPoint(BaseDefinition,
                                                                                                Users);
        if (Manager->GetFunctionAnalysis()->isCompartmentalizedFunction()) {
            if (Manager->GetFunctionAnalysis()->isIgnoredType(BaseDefinition->getType())) {
                AuthenticatedPointer = dyn_cast<Instruction>(
                        Manager->GetFunctionAnalysis()->AddSafePointerCreationAtLocation
                                (BaseDefinition, AuthenticationInsertPoint));
            } else if (auto *PHI = dyn_cast<PHINode>(BaseDefinition)) {
                AuthenticatedPointer = Manager->CreateAuthenticatedInstruction(PHI, DebugActive);
            } else if (isa<CallInst>(BaseDefinition)) {
                /* Pointers from the kernel are handled in BaseIsAuthenticatedPointer above */
                AuthenticatedPointer = dyn_cast<Instruction>(
                        Manager->GetFunctionAnalysis()->AddSafePointerCreationAtLocation
                                (BaseDefinition, AuthenticationInsertPoint));
            } else {
                AuthenticatedPointer = dyn_cast<Instruction>(
                        Manager->GetFunctionAnalysis()->addDataAuthCheckAtLocation(BaseDefinition,
                                                                                   AuthenticationInsertPoint));
            }
        } else {
            AuthenticatedPointer = dyn_cast<Instruction>(
                    Manager->GetFunctionAnalysis()->AddSafePointerCreationAtLocation
                            (BaseDefinition, AuthenticationInsertPoint));
        }

        if (!AuthenticatedPointer) {
            CommonHAKCAnalysis::getWriter() << "Failed to create authenticated pointer for ";
            BaseDefinition->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " in Function\n";
            Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            throw std::exception();
        }
    }

    void ManagedHAKCPointer::CreatePointerReplacements(std::map<ManagedHAKCPointerUseP, Value *> &ReplacementStorage) {
        bool CreatingAuthenticatedReplacements = (&ReplacementStorage == &AuthenticatedPointerReplacements);
        StringRef CopyType = CreatingAuthenticatedReplacements ? "Authenticated" : "Protected";
        std::set<ManagedHAKCPointerUseP> ReplacementUses;
        for (auto &it: ReplacementStorage) {
            if (it.second == nullptr) {
                ReplacementUses.insert(it.first);
            }
        }

        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Creating " << CopyType << " Copies of uses of ";
            BaseDefinition->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << ":\n";
            for (auto &U: ReplacementUses) {
                CommonHAKCAnalysis::getWriter() << "\t" << U << "\n";
            }
        }
        for (auto &U: ReplacementUses) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Creating copy of " << U << "\n";
            }
            Value *ReplacementCopy;
            if (CreatingAuthenticatedReplacements) {
                ReplacementCopy = CreateAuthenticatedValue(U->get());
            } else {
                ReplacementCopy = CreateProtectedValue(U->get());
            }
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << CopyType << " Replacement of " << U << ": ";
                ReplacementCopy->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            ReplacementStorage[U] = ReplacementCopy;
        }
    }

    void ManagedHAKCPointer::CreatePointerUseClones() {
        if (GetBaseDefinitionUserCount() == 0) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "User count of ";
                BaseDefinition->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " is 0. No Pointer Clones needed\n";
            }
            return;
        }

        CreatePointerReplacements(AuthenticatedPointerReplacements);
        if (BaseDefinitionShouldBeTransferred()) {
            CreatePointerReplacements(ProtectedPointerReplacements);
        }
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "\n";
            Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\nAuthenticatedPointerReplacements:\n";
            for (auto &it: AuthenticatedPointerReplacements) {
                CommonHAKCAnalysis::getWriter() << it.first << ": ";
                if (it.second) {
                    it.second->print(CommonHAKCAnalysis::getWriter());
                } else {
                    CommonHAKCAnalysis::getWriter() << "nullptr";
                }
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            CommonHAKCAnalysis::getWriter() << "\n\nProtectedPointerReplacements:\n";
            for (auto &it: ProtectedPointerReplacements) {
                CommonHAKCAnalysis::getWriter() << it.first << ": ";
                if (it.second) {
                    it.second->print(CommonHAKCAnalysis::getWriter());
                } else {
                    CommonHAKCAnalysis::getWriter() << "nullptr";
                }
                CommonHAKCAnalysis::getWriter() << "\n";
            }
        }
    }

    bool ManagedHAKCPointer::BaseDefinitionIsFromKernel() {
        if (auto *Call = dyn_cast<CallInst>(BaseDefinition)) {
            if (Call->getCalledFunction()) {
                return Manager->GetFunctionAnalysis()->IsKernelFunction(Call->getCalledFunction());
            }
        }
        return false;
    }

    bool ManagedHAKCPointer::BaseDefinitionShouldBeTransferred() {
        if (!Manager->GetFunctionAnalysis()->isCompartmentalizedFunction()) {
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
            for (auto &it: ProtectedPointerReplacements) {
                if (isa<CallInst>(it.first->getUser())) {
                    return true;
                }
            }
        }

        return false;
    }

    void ManagedHAKCPointer::TransformUses() {
        if (GetBaseDefinitionUserCount() == 0) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "User count of ";
                BaseDefinition->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " is 0. No transformations needed\n";
            }
            return;
        }

        TransformUseSet(AuthenticatedPointerReplacements);
        if (BaseDefinitionShouldBeTransferred()) {
            TransformUseSet(ProtectedPointerReplacements);
        }
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Function after pointer transformation:\n";
            Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }
    }

    void
    ManagedHAKCPointer::TransformUseSet(std::map<ManagedHAKCPointerUseP, Value *> &StorageToUse) {
        bool UseAuthenticatedValue = (&StorageToUse == &AuthenticatedPointerReplacements);
        StringRef ReplacementSource = UseAuthenticatedValue ? "AuthenticatedPointerReplacements" :
                                      "ProtectedPointerReplacements";

        std::set<ManagedHAKCPointerUseP> FullSet;
        for (auto &it: StorageToUse) {
            if (UseAuthenticatedValue && !it.first->ForAuthenticatedUse()) {
                continue;
            } else if (!UseAuthenticatedValue && !it.first->ForProtectedUse()) {
                continue;
            }
            FullSet.insert(it.first);
        }
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Replacing the following operands with values from "
                                            << ReplacementSource << "\n";
            for (auto &Use: FullSet) {
                CommonHAKCAnalysis::getWriter() << "\t" << Use << "\n";
            }
            CommonHAKCAnalysis::getWriter() << "\nItems in " << ReplacementSource << ":\n";
            for (auto &it: StorageToUse) {
                CommonHAKCAnalysis::getWriter() << it.first << " -> ";
                if (it.second) {
                    it.second->print(CommonHAKCAnalysis::getWriter());
                } else {
                    CommonHAKCAnalysis::getWriter() << "nullptr";
                }
                CommonHAKCAnalysis::getWriter() << "\n";
            }
        }

        for (auto &Use: FullSet) {
            auto *Replacement = StorageToUse[Use];
            if (!Replacement) {
                CommonHAKCAnalysis::getWriter() << "Unable to find replacement of " << Use << " using "
                                                << ReplacementSource << "\n";
                Manager->GetFunctionAnalysis()->getFunction().print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
                throw std::exception();
            }

            if (Replacement == Use->getUser()) {
                continue;
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
            return AuthenticatedPointer;
        }
        auto Authenticated = Manager->CreateAuthenticatedInstruction(Operand, DebugActive);
        if (!Authenticated) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "CreateAuthenticatedInstruction returned null for ";
                Operand->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
        }
        if (Operand != BaseDefinition && isa<Instruction>(Authenticated)) {
            auto *AuthenticatedI = dyn_cast<Instruction>(Authenticated);
            for (auto i = 0; i < AuthenticatedI->getNumOperands(); i++) {
                auto *AuthOperand = AuthenticatedI->getOperand(i);
                if (AuthOperand == BaseDefinition || AuthOperand == ProtectedPointer) {
                    if (AuthenticatedI != AuthenticatedPointer) {
                        AuthenticatedI->setOperand(i, AuthenticatedPointer);
                    }
                }
            }
        }

        return Authenticated;
    }

    Value *ManagedHAKCPointer::CreateProtectedValue(Value *Operand) {
        if (Operand == AuthenticatedPointer) {
            return ProtectedPointer;
        }
        auto Protected = Manager->CreateProtectedInstruction(Operand, DebugActive);
        if (!Protected) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "CreateProtectedInstruction returned null for ";
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
        if (BaseDefinitionShouldBeTransferred() && ProtectedPointer == nullptr) {
            auto *BaseDefinitionInstruction = dyn_cast<Instruction>(BaseDefinition);

            ProtectedPointer = Manager->GetFunctionAnalysis()->CreateMissingTransfer(BaseDefinitionInstruction);
            std::set<ManagedHAKCPointerUseP> UsesToSet;

            for (auto &it: ProtectedPointerReplacements) {
                auto &Use = it.first;
                auto *User = Use->getUser();
                bool RegisterUserAsCopy = false;
                for (unsigned i = 0; i < User->getNumOperands(); i++) {
                    auto &Operand = User->getOperandUse(i);
                    if (Operand.get() == BaseDefinition) {
                        if (DebugActive) {
                            CommonHAKCAnalysis::getWriter() << "Replacing argument " << std::to_string(i)
                                                            << " of ";
                            User->print(CommonHAKCAnalysis::getWriter());
                            CommonHAKCAnalysis::getWriter() << " to ProtectedPointer ";
                            ProtectedPointer->print(CommonHAKCAnalysis::getWriter());
                            CommonHAKCAnalysis::getWriter() << "\n";
                        }
                        User->setOperand(i, ProtectedPointer);
                        RegisterUserAsCopy = UseShouldBeCloned(Operand) && !UseShouldBeIgnored(Operand);
                    }
                }
                if (RegisterUserAsCopy) {
                    UsesToSet.insert(Use);
                }
            }
            for (auto &Use: UsesToSet) {
                auto *User = Use->getUser();
                if (isa<CallInst>(User)) {
                    continue;
                }
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Registering ";
                    User->print(CommonHAKCAnalysis::getWriter());
                    CommonHAKCAnalysis::getWriter() << " as a Protected Copy of " << Use << "\n";
                }
                Manager->RegisterInstructionAsProtectedCopy(dyn_cast<Instruction>(User));
                Use->SetForProtectedUse(true);
                ProtectedPointerReplacements[Use] = User;
            }
        } else if (!BaseDefinitionShouldBeTransferred() && ProtectedPointer == nullptr) {
            ProtectedPointer = BaseDefinition;
        }
    }


    HAKCPointerManager::HAKCPointerManager(HAKCFunctionAnalysis *Analysis) :
            ManagedPointers(),
            AuthenticatedCopies(),
            ProtectedCopies(),
            HAKCAnalysis(Analysis) {

    }

    void HAKCPointerManager::ManagePointer(Value *V, bool debug) {
        auto ManagedPointer = GetManagedPointer(V);
        if (!ManagedPointer) {
            ManagedPointer = std::make_shared<ManagedHAKCPointer>(V, this, debug);
            ManagedPointers.insert(ManagedPointer);
        }
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
        return HAKCAnalysis->getDef(V, false, false);
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

    bool HAKCPointerManager::ValueWillBeAuthenticated(Value *V) {
        if (auto *I = dyn_cast<Instruction>(V)) {
            return AuthenticatedCopies.find(I) != AuthenticatedCopies.end();
        }
        return false;
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

    void HAKCPointerManager::CreateAuthenticatedPointers(bool debug) {
        for (auto &ManagedPtr: GetManagedPointers()) {
            ManagedPtr->CreateBaseAuthenticatedPointer();
            if (!ManagedPtr->GetAuthenticatedPointer()) {
                continue;
            }
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Authenticated Pointer for " << ManagedPtr << ": ";
                ManagedPtr->GetAuthenticatedPointer()->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            ManagedPtr->CreatePointerUseClones();
            if (debug) {
                CommonHAKCAnalysis::getWriter() << "Created Authenticated Copies for " << ManagedPtr << "\n";
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

    void HAKCPointerManager::RegisterInstructionAsAuthenticatedCopy(Instruction *I) {
        RegisterInstructionAsCopy(I, AuthenticatedCopies);
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
} // hakc
