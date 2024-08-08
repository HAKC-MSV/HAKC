//
// Created by derrick on 8/20/21.
//
#include <iostream>
#include <sstream>
#include <tuple>
#include <llvm/IR/Verifier.h>

#include "llvm/IR/InstIterator.h"
#include "llvm/Support/Regex.h"
#include "llvm/IR/DIBuilder.h"

#include "HAKCAnalysis/HAKCModuleAnalysis.h"
#include "HAKCAnalysis/HAKCFunctionAnalysis.h"

namespace hakc {

    HAKCModuleAnalysis::HAKCModuleAnalysis(Module &M)
            : CommonHAKCAnalysis(false),
              IsCompartmentalizedAndContainsDebugName(false),
              M(M),
              AnalysisFunctions(),
              transformer(nullptr),
              Transfers(), NonTransferHAKCFunctions() {
    }

    void HAKCModuleAnalysis::InitAnalysis() {
        HAKC_FUNCTION(HAKCDataAuthenticationName());
        HAKC_FUNCTION(HACKCodeAuthenticationName());

        InitHAKCFunctions();
        GetAnalysisFunctions();
    }

    Module &HAKCModuleAnalysis::GetModule() {
        return M;
    }

    std::string
    HAKCModuleAnalysis::getGlobalHAKCSectionName(GlobalVariable *GV, HAKCCompartmentalizationPolicy &Policy) {
        auto Compartment = Policy.GetCompartment(GV);
        if (Compartment.IsKernelCompartment()) {
            return GV->getSection().str();
        }

        std::string finalName = HAKC_SECTION_PREFIX.str();
        finalName += std::to_string(Compartment.GetCompartmentID()->getSExtValue());

        finalName += GV->getSection().str();
        if (GV->getSection().empty()) {
            if (GV->isConstant()) {
                finalName += ".rodata";
            } else {
                finalName += ".data";
            }
        }
        return finalName;
    }

    void HAKCModuleAnalysis::RegisterUsedCompartment(HAKCCompartment &compartment) {
        if (!(compartment.IsKernelCompartment())) {
            UsedCompartments.push_back(compartment);
        }
    }

    /**
        * @brief Moves all global values to the specified HAKC ELF section
        */
    void HAKCModuleAnalysis::MoveGlobalsToHAKCSection(HAKCCompartmentalizationPolicy &Policy) {
        std::set<GlobalVariable *> globalsToChange;

        for (auto *pGlobal: globalsToChange) {
            auto finalName = getGlobalHAKCSectionName(pGlobal, Policy);
            auto compartment = Policy.GetCompartment(pGlobal);
            RegisterUsedCompartment(compartment);

            if (finalName != pGlobal->getSection()) {
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "Changing section of global " << *pGlobal << " to section "
                                                    << finalName << " from " << pGlobal->getSection() << "\n";
                }
                pGlobal->setSection(finalName);
            }
        }
    }

    bool HAKCModuleAnalysis::functionInAnalysisSet(Function *F) {
        auto Search = [F](Function *Func) {
            return F == Func;
        };
        return std::any_of(AnalysisFunctions.begin(), AnalysisFunctions.end(), Search);
    }

    void HAKCModuleAnalysis::GetAnalysisFunctions() {
        for (auto &F: M.getFunctionList()) {
            if (FunctionNeedsAnalysis(&F)) {
                AnalysisFunctions.push_back(&F);
            }
        }
        CommonHAKCAnalysis::SortFunctionList(AnalysisFunctions);
    }

    bool HAKCModuleAnalysis::FunctionNeedsAnalysis(Function *F) {
        bool needsAnalysis = !F->isIntrinsic() &&
                             !F->isDeclaration() &&
                             F->getSubprogram() != nullptr &&
                             !isOutsideTransferFunc(F) &&
                             !isHAKCFunction(F);
        if (!needsAnalysis) {
            goto out;
        }

        for (auto *user: F->users()) {
            if (!isa<CallInst>(user)) {
                /* Function is passed into a global variable */
                needsAnalysis = true;
            }
        }

        out:
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << F->getName();
            if (!needsAnalysis) {
                CommonHAKCAnalysis::getWriter() << " does not need ";
            } else {
                CommonHAKCAnalysis::getWriter() << " needs ";
            }
            CommonHAKCAnalysis::getWriter() << "analysis\n";
        }

        return needsAnalysis;
    }

    Function *HAKCModuleAnalysis::GetFunctionByName(StringRef Name, FunctionType *FuncTy) {
        auto Callee = GetFunctionCalleeByName(Name, FuncTy);
        return dyn_cast<Function>(Callee.getCallee());
    }

    FunctionCallee HAKCModuleAnalysis::GetFunctionCalleeByName(StringRef Name, FunctionType *FuncTy) {
        auto Callee = GetModule().getOrInsertFunction(Name, FuncTy);
        return Callee;
    }

    bool HAKCModuleAnalysis::AliasShouldBeCreated(Function *F, HAKCCompartmentalizationPolicy &Policy) {
        return TransferFunctionShouldBeCreated(F, Policy);
    }

    bool HAKCModuleAnalysis::FunctionDefinedInAssembly(Function *F) {
        StringRef ModuleAsm = GetModule().getModuleInlineAsm();
        std::string SearchTerm = "[[:space:];]+";
        SearchTerm += F->getName().str();
        SearchTerm += ":";
        Regex NameRegex(SearchTerm);
        SmallVector<StringRef, 2> Matches;

        auto NameInAssembly = NameRegex.match(ModuleAsm, &Matches, nullptr);
        if (NameInAssembly && debug_output) {
            CommonHAKCAnalysis::getWriter() << F->getName() << " was found in the Module inline assembly: " <<
                                            Matches[0] << "\n";
        } else if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Could not find " << SearchTerm << " in\n" << ModuleAsm << "\n";
        }
        return NameInAssembly;
    }

    bool _useEscapes(Use &U, std::set<Value *> &expected) {
        /* If F is used in a global variable */
        if (auto *gv = dyn_cast<GlobalVariable>(U.getUser())) {
            return gv->getSection() != ".discard.addressable";
        } else if (isa<ConstantStruct>(U.getUser()) || isa<SelectInst>(U.getUser())) {
            return true;
        } else if (auto *call = dyn_cast<CallInst>(U.getUser())) {
            for (auto &arg: call->args()) {
                if (arg.get() == U.get()) {
                    return true;
                }
            }
        } else if (auto *bc = dyn_cast<BitCastOperator>(U.getUser())) {
            for (Use &u: bc->uses()) {
                if (expected.find(u.get()) != expected.end()) {
                    continue;
                }
                expected.insert(u.get());
                if (_useEscapes(u, expected)) {
                    return true;
                }
            }
        } else if (isa<ICmpInst>(U.getUser())) {
            return false;
        } else if (auto *store = dyn_cast<StoreInst>(U.getUser())) {
            if (store->getValueOperand() == U.get()) {
                return true;
            }
        } else if (auto *phi = dyn_cast<PHINode>(U.getUser())) {
            for (Use &u: phi->uses()) {
                if (expected.find(u.get()) != expected.end()) {
                    continue;
                }
                expected.insert(u.get());
                if (_useEscapes(u, expected)) {
                    return true;
                }
            }
        }

        return false;
    }

    bool in_debug = false;

    bool useEscapes(Use &U) {
        std::set<Value *> examined;
        bool escapes = _useEscapes(U, examined);
        if (in_debug) {
            CommonHAKCAnalysis::getWriter() << "Use ";
            if (auto *f = dyn_cast<Function>(U.get())) {
                CommonHAKCAnalysis::getWriter() << f->getName();
            } else {
                U->print(CommonHAKCAnalysis::getWriter());
            }
            CommonHAKCAnalysis::getWriter() << " in ";
            U.getUser()->print(CommonHAKCAnalysis::getWriter());
            if (escapes) {
                CommonHAKCAnalysis::getWriter() << " escapes\n";
            } else {
                CommonHAKCAnalysis::getWriter() << " does not escape\n";
            }
        }
        return escapes;
    }

    void HAKCModuleAnalysis::TransformModule(HAKCCompartmentalizationPolicy &Policy) {
        MoveGlobalsToHAKCSection(Policy);
        TransformFunctions(Policy);
        AddCompartmentMetadata(Policy);

        CreateInitGlobalMemberTransfers(Policy);
        AddTransferFunctions(Policy);
    }

    void HAKCModuleAnalysis::TransformFunctions(HAKCCompartmentalizationPolicy &Policy) {
        SmallVector<Function *> SortedFunctions(AnalysisFunctions.begin(), AnalysisFunctions.end());
        llvm::sort(SortedFunctions.begin(), SortedFunctions.end(),
                   [](Function *LHS, Function *RHS) { return LHS->getName().str() < RHS->getName().str(); });

        for (auto *F: SortedFunctions) {
            auto *FunctionTransformation = GetFunctionTransformation(F, Policy);
            FunctionTransformation->InstrumentCode(Policy);
            delete FunctionTransformation;
        }
    }

    void HAKCModuleAnalysis::performTransformations() {
        HAKCCompartmentalizationPolicy Policy(M, this);
        const auto *YamlPath = std::getenv(COMPARTMENT_PATH_ENV_VAR.str().c_str());
        Policy.ReadCompartmentalizationPolicy(YamlPath);

        TransformModule(Policy);
        if (IsCompartmentalizedAndContainsDebugName) {
            CommonHAKCAnalysis::getWriter() << "Final Module After Transformations:\n";
            M.print(CommonHAKCAnalysis::getWriter(), nullptr);
            CommonHAKCAnalysis::getWriter() << "\n";
        }
    }

    bool HAKCModuleAnalysis::functionEscapes(Function *F) {
        if (F->isIntrinsic()) {
            return false;
        }
        for (auto &U: F->uses()) {
            if (useEscapes(U)) {
                return true;
            }
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Use ";
                U.getUser()->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " does not escape\n";
            }
        }
        Function *transfer = GetModule().getFunction(getOutsideTransferName(F));
        if (transfer) {
            /* A transfer function reference has been made, so it escapes */
            return true;
        }

        return FunctionIsExported(F) || !isFunctionStatic(F);
    }

    bool HAKCModuleAnalysis::FunctionIsExported(Function *F) {
        return false;
    }

    bool HAKCModuleAnalysis::TransferFunctionShouldBeCreated(Function *F, HAKCCompartmentalizationPolicy &Policy) {
        if (F->isDeclaration()) {
            return false;
        }
        if (CommonHAKCAnalysis::NoKernelTransferFunctionsSet()) {
            auto Compartment = Policy.GetCompartment(F);
            if (Compartment.IsKernelCompartment()) {
                return false;
            }
        }

        return CommonHAKCAnalysis::FunctionHasPointerArg(F);
    }

    void HAKCModuleAnalysis::RegisterCustomTransfer(const hakc_custom_transfer_def_t &CustomTransfer) {
        if (CustomTransfer->GetFunction() == nullptr || CustomTransfer->GetType() == nullptr) {
            return;
        }

        CustomTransfers.push_back(CustomTransfer);
    }

    void HAKCModuleAnalysis::RegisterHAKCTransfer(const hakc_transfer_def_t &Transfer) {
        if (Transfer) {
            Transfers.insert(Transfer);
        }
    }

    void HAKCModuleAnalysis::RegisterNonTransferHAKCFunction(const hakc_function_def_t &HAKCFunction) {
        if (HAKCFunction) {
            NonTransferHAKCFunctions.insert(HAKCFunction);
        }
    }

    std::set<hakc_function_def_t> HAKCModuleAnalysis::GetHAKCFunctions() {
        std::set<hakc_function_def_t> AllFunctions;
        for (auto &p: NonTransferHAKCFunctions) {
            AllFunctions.insert(p);
        }
        for (auto &p: GetHAKCTransferFunctions()) {
            AllFunctions.insert(p);
        }
        return AllFunctions;
    }

    std::set<hakc_transfer_def_t> HAKCModuleAnalysis::GetHAKCTransferFunctions() {
        std::set<hakc_transfer_def_t> TransferFunctions;
        for (auto &p: Transfers) {
            TransferFunctions.insert(p);
        }
        for (auto &p: GetHAKCCustomTransferFunctions()) {
            TransferFunctions.insert(p);
        }
        return TransferFunctions;
    }

    std::set<hakc_custom_transfer_def_t> HAKCModuleAnalysis::GetHAKCCustomTransferFunctions() {
        std::set<hakc_custom_transfer_def_t> TransferFunctions;
        for (auto &p: CustomTransfers) {
            TransferFunctions.insert(p);
        }
        return TransferFunctions;
    }

    void HAKCModuleAnalysis::AddTransferFunctions(HAKCCompartmentalizationPolicy &Policy) {
        std::vector<Function *> FuncsNeedingTransfers;
        for (auto &F: GetModule().getFunctionList()) {
            if (functionIsTransferCandidate(&F)) {
                FuncsNeedingTransfers.push_back(&F);
            }
        }
        SortFunctionList(FuncsNeedingTransfers);
        for (auto *Funcp: FuncsNeedingTransfers) {
            Function &F = *Funcp;
            debug_output = (F.getName() == getHAKCDebugName());
            if (!isOutsideTransferFunc(&F) && functionEscapes(&F)) {
                std::vector<Value *> arguments;
                std::vector<std::tuple<Value *, CallInst *, Value *>> argsToRestore;
                Function *transferFunc = nullptr;

                if (!CommonHAKCAnalysis::FunctionHasPointerArg(&F)) {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Function " << F.getName() << " has no pointer arguments. "
                                                        << "Skipping transfer function creation.\n";
                    }
                    continue;
                }

                auto RawCompartmentID = getTransformer(Policy).getFunctionCompartmentID(&F);
                auto *compartmentId = getTransformer(Policy).GetHAKCCompartmentValue(RawCompartmentID);

                if (RawCompartmentID < 0) {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Could not find compartment ID. Using kernel defaults\n";
                    }
                    compartmentId = getTransformer(Policy).GetHAKCCompartmentValue(KERNEL_COMPARTMENT);
                }

                if (functionIsTransferCandidate(&F)) {
                    transferFunc = getTransformer(Policy).CreateTransferFunction(&F);
                    if (!transferFunc) {
                        CommonHAKCAnalysis::getWriter() << "Could not create transfer for " << F.getName() << "\n";
                        throw std::exception();
                    }
                    bool TransferAlreadyExisted = (transferFunc->getInstructionCount() > 0 && !F.isDeclaration());
                    if (debug_output) {
                        if (TransferAlreadyExisted) {
                            CommonHAKCAnalysis::getWriter() << "Retrieved transfer function " <<
                                                            transferFunc->getName();
                        } else {
                            CommonHAKCAnalysis::getWriter() << "Created transfer function " << transferFunc->getName();
                        }
                        CommonHAKCAnalysis::getWriter() << " in compartment " << *compartmentId << "\n";
                        if (!TransferAlreadyExisted) {
                            CommonHAKCAnalysis::getWriter() << *transferFunc << "\n";
                        }
                    }

                    if (F.isDeclaration() && debug_output) {
                        CommonHAKCAnalysis::getWriter() << F.getName() << " is a declaration\n";
                    }
                } else if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "No transfer created for " << F.getName() << "\n";
                }
                if (!transferFunc) {
                    transferFunc = GetFunctionByName(getOutsideTransferName(&F), F.getFunctionType());
                    /* Ensure that transfer functions not defined here are treated
                    * the same as the function we are replacing */
                    transferFunc->setLinkage(F.getLinkage());
                    transferFunc->copyAttributesFrom(&F);
                }

                if (valueShouldBeReplacedWithTransfer(&F)) {
                    if (!IsNoTransferFunction(&F)) {
                        if (debug_output) {
                            CommonHAKCAnalysis::getWriter() << "Replacing uses of " << F.getName()
                                                            << " with " << transferFunc->getName() << "\n";
                        }
                        in_debug = debug_output;
                        F.replaceUsesWithIf(transferFunc, useEscapes);
                        if (debug_output) {
                            CommonHAKCAnalysis::getWriter() << "Done\n";
                            GetModule().print(CommonHAKCAnalysis::getWriter(), nullptr);
                            CommonHAKCAnalysis::getWriter() << "\n";
                        }
                    }
                }
                if (AliasShouldBeCreated(&F, Policy)) {
                    auto OrigName = F.getName().str();
                    auto NewName = CommonHAKCAnalysis::getOriginalTransformedName(&F);
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Changing name from " << F.getName()
                                                        << " to " << NewName << "\n";
                    }
                    F.setName(NewName);
                    auto *alias = GlobalAlias::create(OrigName, transferFunc);
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Final Transfer:\n" << *transferFunc << "\nAlias: " << *alias
                                                        << "\n";
                    }
                    auto *OrigSP = F.getSubprogram();
                    if (OrigSP) {
                        DIBuilder DIB(*F.getParent(), false, OrigSP->getUnit());
                        DISubprogram::DISPFlags SPFlags = DISubprogram::SPFlagDefinition |
                                                          DISubprogram::SPFlagOptimized |
                                                          DISubprogram::SPFlagLocalToUnit;
                        auto NewSP = DIB.createFunction(OrigSP->getScope(), transferFunc->getName(),
                                                        transferFunc->getName(),
                                                        OrigSP->getFile(), 0, OrigSP->getType(), 0, DINode::FlagZero,
                                                        SPFlags);
                        transferFunc->setSubprogram(NewSP);
                    }
                }
            } else if (debug_output) {
                CommonHAKCAnalysis::getWriter() << F.getName() << " does not escape\n";
            }
        }
    }

//    void HAKCModuleAnalysis::compartmentalizeModule() {
//        SmallVector<Function *> SortedFunctions(AnalysisFunctions.begin(), AnalysisFunctions.end());
//        llvm::sort(SortedFunctions.begin(), SortedFunctions.end(),
//                   [](Function *LHS, Function *RHS) { return LHS->getName().str() < RHS->getName().str(); });
//
//        for (auto *F: SortedFunctions) {
//            debug_output = (F->getName() == getHAKCDebugName());
//            if (debug_output) {
//                IsCompartmentalizedAndContainsDebugName = true;
//            }
//            CompartmentalizeFunction(F);
//        }
//        AddCompartmentMetadata();
//
//        TransferModuleParams();
//
//        CreateInitGlobalMemberTransfers();
//    }

    bool HAKCModuleAnalysis::ConstantStructTransferIsNeeded(ConstantStruct *ConstStruct,
                                                            HAKCCompartmentalizationPolicy &Policy) {
        bool Result = false;
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "TransferIsNeeded Checking " << *ConstStruct << "\n";
        }
        for (auto &Member: ConstStruct->operands()) {
            auto *Def = getDef(Member.get(), false, debug_output);
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Checking struct member " << *Member << " with Def " << *Def << "\n";
            }
            if (isa<ConstantPointerNull>(Def)) {
                continue;
            }
            if (auto *GlobalVal = dyn_cast<GlobalValue>(Def)) {
                Result = !IsKernelSymbol(GlobalVal, Policy);
            } else if (auto *StructMember = dyn_cast<ConstantStruct>(Def)) {
                Result = ConstantStructTransferIsNeeded(StructMember, Policy);
            }

            if (Result) {
                break;
            }
        }

        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " Result: " << std::to_string(Result) << "\n";
        }
        return Result;
    }

    bool HAKCModuleAnalysis::TransferIsNeeded(GlobalVariable *GlobalVar, HAKCCompartmentalizationPolicy &Policy) {
        bool IsKernelSym = IsKernelSymbol(GlobalVar, Policy);
        bool Result = GlobalVar->hasInitializer() && !IsKernelSym;
        if (Result) {
            if (auto *ConstStruct = dyn_cast<ConstantStruct>(GlobalVar->getInitializer())) {
                Result = ConstantStructTransferIsNeeded(ConstStruct, Policy);
            } else {
                if (auto *GlobalVal = dyn_cast<GlobalValue>(GlobalVar->getInitializer())) {
                    Result = !IsKernelSym;
                } else if (isa<ConstantPointerNull>(GlobalVar->getInitializer())) {
                    Result = false;
                } else {
                    Result = IsPointerLikeType(GlobalVar->getInitializer()->getType());
                }
            }
        }

        return Result;
    }

    void HAKCModuleAnalysis::CreateInitGlobalMemberTransfers(HAKCCompartmentalizationPolicy &Policy) {
        std::vector<GlobalVariable *> GlobalsToModifyDuringInit;
        for (auto &GV: M.getGlobalList()) {
            if (TransferIsNeeded(&GV, Policy)) {
                GlobalsToModifyDuringInit.push_back(&GV);
            }
        }
        SortGlobalList(GlobalsToModifyDuringInit);

        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Creating Init transfer functions for "
                                            << std::to_string(GlobalsToModifyDuringInit.size()) << " globals:\n";
            for (auto *GlobToTransfer: GlobalsToModifyDuringInit) {
                CommonHAKCAnalysis::getWriter() << GlobToTransfer->getName() << "\n";
            }
        }

        for (auto *GlobToTransfer: GlobalsToModifyDuringInit) {
            auto *InitTransfer = CreateInitTransfer(GlobToTransfer, Policy);
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Created InitTransfer " << InitTransfer->getName() << "\n";
            }
        }
    }

    StringRef HAKCModuleAnalysis::GlobalInitTransferPrefix() const {
        return "hakc_glob_init_xfer_";
    }


    Function *
    HAKCModuleAnalysis::CreateInitTransfer(GlobalVariable *GlobalVar, HAKCCompartmentalizationPolicy &Policy) {
        if (!GlobalVar->hasInitializer()) {
            CommonHAKCAnalysis::PrettyPrintValue(GlobalVar, CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " has no initializer\n";
            throw std::exception();
        }

        SmallString<128> FunctionName = GlobalInitTransferPrefix();

        for (auto letter: GlobalVar->getName()) {
            if (letter != '@') {
                FunctionName += letter;
            }
        }

        auto *GlobalTransferTy = FunctionType::get(Type::getVoidTy(M.getContext()), {});
        auto *GlobalInitFunc = GetFunctionByName(FunctionName, GlobalTransferTy);
        if (!GlobalInitFunc) {
            CommonHAKCAnalysis::getWriter() << "Could not get Global Transfer function " << FunctionName << "\n";
            throw std::exception();
        }

        if (GlobalInitFunc->empty()) {
            PopulateGlobalInitTransferFunc(GlobalInitFunc, GlobalVar, Policy);
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Finished Populating Global Init Transfer\n";
                GlobalInitFunc->print(CommonHAKCAnalysis::getWriter(), nullptr);
                CommonHAKCAnalysis::getWriter() << "\n";
            }
        }

        return GlobalInitFunc;
    }

    StringRef HAKCModuleAnalysis::GlobalInitTransferSectionName() const {
        return ".hakc.glob_init.text";
    }

    StringRef HAKCModuleAnalysis::GlobalInitTransferPointerSectionName() const {
        return ".hakc.global_init.data";
    }

    std::string
    HAKCModuleAnalysis::GlobalVariableROSectionName(GlobalVariable *GlobalVar, HAKCCompartmentalizationPolicy &Policy) {
        auto Compartment = Policy.GetCompartment(GlobalVar);
        std::string SectionName = ".hakc.";
        SectionName += std::to_string(Compartment.GetCompartmentIDValue());
        SectionName += ".ro_data";

        return SectionName;
    }

    void HAKCModuleAnalysis::PopulateGlobalInitTransferFunc(Function *GlobTransfer, GlobalVariable *GlobalVar,
                                                            HAKCCompartmentalizationPolicy &Policy) {
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Populating Global Init Transfer Function " << GlobTransfer->getName()
                                            << "\n";
        }

        GlobTransfer->setSection(GlobalInitTransferSectionName());
        if (!GlobTransfer->empty()) {
            return;
        }

        if (GlobalVar->isConstant()) {
            GlobalVar->setConstant(false);
            GlobalVar->setSection(GlobalVariableROSectionName(GlobalVar, Policy));
        }

        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Starting Global Init Population\n";
        }
        getTransformer(Policy).PopulateGlobalTransfer(GlobTransfer, GlobalVar, debug_output);
        if (llvm::verifyFunction(*GlobTransfer, &CommonHAKCAnalysis::getWriter())) {
            CommonHAKCAnalysis::getWriter() << "\nFaulty Global Transfer function "
                                            << GlobTransfer->getName() << "\n";
            GetModule().print(CommonHAKCAnalysis::getWriter(), nullptr);
            throw std::exception();
        }

        auto GlobalTrackerName = GlobTransfer->getName() + "_loc";
        auto *TransferPointer = dyn_cast<GlobalVariable>(
                M.getOrInsertGlobal(GlobalTrackerName.getSingleStringRef(), GlobTransfer->getType()));
        TransferPointer->setConstant(true);
        TransferPointer->setInitializer(GlobTransfer);
        TransferPointer->setSection(GlobalInitTransferPointerSectionName());
    }

    void HAKCModuleAnalysis::AddCompartmentMetadata(HAKCCompartmentalizationPolicy &Policy) {
        for (auto Compartment: UsedCompartments) {
            getTransformer(Policy).AddCompartmentMetadataEntry(Compartment);
        }
    }

    HAKCTransformer &HAKCModuleAnalysis::getTransformer(HAKCCompartmentalizationPolicy &Policy) {
        if (!transformer) {
            transformer = CreateTransformer(Policy);
        }
        return *transformer;
    }

//    bool HAKCModuleAnalysis::isModuleTransformed() {
//        return ModuleModified;
//    }

//    void HAKCModuleAnalysis::CompartmentalizeFunction(Function *F) {
//        if (debug_output) {
//            CommonHAKCAnalysis::getWriter() << "Compartmentalizing " << F->getName() << "\n";
//        }
//        getTransformer().getSystemInformation().SetDebugActive(debug_output);
//        auto compartment = getTransformer().getFunctionCompartmentID(F);
//        RegisterUsedCompartment(compartment);
//
//        HAKCFunctionAnalysis *functionAnalysis = GetFunctionTransformation(F);
//        functionAnalysis->InstrumentCode();
//        ModuleModified |= functionAnalysis->modifiedFunction();
//
//        totalCodeChecks += functionAnalysis->GetCodeAuthenticationCount();
//        totalDataChecks += functionAnalysis->GetDataAuthenticationCount();
//        totalTransfers += functionAnalysis->GetCompartmentTransferCount();
//
//        if (debug_output) {
//            CommonHAKCAnalysis::getWriter() << "Finished Compartmentalizing " << F->getName() << "\n";
//        }
//
//        getTransformer().getSystemInformation().SetDebugActive(false);
//        delete functionAnalysis;
//    }

    std::set<StringRef> HAKCModuleAnalysis::GetSafeTransitionFunctions() {
        std::set<StringRef> base =
                {
                        "strlen",
                        "strnlen",
                        "memcpy",
                        "memcmp",
                        "strcmp",
                        "strncmp",
                        "memchr",
                        "memmove",
                        "strchr",
                        "strrchr",
                        "memset",
                };
        auto additions = GetSafeTransitionFunctions_Arch();
        return AddToSet(base, additions);
    }

    StringRef hakc::HAKCModuleAnalysis::HAKCDataAuthenticationName() {
        return "check_hakc_data_access";
    }

    StringRef hakc::HAKCModuleAnalysis::HACKCodeAuthenticationName() {
        return "check_hakc_code_access";
    }

    StringRef hakc::HAKCModuleAnalysis::HAKCCompartmentTransferName() {
        return "hakc_transfer_to_clique";
    }

    StringRef HAKCModuleAnalysis::HAKCSignWithDivisionName() {
        return "hakc_sign_pointer_with_color";
    }

    StringRef hakc::HAKCModuleAnalysis::HAKCPerCPUCompartmentTransferName() {
        return "hakc_transfer_percpu_to_clique";
    }

    StringRef hakc::HAKCModuleAnalysis::HAKCEntryTokenName() {
        return "HAKCEntryToken";
    }
}// namespace hakc
