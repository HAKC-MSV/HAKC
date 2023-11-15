//
// Created by de29664 on 9/18/23.
//

#ifndef HAKC_MANAGEDHAKCPOINTER_H
#define HAKC_MANAGEDHAKCPOINTER_H

#include <map>
#include <set>
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"

#include "HAKCPointerManager.h"

namespace hakc {
    using namespace llvm;

    /**
     * Stores Instruction and Operand to change
     */
    class ManagedHAKCPointerUse {
    public:
        ManagedHAKCPointerUse(User *UserP, unsigned OperandNo);

        User *getUser() const;

        unsigned getOperandNo() const;

        Value *get() const;

        bool ForAuthenticatedUse() const;

        bool ForProtectedUse() const;

        void SetForAuthenticatedUse(bool ForAuthUse);

        void SetForProtectedUse(bool ForProtUse);

    protected:
        User *UserP;
        unsigned OperandNo;
        bool AuthenticatedUse;
        bool ProtectedUse;

    public:
        friend bool operator==(const std::shared_ptr<ManagedHAKCPointerUse> &lhs, const Use &rhs) {
            return lhs->getUser() == rhs.getUser() && lhs->getOperandNo() == rhs.getOperandNo();
        }

        friend bool operator!=(const std::shared_ptr<ManagedHAKCPointerUse> &lhs, const Use &rhs) {
            return !(lhs == rhs);
        }

        friend bool operator==(const Use &lhs, const std::shared_ptr<ManagedHAKCPointerUse> &rhs) {
            return (rhs == lhs);
        }

        friend bool operator!=(const Use &lhs, const std::shared_ptr<ManagedHAKCPointerUse> &rhs) {
            return !(lhs == rhs);
        }

        friend bool operator==(const std::shared_ptr<ManagedHAKCPointerUse> &lhs, const
        std::shared_ptr<ManagedHAKCPointerUse> &rhs) {
            return (lhs->getUser() == rhs->getUser()) && (lhs->getOperandNo() == rhs->getOperandNo());
        }

        friend bool operator!=(const std::shared_ptr<ManagedHAKCPointerUse> &lhs, const
        std::shared_ptr<ManagedHAKCPointerUse> &rhs) {
            return !(lhs == rhs);
        }

        friend raw_ostream &operator<<(raw_ostream &os, const std::shared_ptr<ManagedHAKCPointerUse> &HAKCPointerUse) {
            os << "Argument " << std::to_string(HAKCPointerUse->getOperandNo()) << " of ";
            HAKCPointerUse->UserP->print(os);
            return os;
        }
    };

    using ManagedHAKCPointerUseP = std::shared_ptr<ManagedHAKCPointerUse>;

    /**
     * A single managed Pointer.  Contains the original definition of the pointer, an authenticated pointer suitable
     * for dereferencing, and a protected pointer to be used in function arguments.  The base definition and
     * protected pointer can be different if BaseDefinition is from an external function call.
     */
    class ManagedHAKCPointer {
    protected:
        /**
         * The original source of a pointer
         */
        Value *BaseDefinition;
        /**
         * A pointer suitable for dereferencing
         */
        Value *AuthenticatedPointer;
        /**
         * A pointer belonging to the current function compartment
         */
        Value *ProtectedPointer;
        bool DebugActive;
        HAKCPointerManager *Manager;
        /**
         * Pointer uses and their replacements
         */
        std::map<ManagedHAKCPointerUseP, Value *> AuthenticatedPointerReplacements;
        std::map<ManagedHAKCPointerUseP, Value *> ProtectedPointerReplacements;

        void ClassifyAllUsesOfBaseDefinition(Value *Def);

        /**
         * Return the Authenticated version of Operand
         * @param Operand
         * @return
         */
        Value *CreateAuthenticatedValue(Value *Operand);

        /**
         * Return the Signed version of Operand
         * @param Operand
         * @return
         */
        Value *CreateProtectedValue(Value *Operand);

        bool UseShouldUtilizeAuthenticatedPointer(Use &U);

        bool UseShouldBeCloned(Use &U);

        bool UseShouldUtilizeSignedBasePointer(Use &U);

        bool UseShouldBeIgnored(Use &U);

        bool UseIsAnalyzed(ManagedHAKCPointerUseP &UseP);

        bool BaseIsAuthenticatedPointer();

        void TransformUseSet(std::map<ManagedHAKCPointerUseP, Value *> &StorageToUse);

        void CreatePointerReplacements(std::map<ManagedHAKCPointerUseP, Value *> &ReplacementStorage);

        std::set<Instruction *> GetBaseDefinitionUsers();

        unsigned GetAuthenticatedUserCount();

        unsigned GetProtectedUserCount();

    public:
        ManagedHAKCPointer(Value *Pointer, HAKCPointerManager *Manager, bool debug);

        Value *GetBaseDefinition() const;

        Value *GetAuthenticatedPointer();

        Value *GetProtectedPointer();

        void CreateBaseAuthenticatedPointer();

        void CreatePointerUseClones();

        bool BaseDefinitionShouldBeTransferred();

        void TransformUses();

        void MaybeCreateMissingTransfer();

        void InitializeUses();

    private:
        void InitBaseDefinition(Value *Pointer);


    public:
        friend bool operator==(const std::shared_ptr<ManagedHAKCPointer> &lhs, Value *V) {
            return lhs->GetBaseDefinition() == lhs->Manager->GetDef(V);
        }

        friend bool operator!=(const std::shared_ptr<ManagedHAKCPointer> &lhs, Value *V) {
            return !(lhs == V);
        }

        friend bool operator==(Value *V, const std::shared_ptr<ManagedHAKCPointer> &rhs) {
            return (rhs == V);
        }

        friend bool operator!=(Value *V, const std::shared_ptr<ManagedHAKCPointer> &rhs) {
            return !(V == rhs);
        }

        friend bool operator==(const std::shared_ptr<ManagedHAKCPointer> &lhs, const
        std::shared_ptr<ManagedHAKCPointer> &rhs) {
            return lhs->GetBaseDefinition() == rhs->GetBaseDefinition();
        }

        friend bool operator!=(const std::shared_ptr<ManagedHAKCPointer> &lhs, const
        std::shared_ptr<ManagedHAKCPointer> &rhs) {
            return !(lhs == rhs);
        }

        friend raw_ostream &operator<<(raw_ostream &os, const std::shared_ptr<ManagedHAKCPointer> &HAKCPointer) {
            HAKCPointer->GetBaseDefinition()->print(os);
            return os;
        }
    };

} // hakc

#endif //HAKC_MANAGEDHAKCPOINTER_H
