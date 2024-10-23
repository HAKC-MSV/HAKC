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
        bool MajorityColorSet;
        sym_color_t MajorityColor;

        ConstantInt *getSymbolColor(GlobalValue *GV);

        ConstantInt *GetColorValue(sym_color_t Color);

        GlobalValue *ExtractGlobalFromKernelParam(GlobalVariable *GV);

        void emitModParamGetCtx(GlobalValue *kernparam);

        bool functionEscapes(Function *F);

        void registerUsedCompartment(int64_t compartment);

        std::string getGlobalHAKCSectionName(GlobalVariable *GV);

        void updateCallParameters(std::map<Function *, std::set<CallInst *>> calls_map);

        void CompartmentalizeFunction(Function *F);

        HAKCFunctionAnalysis *GetFunctionTransformation(Function *F);

        bool isModuleCompartmentalized();

        void GetAnalysisFunctions();

        std::shared_ptr<HAKCTransformer> CreateTransformer();

        void compartmentalizeModule();

        void removeSignatures();

        void addTransferFunctions();

        void RegisterCustomTransfer(hakc_custom_transfer_def_t CustomTransfer);

        void RegisterHAKCTransfer(hakc_transfer_def_t Transfer);

        void RegisterNonTransferHAKCFunction(hakc_function_def_t HAKCFunction);

        bool FunctionNeedsAnalysis(Function *F);

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
        std::map<Function *, std::set<CallInst *>> HAKCFunctions;
        HAKCSystemInformation SysInfo;

        explicit HAKCModuleAnalysis(Module &M);

        bool functionIsTransferCandidate(Function *F);

        ConstantInt *getFunctionColor(Function *F);

        ConstantInt *getGlobalColor(GlobalVariable *GV);

        std::set<StringRef> GetIgnoredGlobals();

        std::map<StringRef, std::tuple<void*, std::vector<int>>> GetKernelAllocationSizeMap();

        bool valueIsReadonlyPtr(Value *value);

        std::set<StringRef> GetIgnoredTypes();

        std::set<StringRef> GetNoTransferFunctions();

        sym_color_t GetMajoritySymbolColor();

        static std::string getColorStringFromValue(ConstantInt *color);

        static sym_color_t getColorFromValue(ConstantInt *Color);

        ~HAKCModuleAnalysis() = default;

        bool isModuleTransformed();;

        void performTransformations();

        bool functionInAnalysisSet(Function *F);

        void addCompartmentMetadata();

        HAKCTransformer &getTransformer();

        std::set<StringRef> GetHAKCSourcePaths();

        std::set<StringRef> GetSeparateNamespacePaths();

        std::set<StringRef> GetSafeTransitionFunctions();

        std::set<StringRef> GetSafeTransitionFunctions_Arch();

        bool TransferFunctionShouldBeCreated(Function *F);

        bool AliasShouldBeCreated(Function *F);

        bool FunctionDefinedInAssembly(Function *F);

        std::set<hakc_function_def_t> GetHAKCFunctions();

        std::set<hakc_transfer_def_t> GetHAKCTransferFunctions();

        std::set<hakc_custom_transfer_def_t> GetHAKCCustomTransferFunctions();
        
        void InitHAKCFunctions();

        void InitAnalysis();

        StringRef HACKCodeAuthenticationName();

        StringRef HAKCDataAuthenticationName();

        StringRef HAKCCompartmentTransferName();

        StringRef HAKCPerCPUCompartmentTransferName();

        StringRef HAKCEntryTokenName();

        Function *GetFunctionByName(StringRef Name, FunctionType *FuncTy);

        FunctionCallee GetFunctionCalleeByName(StringRef Name, FunctionType *FuncTy);

        StructType *GetKernelParamType();

        void generateModuleParamGetCtxFunction(GlobalVariable *GV);

        void transferModuleParams();
    };

} // hakc

#endif //HAKC_HAKCMODULEANALYSIS_H
