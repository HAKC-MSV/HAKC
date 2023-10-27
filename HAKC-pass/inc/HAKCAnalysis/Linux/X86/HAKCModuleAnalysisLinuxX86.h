//
// Created by de29664 on 3/31/23.
//

#ifndef HAKC_HAKCMODULEANALYSISX86_H
#define HAKC_HAKCMODULEANALYSISX86_H

#include "HAKCAnalysis/Linux/HAKCModuleAnalysisLinux.h"

namespace hakc {

    class HAKCModuleAnalysisLinuxX86 : public HAKCModuleAnalysisLinux {
    public:
        HAKCModuleAnalysisLinuxX86(Module &M);

        std::set<StringRef> GetSeparateNamespacePaths() override;

        std::set<StringRef> GetNoTransferFunctions() override;

        std::set<StringRef> GetHAKCSourcePaths() override;

        bool functionIsTransferCandidate(Function *F) override;

        bool TransferFunctionShouldBeCreated(Function *F) override;

        bool AliasShouldBeCreated(Function *F) override;

    protected:
        HAKCFunctionAnalysis *GetFunctionTransformation(Function *F) override;

        std::shared_ptr<HAKCTransformer> CreateTransformer() override;
    };

} // hakc

#endif //HAKC_HAKCMODULEANALYSISX86_H
