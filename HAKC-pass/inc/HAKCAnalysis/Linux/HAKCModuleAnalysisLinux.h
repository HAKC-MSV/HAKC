//
// Created by de29664 on 3/31/23.
//

#ifndef HAKC_HAKCMODULEANALYSISLINUX_H
#define HAKC_HAKCMODULEANALYSISLINUX_H

#include "HAKCAnalysis/HAKCModuleAnalysis.h"
#include "HAKCTransformers/Linux/HAKCTransformerLinux.h"

namespace hakc {

    class HAKCTransformerLinux;

    class HAKCModuleAnalysisLinux : public HAKCModuleAnalysis {
    public:
        explicit HAKCModuleAnalysisLinux(Module &M);

        void InitHAKCFunctions() override;

        std::set<StringRef> GetHAKCSourcePaths() override;

        static HAKC_Division_ID getFunctionColor(Function *F, HAKCCompartmentalizationPolicy &Policy);

        static HAKC_Division_ID getGlobalColor(GlobalVariable *GV, HAKCCompartmentalizationPolicy &Policy);

        virtual StructType *GetKernelParamType();

        std::set<StringRef> GetIgnoredGlobals() override;

        std::map<StringRef, hakc_allocation_size_map_t> GetKernelAllocationSizeMap() override;

        std::set<StringRef> GetSeparateNamespacePaths() override;

        bool valueIsReadonlyPtr(Value *value) override;

        std::set<StringRef> GetIgnoredTypes() override;

        std::set<StringRef> GetNoTransferFunctions() override;

//        virtual sym_color_t GetMajoritySymbolColor();

        static std::string getColorStringFromValue(HAKC_Division_ID color);

        static sym_color_t getColorFromValue(HAKC_Division_ID Color);

        std::vector<StringRef> GetSafeTransitionFunctions_Arch() override;

        static std::string getKstrtab_entry_name(Function *F);

        static std::string getKstrtabns_entry_name(Function *F);

    protected:
        std::string getGlobalHAKCSectionName(GlobalVariable *GV, HAKCCompartmentalizationPolicy &Policy) override;

        void TransformModule(HAKCCompartmentalizationPolicy &Policy) override;

        static HAKC_Division_ID getSymbolColor(GlobalValue *GV, HAKCCompartmentalizationPolicy &Policy);

        HAKC_Division_ID getColor(sym_color_t Color);

        virtual GlobalValue *ExtractGlobalFromKernelParam(GlobalVariable *GV);

        virtual void emitModParamGetCtx(GlobalValue *kernparam, HAKCCompartmentalizationPolicy &Policy);

        bool FunctionNeedsAnalysis(Function *F) override;

        bool FunctionIsExported(Function *F) override;

        virtual void TransferModuleParams(HAKCCompartmentalizationPolicy &Policy);

        void GenerateModuleParamGetCtxFunction(GlobalVariable *GV, HAKCCompartmentalizationPolicy &Policy);

    };

} // hakc

#endif //HAKC_HAKCMODULEANALYSISLINUX_H
