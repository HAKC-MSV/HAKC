//
// Created by de29664 on 11/14/23.
//

#ifndef HAKC_HAKCPOINTERMANAGER_H
#define HAKC_HAKCPOINTERMANAGER_H

#include <set>
#include "llvm/IR/Instructions.h"

namespace hakc {
    using namespace llvm;

    class HAKCFunctionAnalysis;

    class ManagedHAKCPointer;

    /**
     * In a function, there are two versions of each pointer that need to be tracked, a pointer that can be
     * dereferenced and a pointer that is protected.  Any dereference could be the result of arbitrary number of
     * Instructions, and we assume that pointer origins (e.g., function arguments or the result of a call) return a
     * protected pointer.  So Instructions that lead to a dereference will dereference a protected pointer, so those
     * Instructions need to be cloned and modified to use the authenticated pointer.  This manager tracks those
     * clones, so exactly one is ever created.
     */
    class HAKCPointerManager {
        friend class ManagedHAKCPointer;
    public:
        explicit HAKCPointerManager(HAKCFunctionAnalysis *Analysis);

        bool ManagePointer(Value *V, bool debug);

        std::set<std::shared_ptr<ManagedHAKCPointer>> GetManagedPointers();

        HAKCFunctionAnalysis *GetFunctionAnalysis();

        /**
         * Returns the ManagedHAKCPointer that corresponds to the definition V
         * @param V
         * @return
         */
        std::shared_ptr<ManagedHAKCPointer> GetManagedPointer(Value *V);

        bool empty();

        Value *GetDef(Value *V);

        Value *GetDef(Value *V, bool DebugActive);

        /**
         * Return the Authenticated version of Pointer
         * @param Pointer
         * @param Debug
         * @return
         */
        Value *CreateAuthenticatedValue(Value *Pointer, bool Debug);

        Value *CreateProtectedValue(Value *Pointer, bool debug);

        Value* FindAuthenticatedValue(Value *V);
        Value* FindProtectedValue(Value *V);

        /**
         * Create authenticated versions of the ManagedHAKCPointer set
         * @param Debug
         */
        void CreateAuthenticatedPointersAndAllClones(bool Debug);

        void TransformPointers(bool Debug);

        void AddAuthenticatedPointer(Value *Ptr, Value *Replacement);
        void AddProtectedPointer(Value *Ptr, Value *Replacement);
        void AddAuthenticatedPointer(Value *Ptr, Value *Replacement, bool Debug);
        void AddProtectedPointer(Value *Ptr, Value *Replacement, bool Debug);

        bool ValueWillBeAuthenticated(Value *V);

        unsigned GetDataAuthenticationsAdded();
        unsigned GetCodeAuthenticationsAdded();
        unsigned GetSafePointersAdded();
        unsigned GetClonesAdded();
        unsigned GetTotalAdditions();

        void PrintProtectedValues() const;
        void PrintAuthenticatedValues() const;

    protected:
        /**
         * The set of pointers under management
         */
        std::set<std::shared_ptr<ManagedHAKCPointer>> ManagedPointers;

        std::map<Value *, Value *> AuthenticatedValues;
        std::map<Value *, Value *> ProtectedValues;

        HAKCFunctionAnalysis *HAKCAnalysis;

        unsigned DataAuthenticationsAdded;
        unsigned CodeAuthenticationsAdded;
        unsigned SafePointersAdded;
        unsigned ClonesAdded;

        Instruction *CloneInstruction(Instruction *I);

        Value* CreateSafePointerAtLocation(Value *Pointer, Instruction *InsertLocation);

        Value* CreateAuthenticationAtLocation(Value *Pointer, Instruction *InsertLocation);

        bool PointerIsEligibleForManagement(Value *Pointer, bool Debug);

        bool CloneableManagedPointer(Value *V);
    };

} // hakc

#endif //HAKC_HAKCPOINTERMANAGER_H
