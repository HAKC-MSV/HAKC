//
// Created by de29664 on 3/29/23.
//

#ifndef HAKC_HAKCMODULEANALYSISCHERIBSDCHERI_H
#define HAKC_HAKCMODULEANALYSISCHERIBSDCHERI_H

#include "HAKCAnalysis/HAKCModuleAnalysis.h"

namespace hakc {

    class HAKCModuleAnalysisCheriBSDCheri : public HAKCModuleAnalysis {
    public:
        explicit HAKCModuleAnalysisCheriBSDCheri(Module &Module);

        std::set<StringRef> GetNoTransferFunctions() override;

        std::map<StringRef, hakc_allocation_size_map_t> GetKernelAllocationSizeMap() override;

        std::set<StringRef> GetIgnoredTypes() override;

        std::set<StringRef> GetHAKCSourcePaths() override;

        StructType *GetKernelParamType() override;

        void generateModuleParamGetCtxFunction(GlobalVariable *GV) override;

        void transferModuleParams() override;

        void InitHAKCFunctions() override;

        StringRef HACKCodeAuthenticationName() override;

        StringRef HAKCDataAuthenticationName() override;

        StringRef HAKCCompartmentTransferName() override;

        static StringRef HAKCCompartmentTransferNoCapName();

        bool FunctionNeedsAnalysis(Function *F) override;

        Function *GetFunctionByName(StringRef Name, FunctionType *FuncTy) override;

    protected:

        HAKCFunctionAnalysis *GetFunctionTransformation(Function *F) override;

        std::set<StringRef> GetSeparateNamespacePaths() override;

        std::shared_ptr<HAKCTransformer> CreateTransformer() override;

        std::vector<StringRef> GetSafeTransitionFunctions_Arch() override;

        bool HybridModeEnabled();
    };

} // hakc

#endif //HAKC_HAKCMODULEANALYSISCHERIBSDCHERI_H
