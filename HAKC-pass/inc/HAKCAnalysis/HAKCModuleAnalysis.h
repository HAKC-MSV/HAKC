//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCMODULEANALYSIS_H
#define HAKC_HAKCMODULEANALYSIS_H

#include "CommonHAKCAnalysis.h"
#include "HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"

namespace hakc {

#define HAKC_TRANSFER(Name, CompartmentIDIdx, ColorIdx) RegisterHAKCTransfer \
(std::make_shared<hakc::HAKCTransferFunction>(Name, (unsigned)0, CompartmentIDIdx, ColorIdx))

#define HAKC_TRANSFER_NO_COLOR(Name, CompartmentIDIdx) RegisterHAKCTransfer \
(std::make_shared<hakc::HAKCTransferFunction>(Name, (unsigned)0, CompartmentIDIdx))

#define HAKC_FUNCTION(Name) RegisterNonTransferHAKCFunction(std::make_shared<hakc::HAKCFunctionDefinition>(Name))

#define HAKC_CUSTOM_TRANSFER(CustomTransfer, Args...) RegisterCustomTransfer(std::make_shared<CustomTransfer>(Args)

    class HAKCFunctionAnalysis;

    class HAKCModuleAnalysis : public CommonHAKCAnalysis {
    protected:
        bool moduleModified;
        std::set<int64_t> used_compartments;
        Module &M;

        explicit HAKCModuleAnalysis(Module &M);

        bool functionEscapes(Function *F);

        void registerUsedCompartment(int64_t compartment);

        virtual std::string getGlobalHAKCSectionName(GlobalVariable *GV);

        virtual void CompartmentalizeFunction(Function *F);

        virtual HAKCFunctionAnalysis *GetFunctionTransformation(Function *F) = 0;

        virtual bool isModuleCompartmentalized();

        virtual void GetAnalysisFunctions();

        virtual std::shared_ptr<HAKCTransformer> CreateTransformer() = 0;

        virtual void compartmentalizeModule();

        virtual void removeSignatures();

        virtual void addTransferFunctions();

        void RegisterCustomTransfer(const hakc_custom_transfer_def_t &CustomTransfer);

        void RegisterHAKCTransfer(const hakc_transfer_def_t &Transfer);

        void RegisterNonTransferHAKCFunction(const hakc_function_def_t &HAKCFunction);

        virtual bool FunctionNeedsAnalysis(Function *F);

        virtual Function* CreateInitTransfer(GlobalVariable *GlobalVar);

        virtual StringRef GlobalInitTransferPrefix() const;

        virtual StringRef GlobalInitTransferSectionName() const;

        virtual std::string GlobalVariableROSectionName(GlobalVariable *GlobalVar);

        virtual void PopulateGlobalInitTransferFunc(Function *GlobTransfer, GlobalVariable *GlobalVar);

        std::set<Function *> AnalysisFunctions;

    private:
        std::shared_ptr<HAKCTransformer> transformer;

        std::vector<hakc_custom_transfer_def_t> CustomTransfers;
        std::set<hakc_transfer_def_t> Transfers;
        std::set<hakc_function_def_t> NonTransferHAKCFunctions;

        void moveGlobalsToPMCSection();

        /**
         * @brief Determines if a symbol is used in EXPORT_SYMBOL macro
         * @param F
         * @return
         */
        bool functionIsExported(Function *F);

    public:
        unsigned totalDataChecks, totalCodeChecks, totalTransfers;

        virtual ~HAKCModuleAnalysis() = default;

        virtual bool isModuleTransformed();

        virtual void performTransformations();

        bool functionInAnalysisSet(Function *F);

        virtual void addCompartmentMetadata();

        HAKCTransformer &getTransformer() override;

        virtual std::set<StringRef> GetHAKCSourcePaths() = 0;

        virtual std::set<StringRef> GetSeparateNamespacePaths() = 0;

        std::set<StringRef> GetSafeTransitionFunctions() override;

        virtual std::vector<StringRef> GetSafeTransitionFunctions_Arch() = 0;

        virtual bool TransferFunctionShouldBeCreated(Function *F);

        virtual bool AliasShouldBeCreated(Function *F);

        virtual bool FunctionDefinedInAssembly(Function *F);

        std::set<hakc_function_def_t> GetHAKCFunctions() override;

        std::set<hakc_transfer_def_t> GetHAKCTransferFunctions() override;

        virtual std::set<hakc_custom_transfer_def_t> GetHAKCCustomTransferFunctions();

        virtual void InitAnalysis();

        virtual void InitHAKCFunctions() = 0;

        virtual StringRef HACKCodeAuthenticationName();

        virtual StringRef HAKCDataAuthenticationName();

        virtual StringRef HAKCCompartmentTransferName();

        virtual StringRef HAKCPerCPUCompartmentTransferName();

        virtual StringRef HAKCSignWithColorName();

        virtual StringRef HAKCEntryTokenName();

        virtual Function *GetFunctionByName(StringRef Name, FunctionType *FuncTy);

        virtual FunctionCallee GetFunctionCalleeByName(StringRef Name, FunctionType *FuncTy);

        virtual StructType *GetKernelParamType() = 0;

        virtual void generateModuleParamGetCtxFunction(GlobalVariable *GV) = 0;

        virtual void transferModuleParams() = 0;

        virtual void CreateInitGlobalMemberTransfers();

        Module &GetModule();
    };

} // hakc

#endif //HAKC_HAKCMODULEANALYSIS_H
