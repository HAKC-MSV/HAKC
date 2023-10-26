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

    protected:
        HAKCModuleAnalysisCheriBSDCheri *ModAnalysis;

        HAKCModuleAnalysis &getModuleAnalysis() override;

        std::set<Intrinsic::ID> GetInstrinsicsToSkip() override;

        void handleComparison(CmpInst *compare) override;

        bool pointerShouldBeChecked(Value *ptr) override;

        static bool TypeMatchesIgnoredTypes(Type *Ty);
    };

} // hakc

#endif //HAKC_HAKCFUNCTIONANALYSISCHERIBSDCHERI_H
