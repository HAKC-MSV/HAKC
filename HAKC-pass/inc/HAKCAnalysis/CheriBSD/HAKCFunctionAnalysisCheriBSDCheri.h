//
// Created by de29664 on 3/29/23.
//

#ifndef HAKC_HAKCFUNCTIONANALYSISCHERIBSDCHERI_H
#define HAKC_HAKCFUNCTIONANALYSISCHERIBSDCHERI_H

#include "HAKCAnalysis/HAKCModuleAnalysis.h"
#include "HAKCModuleAnalysisCheriBSDCheri.h"

namespace hakc {

    class HAKCFunctionAnalysisCheriBSDCheri : public HAKCFunctionAnalysis {
    public:
        HAKCFunctionAnalysisCheriBSDCheri(Function *F, HAKCModuleAnalysisCheriBSDCheri *ModAnalysis);

        Instruction *GetFinalAllocaDef(AllocaInst *Alloca) override;

        bool PointerIsAuthenticated_Arch(Value *Pointer) override;

        bool PointerShouldBeConsideredCode(Value *Pointer) override;

        std::vector<Value *> findDefChain(Value *v, bool followLoad, bool debug) override;

        static StringRef GetSafeCapName;

        static StringRef GetSafePtrName;

    protected:
        HAKCModuleAnalysisCheriBSDCheri *ModAnalysis;

        HAKCModuleAnalysis &getModuleAnalysis() override;

        std::set<Intrinsic::ID> GetInstrinsicsToSkip() override;

        std::set<Intrinsic::ID> GetCapabilityAdjustingIntrinsics();

        std::set<Intrinsic::ID> GetIntrinsicsNeedingAuthenticatedArgs() override;

        std::set<Intrinsic::ID> GetIntrinsicsToClone() override;

        void handleComparison(CmpInst *compare) override;

        bool pointerShouldBeChecked(Value *ptr) override;

        static bool TypeMatchesIgnoredTypes(Type *Ty);

        bool IsFunctionPointerWrapper(Value *Pointer);

        bool IsFunctionPointerStruct(Value *Pointer);

        std::set<StringRef> GetSafePointerFunctionNames() override;

        void UpdateHAKCFunctionParameters_Arch(CallInst *CallI, hakc_compartment_id_t TargetID,
                                               hakc_transfer_def_t &HAKCTransferFunction) override;

    private:
        std::vector<Value *> AddToDefChain(Value *V, std::vector<Value*> &ExistingChain, bool FollowLoad, bool Debug);
    };

} // hakc

#endif //HAKC_HAKCFUNCTIONANALYSISCHERIBSDCHERI_H
