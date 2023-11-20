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

        void ManagePointer(Value *V, bool debug);

        std::set<std::shared_ptr<ManagedHAKCPointer>> GetManagedPointers();

        HAKCFunctionAnalysis *GetFunctionAnalysis();

        /**
         * Returns the ManagedHAKCPointer that corresponds to the definition V
         * @param V
         * @return
         */
        std::shared_ptr<ManagedHAKCPointer> GetManagedPointer(Value *V);

        /**
         * Returns the ManagedHAKCPointer if V == BaseDefinition
         * @param V
         * @return
         */
        std::shared_ptr<ManagedHAKCPointer> GetManagedPointerByBaseDefinition(Value *V);

        bool empty();

        Value *GetDef(Value *V);

        /**
         * Return the Authenticated version of Pointer
         * @param Pointer
         * @param debug
         * @return
         */
        Value *CreateAuthenticatedInstruction(Value *Pointer, bool debug);

        Value *CreateProtectedInstruction(Value *Pointer, bool debug);

        /**
         * Search the copy set for V
         * @param V
         * @return
         */
        Instruction *FindAuthenticatedCopy(Value *V);

        Instruction *FindProtectedCopy(Value *V);

        /**
         * Checks if V is an authenticated pointer or an authenticated copy
         */
        bool ValueIsAuthenticated(Value *V);

        /**
         * Return true if V is in the copy set
         * @param V
         * @return
         */
        bool ValueIsAuthenticatedCopy(Value *V);

        bool ValueIsProtectedCopy(Value *V);

        /**
         * Return true if V is an authenticated pointer
         * @param V
         * @return
         */
        bool ValueIsAuthenticatedPointer(Value *V);

        bool ValueIsProtectedPointer(Value *V);

        /**
         * Create authenticated versions of the ManagedHAKCPointer set
         * @param debug
         */
        void CreateAuthenticatedPointersAndAllClones(bool debug);

        void RegisterInstructionAsProtectedCopy(Instruction *I);

        void TransformPointers(bool Debug);

        unsigned GetDataAuthenticationsAdded();
        unsigned GetCodeAuthenticationsAdded();
        unsigned GetSafePointersAdded();
        unsigned GetClonesAdded();
        unsigned GetTotalAdditions();

    protected:
        /**
         * The set of pointers under management
         */
        std::set<std::shared_ptr<ManagedHAKCPointer>> ManagedPointers;
        /**
         * Mapping of original Instructions to their copies
         */
        std::map<Instruction *, Instruction *> AuthenticatedCopies;
        std::map<Instruction *, Instruction *> ProtectedCopies;

        HAKCFunctionAnalysis *HAKCAnalysis;

        unsigned DataAuthenticationsAdded;
        unsigned CodeAuthenticationsAdded;
        unsigned SafePointersAdded;
        unsigned ClonesAdded;

        Instruction *CloneInstruction(Instruction *I, std::map<Instruction *, Instruction *> &CopyStorage);

        Instruction *FindCopy(Value *V, std::map<Instruction *, Instruction *> &CopyStorage);

        bool ValueIsCopy(Value *V, std::map<Instruction *, Instruction *> &CopyStorage);

        void RegisterInstructionAsCopy(Instruction *I, std::map<Instruction *, Instruction *> &CopyStorage);

        void TransformClones(std::map<Instruction *, Instruction *> &CloneStorage, bool Debug);

        Value* CreateSafePointerAtLocation(Value *Pointer, Instruction *InsertLocation);

        Value* CreateAuthenticationAtLocation(Value *Pointer, Instruction *InsertLocation);
    };

} // hakc

#endif //HAKC_HAKCPOINTERMANAGER_H
