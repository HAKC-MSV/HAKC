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
        HAKCFunctionAnalysisCheriBSDCheri(Function *F, HAKCModuleAnalysisCheriBSDCheri *ModTransform);

        Instruction *GetFinalAllocaDef(AllocaInst *Alloca) override;

        bool PointerIsAuthenticated_Arch(Value *Pointer) override;

        bool PointerShouldBeConsideredCode(Value *Pointer) override;

    protected:
        HAKCModuleAnalysisCheriBSDCheri *ModAnalysis;

        HAKCModuleAnalysis &getModuleAnalysis() override;

        std::set<Intrinsic::ID> GetInstrinsicsToSkip() override;

        std::set<Intrinsic::ID> GetIntrinsicsNeedingAuthenticatedArgs() override;

        void handleComparison(CmpInst *compare) override;

        bool pointerShouldBeChecked(Value *ptr) override;

        static bool TypeMatchesIgnoredTypes(Type *Ty);

        bool IsFunctionPointerWrapper(Value *Pointer);

        bool IsFunctionPointerStruct(Value *Pointer);
    };

} // hakc

#endif //HAKC_HAKCFUNCTIONANALYSISCHERIBSDCHERI_H
