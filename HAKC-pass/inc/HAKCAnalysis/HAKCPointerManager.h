//
// Created by de29664 on 11/14/23.
//

#ifndef HAKC_HAKCPOINTERMANAGER_H
#define HAKC_HAKCPOINTERMANAGER_H

#include <set>
#include "llvm/IR/Instructions.h"
#include "ManagedHAKCPointer.h"

namespace hakc {
    using namespace llvm;

    class HAKCFunctionAnalysis;

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
        explicit HAKCPointerManager(HAKCFunctionAnalysis *Analysis, bool DebugActive);

        bool ManagePointer(Value *V);

        std::set<ManagedHAKCPointerP> GetManagedPointers();

        HAKCFunctionAnalysis *GetFunctionAnalysis();

        /**
         * Returns the ManagedHAKCPointer that corresponds to the definition V
         * @param V
         * @return
         */
        ManagedHAKCPointerP GetManagedPointer(Value *V);

        bool empty();

        Value *GetDef(Value *V);

        /**
         * Return the Authenticated version of Pointer
         * @param Pointer
         * @param Debug
         * @return
         */
        Value *CreateAuthenticatedValue(ManagedHAKCPointerUseP PointerUse);

        Value *CreateProtectedValue(ManagedHAKCPointerUseP PointerUse);

        Value *FindAuthenticatedValue(Value *V);

        Value *FindProtectedValue(Value *V);

        Value *FindAuthenticatedValue(ManagedHAKCPointerUseP PointerUse);

        Value *FindProtectedValue(ManagedHAKCPointerUseP PointerUse);

        Value *FindManagedValue(std::map<ManagedHAKCPointerUseP, Value *> &Storage, ManagedHAKCPointerUseP PointerUse);

        /**
         * Create authenticated versions of the ManagedHAKCPointer set
         * @param Debug
         */
        void CreateAuthenticatedPointersAndAllClones();

        void TransformPointers();

        void AddAuthenticatedPointer(ManagedHAKCPointerUseP PointerUse, Value *Replacement);

        void AddProtectedPointer(ManagedHAKCPointerUseP PointerUse, Value *Replacement);

        bool ValueWillBeAuthenticated(Value *V);

        unsigned GetDataAuthenticationsAdded() const;

        unsigned GetCodeAuthenticationsAdded() const;

        unsigned GetSafePointersAdded() const;

        unsigned GetClonesAdded() const;

        unsigned GetTotalAdditions() const;

        void PrintProtectedValues() const;

        void PrintAuthenticatedValues() const;

        bool FunctionIsCompartmentalized() const;

        void SetFunctionIsCompartmentalized(bool FunctionIsCompartmentalized);

        void UpdateProtectedMultiUsers(User *AuthenticatedMultiUser, User *ProtectedMultiUser);

    protected:
        /**
         * The set of pointers under management
         */
        std::set<ManagedHAKCPointerP> ManagedPointers;

        std::map<ManagedHAKCPointerUseP, Value *> AuthenticatedValues;
        std::map<ManagedHAKCPointerUseP, Value *> ProtectedValues;

        std::set<ManagedHAKCPointerUseP> AnalyzedUses;

        HAKCFunctionAnalysis *HAKCAnalysis;

        unsigned DataAuthenticationsAdded;
        unsigned CodeAuthenticationsAdded;
        unsigned SafePointersAdded;
        unsigned ClonesAdded;

        bool IsCompartmentalized;
        bool DebugActive;

        Instruction *CloneInstruction(Instruction *I);

        Value *CreateSafePointerAtLocation(Value *Pointer, Instruction *InsertLocation);

        Value *CreateAuthenticationAtLocation(Value *Pointer, Instruction *InsertLocation);

        bool PointerIsEligibleForManagement(Value *Pointer);

        void AddHAKCPointerReplacement(ManagedHAKCPointerUseP PtrUse, Value *Replacement,
                                       bool AddingAuthenticatedReplacements);

        Value *FindManagedValue(std::map<ManagedHAKCPointerUseP, Value *> &Storage, Value *Target);

        void ManageNewPointer(Value *V);

        void ClassifyAllUsesOfDefinition(Value *Definition, ManagedHAKCPointerP ManagedPointer);

        bool UseIsAnalyzed(ManagedHAKCPointerUseP &UseP);

        static bool UseShouldBeIgnored(Use &U);

        static bool UseShouldBeCloned(Use &U);

        bool UseShouldUtilizeAuthenticatedPointer(Use &U);

        bool UseShouldUtilizeSignedBasePointer(Use &U);

        bool IsClonedUseNeedingAdditionalClassification(Use &U);

        static void PrintManagedValues(const std::map<ManagedHAKCPointerUseP, Value *> &Storage);

        Value *FindManagedPointerReplacement(Value *Target, bool ReturnAuthenticatedPointer);

    private:
        unsigned CurrentPointerID;
    };

} // hakc

#endif //HAKC_HAKCPOINTERMANAGER_H
