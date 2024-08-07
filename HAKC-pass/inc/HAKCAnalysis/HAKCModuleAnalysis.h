//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCMODULEANALYSIS_H
#define HAKC_HAKCMODULEANALYSIS_H

#include "CommonHAKCAnalysis.h"
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
        bool IsCompartmentalizedAndContainsDebugName;
        std::vector<HAKCCompartment> UsedCompartments;
        Module &M;
        std::vector<Function *> AnalysisFunctions;

        explicit HAKCModuleAnalysis(Module &M);

        bool functionEscapes(Function *F);

        void RegisterUsedCompartment(HAKCCompartment &compartment);

        virtual std::string getGlobalHAKCSectionName(GlobalVariable *GV, HAKCCompartmentalizationPolicy &Policy);

        virtual HAKCFunctionAnalysis *
        GetFunctionTransformation(Function *F, HAKCCompartmentalizationPolicy &Policy) = 0;

        virtual void GetAnalysisFunctions();

        virtual std::shared_ptr<HAKCTransformer> CreateTransformer(HAKCCompartmentalizationPolicy &Policy) = 0;

        virtual void TransformModule(HAKCCompartmentalizationPolicy &Policy);

        virtual void TransformFunctions(HAKCCompartmentalizationPolicy &Policy);

        virtual void AddTransferFunctions(HAKCCompartmentalizationPolicy &Policy);

        void RegisterCustomTransfer(const hakc_custom_transfer_def_t &CustomTransfer);

        void RegisterHAKCTransfer(const hakc_transfer_def_t &Transfer);

        void RegisterNonTransferHAKCFunction(const hakc_function_def_t &HAKCFunction);

        virtual bool FunctionNeedsAnalysis(Function *F);

        virtual Function *CreateInitTransfer(GlobalVariable *GlobalVar, HAKCCompartmentalizationPolicy &Policy);

        virtual StringRef GlobalInitTransferPrefix() const;

        virtual StringRef GlobalInitTransferSectionName() const;

        virtual StringRef GlobalInitTransferPointerSectionName() const;

        virtual std::string
        GlobalVariableROSectionName(GlobalVariable *GlobalVar, HAKCCompartmentalizationPolicy &Policy);

        virtual void PopulateGlobalInitTransferFunc(Function *GlobTransfer, GlobalVariable *GlobalVar,
                                                    HAKCCompartmentalizationPolicy &Policy);

        virtual bool TransferIsNeeded(GlobalVariable *GlobalVar, HAKCCompartmentalizationPolicy &Policy);

        virtual bool
        ConstantStructTransferIsNeeded(ConstantStruct *ConstStruct, HAKCCompartmentalizationPolicy &Policy);

    private:
        std::shared_ptr<HAKCTransformer> transformer;

        std::vector<hakc_custom_transfer_def_t> CustomTransfers;
        std::set<hakc_transfer_def_t> Transfers;
        std::set<hakc_function_def_t> NonTransferHAKCFunctions;

        void MoveGlobalsToHAKCSection(HAKCCompartmentalizationPolicy &Policy);

    public:
        virtual ~HAKCModuleAnalysis() = default;

        virtual void performTransformations();

        bool functionInAnalysisSet(Function *F);

        virtual void AddCompartmentMetadata(HAKCCompartmentalizationPolicy &Policy);

        HAKCTransformer &getTransformer(HAKCCompartmentalizationPolicy &Policy);

        virtual std::set<StringRef> GetHAKCSourcePaths() = 0;

        virtual std::set<StringRef> GetSeparateNamespacePaths() = 0;

        std::set<StringRef> GetSafeTransitionFunctions() override;

        virtual std::vector<StringRef> GetSafeTransitionFunctions_Arch() = 0;

        virtual bool TransferFunctionShouldBeCreated(Function *F, HAKCCompartmentalizationPolicy &Policy);

        virtual bool AliasShouldBeCreated(Function *F, HAKCCompartmentalizationPolicy &Policy);

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

        virtual StringRef HAKCSignWithDivisionName();

        virtual StringRef HAKCEntryTokenName();

        virtual Function *GetFunctionByName(StringRef Name, FunctionType *FuncTy);

        virtual FunctionCallee GetFunctionCalleeByName(StringRef Name, FunctionType *FuncTy);

        virtual void CreateInitGlobalMemberTransfers(HAKCCompartmentalizationPolicy &Policy);

        Module &GetModule();

        /**
            * @brief Determines if a symbol is used in EXPORT_SYMBOL macro
            * @param F
            * @return
            */
        virtual bool FunctionIsExported(Function *F);
    };

} // hakc

#endif //HAKC_HAKCMODULEANALYSIS_H
