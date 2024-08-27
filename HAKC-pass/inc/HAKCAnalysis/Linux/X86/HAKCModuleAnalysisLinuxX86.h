//
// Created by de29664 on 3/31/23.
//

#ifndef HAKC_HAKCMODULEANALYSISX86_H
#define HAKC_HAKCMODULEANALYSISX86_H

#include "HAKCAnalysis/Linux/HAKCModuleAnalysisLinux.h"

namespace hakc {

    class HAKCModuleAnalysisLinuxX86 : public HAKCModuleAnalysisLinux {
    public:
        explicit HAKCModuleAnalysisLinuxX86(Module &M);

        std::set<StringRef> GetSeparateNamespacePaths() override;

        std::set<StringRef> GetNoTransferFunctions() override;

        std::set<StringRef> GetHAKCSourcePaths() override;

        bool functionIsTransferCandidate(Function *F, HAKCCompartmentalizationPolicy &Policy) override;

        bool TransferFunctionShouldBeCreated(Function *F, HAKCCompartmentalizationPolicy &Policy) override;

        bool AliasShouldBeCreated(Function *F, HAKCCompartmentalizationPolicy &Policy) override;

    protected:
        HAKCFunctionAnalysis *GetFunctionTransformation(Function *F, HAKCCompartmentalizationPolicy &Policy) override;

        std::shared_ptr<HAKCTransformer> CreateTransformer(HAKCCompartmentalizationPolicy &Policy) override;
    };

} // hakc

#endif //HAKC_HAKCMODULEANALYSISX86_H
