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

namespace hakc {
    using namespace llvm;

    class ManagedHAKCPointerUse;

    using ManagedHAKCPointerUseP = std::shared_ptr<ManagedHAKCPointerUse>;

    class ManagedHAKCPointer;

    using ManagedHAKCPointerP = std::shared_ptr<ManagedHAKCPointer>;

    class HAKCPointerManager;

    /**
     * Stores Instruction and Operand to change
     */
    class ManagedHAKCPointerUse {
    public:
        ManagedHAKCPointerUse(ManagedHAKCPointerP P, User *User, unsigned OperandNo);

        User *getUser() const;

        void setUser(User *U);

        unsigned getOperandNo() const;

        Value *get() const;

        ManagedHAKCPointerP &getManagedPtr();

    protected:
        ManagedHAKCPointerP ManagedPtr;
        User *UserP;
        unsigned OperandNo;

    public:
        friend bool operator==(const ManagedHAKCPointerUseP &lhs, const Use &rhs) {
            return lhs->getUser() == rhs.getUser() && lhs->getOperandNo() == rhs.getOperandNo();
        }

        friend bool operator!=(const ManagedHAKCPointerUseP &lhs, const Use &rhs) {
            return !(lhs == rhs);
        }

        friend bool operator==(const Use &lhs, const ManagedHAKCPointerUseP &rhs) {
            return (rhs == lhs);
        }

        friend bool operator!=(const Use &lhs, const ManagedHAKCPointerUseP &rhs) {
            return !(lhs == rhs);
        }

        friend bool operator==(const ManagedHAKCPointerUseP &lhs, const ManagedHAKCPointerUseP &rhs) {
            return (lhs->getUser() == rhs->getUser()) && (lhs->getOperandNo() == rhs->getOperandNo());
        }

        friend bool operator!=(const ManagedHAKCPointerUseP &lhs, const ManagedHAKCPointerUseP &rhs) {
            return !(lhs == rhs);
        }

        friend raw_ostream &operator<<(raw_ostream &os, const ManagedHAKCPointerUseP &HAKCPointerUse);
    };

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

        bool BaseIsAuthenticated;

        bool ManuallyTransferred;

        unsigned ID;

        /**
         * Pointer uses and their replacements
         */
        std::set<ManagedHAKCPointerUseP> AuthenticatedUses;
        std::set<ManagedHAKCPointerUseP> ProtectedUses;
        std::set<ManagedHAKCPointerUseP> CloneUses;

        /**
         * Return the Authenticated version of Use
         * @param Use
         * @return
         */
        Value *CreateAuthenticatedValue(ManagedHAKCPointerUseP &Use);

        /**
         * Return the Signed version of Use
         * @param Use
         * @return
         */
        Value *CreateProtectedValue(ManagedHAKCPointerUseP &Use);

        void TransformUseSet(std::set<ManagedHAKCPointerUseP> &UseSet);

        void TransformClones();

        void CreatePointerReplacements();

        bool ComputeBasePointerAuthenticated();

        std::set<ManagedHAKCPointerUseP> GetAllUses();

        void SetProtectedPointer(Value *NewProtectedPointer);

        void SetAuthenticatedPointer(Value *NewAuthenticatedPointer);

    public:
        ManagedHAKCPointer(Value *Pointer, HAKCPointerManager *Manager, unsigned ID);

        Value *GetBaseDefinition() const;

        Value *GetAuthenticatedPointer();

        Value *GetProtectedPointer();

        void CreateBaseAuthenticatedPointer();

        void CreatePointerUseClones();

        bool BaseDefinitionShouldBeTransferred();

        void TransformUses();

        void MaybeCreateMissingTransfer();

        void RegisterManualHAKCTransfer(CallBase *CallI);

        unsigned GetAuthenticatedUserCount();

        unsigned GetProtectedUserCount();

        bool BaseIsAuthenticatedPointer() const;

        bool DetermineIfBasePointerIsAuthenticated();

        unsigned GetID() const;

        void AddAuthenticatedUse(ManagedHAKCPointerUseP &UPtr);

        void AddProtectedUse(ManagedHAKCPointerUseP &UPtr);

        void AddCloneUse(ManagedHAKCPointerUseP &UPtr);

    private:
        void InitBaseDefinition(Value *Pointer);

        void CheckPointerReplacement(Value *Old, Value *New, StringRef TypeName) const;


    public:
        friend bool operator==(const ManagedHAKCPointerP &lhs, Value *V) {
            return lhs->GetBaseDefinition() == V;
        }

        friend bool operator!=(const ManagedHAKCPointerP &lhs, Value *V) {
            return !(lhs == V);
        }

        friend bool operator==(Value *V, const ManagedHAKCPointerP &rhs) {
            return (rhs == V);
        }

        friend bool operator!=(Value *V, const ManagedHAKCPointerP &rhs) {
            return !(V == rhs);
        }

        friend bool operator==(const ManagedHAKCPointerP &lhs, const ManagedHAKCPointerP &rhs) {
            return lhs->GetBaseDefinition() == rhs->GetBaseDefinition();
        }

        friend bool operator!=(const ManagedHAKCPointerP &lhs, const ManagedHAKCPointerP &rhs) {
            return !(lhs == rhs);
        }

        friend raw_ostream &operator<<(raw_ostream &os, const ManagedHAKCPointer &ManagedPointer) {
            os << "Managed Pointer " << std::to_string(ManagedPointer.GetID());
            if(ManagedPointer.GetBaseDefinition()) {
                os << " [";
                if(isa<Argument>(ManagedPointer.GetBaseDefinition())) {
                    os << "  ";
                }
                os << *ManagedPointer.GetBaseDefinition() << "  ]";
            }
            return os;
        }

        friend raw_ostream &operator<<(raw_ostream &os, const ManagedHAKCPointerP &HAKCPointer) {
            os << *HAKCPointer;
            return os;
        }
    };

} // hakc

#endif //HAKC_MANAGEDHAKCPOINTER_H
