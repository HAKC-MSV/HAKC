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
#include "HAKCSystemInformation.h"

namespace hakc {

    HAKCModuleAnalysis::HAKCModuleAnalysis(Module &M)
            : CommonHAKCAnalysis(false),
              moduleModified(false),
              IsCompartmentalizedAndContainsDebugName(false),
              M(M),
              AnalysisFunctions(),
              transformer(nullptr),
              Transfers(), NonTransferHAKCFunctions(),
              totalDataChecks(0),
              totalCodeChecks(0), totalTransfers(0) {
    }

    void HAKCModuleAnalysis::InitAnalysis() {
        HAKC_FUNCTION(HAKCDataAuthenticationName());
        HAKC_FUNCTION(HACKCodeAuthenticationName());

        InitHAKCFunctions();
        GetAnalysisFunctions();
    }

    std::string HAKCModuleAnalysis::getGlobalHAKCSectionName(GlobalVariable *GV) {
        std::string finalName = HAKC_SECTION_PREFIX.str();
        auto compartmentId = getTransformer().getGlobalCompartmentID(GV);
        finalName += std::to_string(compartmentId);

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

    void HAKCModuleAnalysis::registerUsedCompartment(int64_t compartment) {
        if (compartment >= 0)
            used_compartments.insert(compartment);
    }

    /**
        * @brief Moves all global values to the specified HAKC ELF section
        */
    void HAKCModuleAnalysis::moveGlobalsToPMCSection() {
        std::set < GlobalVariable * > globalsToChange;

        for (auto *pGlobal: globalsToChange) {
            auto &global = *pGlobal;
            auto finalName = getGlobalHAKCSectionName(pGlobal);
            auto compartment = getTransformer().getGlobalCompartmentID(pGlobal);
            registerUsedCompartment(compartment);

            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Changing section of global ";
                global.print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " to section " << finalName << " from " << global.getSection()
                                                << "\n";
            }

            global.setSection(finalName);
            moduleModified = true;
        }
    }

    bool HAKCModuleAnalysis::functionInAnalysisSet(Function *F) {
        return AnalysisFunctions.find(F) != AnalysisFunctions.end();
    }

    void HAKCModuleAnalysis::GetAnalysisFunctions() {
        for (auto &F: M.getFunctionList()) {
            if (FunctionNeedsAnalysis(&F)) {
                AnalysisFunctions.insert(&F);
            }
        }
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
        auto Callee = getModule().getOrInsertFunction(Name, FuncTy);
        return Callee;
    }

    bool HAKCModuleAnalysis::isModuleCompartmentalized() {
        return getTransformer().getSystemInformation().ContainsCompartmentalizedSymbols();
    }

    bool HAKCModuleAnalysis::AliasShouldBeCreated(Function *F) {
        return TransferFunctionShouldBeCreated(F);
    }

    bool HAKCModuleAnalysis::FunctionDefinedInAssembly(Function *F) {
        StringRef ModuleAsm = getModule().getModuleInlineAsm();
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
        std::set < Value * > examined;
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

    void HAKCModuleAnalysis::performTransformations() {
        if (isModuleCompartmentalized()) {
            compartmentalizeModule();
        } else {
            removeSignatures();
        }

        addTransferFunctions();
        if(IsCompartmentalizedAndContainsDebugName) {
            CommonHAKCAnalysis::getWriter() << "Final Module After Transformations:\n";
            M.print(CommonHAKCAnalysis::getWriter(), nullptr);
            CommonHAKCAnalysis::getWriter() << "\n";
        }
    }

    void HAKCModuleAnalysis::removeSignatures() {
        for (auto *F: AnalysisFunctions) {
            debug_output = F->getName() == getHAKCDebugName();
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Removing signatures in " << F->getName() << "\n";
                F->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            HAKCFunctionAnalysis *functionAnalysis = GetFunctionTransformation(F);
            functionAnalysis->InstrumentKernelCode();
            moduleModified |= functionAnalysis->modifiedFunction();
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Completed signature removal in " << F->getName() << "\n";
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            delete functionAnalysis;
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
        Function *transfer = getModule().getFunction(getOutsideTransferName(F));
        if (transfer) {
            /* A transfer function reference has been made, so it escapes */
            return true;
        }

        return functionIsExported(F) || !isFunctionStatic(F);
    }

    bool HAKCModuleAnalysis::functionIsExported(Function *F) {
        return getTransformer().FunctionIsExported(F);
    }

    bool HAKCModuleAnalysis::TransferFunctionShouldBeCreated(Function *F) {
        if (F->isDeclaration()) {
            return false;
        }
        if (CommonHAKCAnalysis::NoKernelTransferFunctionsSet()) {
            auto CompartmentID = getTransformer().getFunctionCompartmentID(F);
            if (CommonHAKCAnalysis::IsKernelCompartment(CompartmentID)) {
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

    std::set <hakc_function_def_t> HAKCModuleAnalysis::GetHAKCFunctions() {
        std::set <hakc_function_def_t> AllFunctions;
        for (auto &p: NonTransferHAKCFunctions) {
            AllFunctions.insert(p);
        }
        for (auto &p: GetHAKCTransferFunctions()) {
            AllFunctions.insert(p);
        }
        return AllFunctions;
    }

    std::set <hakc_transfer_def_t> HAKCModuleAnalysis::GetHAKCTransferFunctions() {
        std::set <hakc_transfer_def_t> TransferFunctions;
        for (auto &p: Transfers) {
            TransferFunctions.insert(p);
        }
        for (auto &p: GetHAKCCustomTransferFunctions()) {
            TransferFunctions.insert(p);
        }
        return TransferFunctions;
    }

    std::set <hakc_custom_transfer_def_t> HAKCModuleAnalysis::GetHAKCCustomTransferFunctions() {
        std::set <hakc_custom_transfer_def_t> TransferFunctions;
        for (auto &p: CustomTransfers) {
            TransferFunctions.insert(p);
        }
        return TransferFunctions;
    }

    void HAKCModuleAnalysis::addTransferFunctions() {
        std::vector < Function * > FuncsNeedingTransfers;
        for (auto &F: getModule().getFunctionList()) {
            if (functionIsTransferCandidate(&F)) {
                FuncsNeedingTransfers.push_back(&F);
            }
        }
        for (auto *Funcp: FuncsNeedingTransfers) {
            Function &F = *Funcp;
            debug_output = (F.getName() == getHAKCDebugName());
            if (!isOutsideTransferFunc(&F) && functionEscapes(&F)) {
                std::vector < Value * > arguments;
                std::vector <std::tuple<Value *, CallInst *, Value *>> argsToRestore;
                Function *transferFunc = nullptr;

                if (!CommonHAKCAnalysis::FunctionHasPointerArg(&F)) {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Function " << F.getName() << " has no pointer arguments. "
                                                        << "Skipping transfer function creation.\n";
                    }
                    continue;
                }

                auto RawCompartmentID = getTransformer().getFunctionCompartmentID(&F);
                auto *compartmentId = getTransformer().GetHAKCCompartmentValue(RawCompartmentID);

                if (RawCompartmentID < 0) {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Could not find compartment ID. Using kernel defaults\n";
                    }
                    compartmentId = getTransformer().GetHAKCCompartmentValue(KERNEL_COMPARTMENT);
                }

                if (functionIsTransferCandidate(&F)) {
                    transferFunc = getTransformer().CreateTransferFunction(&F);
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
                        CommonHAKCAnalysis::getWriter() << " in compartment ";
                        compartmentId->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                        if (!TransferAlreadyExisted) {
                            transferFunc->print(CommonHAKCAnalysis::getWriter());
                            CommonHAKCAnalysis::getWriter() << "\n";
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
                            getModule().print(CommonHAKCAnalysis::getWriter(), nullptr);
                            CommonHAKCAnalysis::getWriter() << "\n";
                        }
                    }
                }
                if (AliasShouldBeCreated(&F)) {
                    auto OrigName = F.getName().str();
                    auto NewName = CommonHAKCAnalysis::getOriginalTransformedName(&F);
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Changing name from " << F.getName()
                                                        << " to " << NewName << "\n";
                    }
                    F.setName(NewName);
                    auto *alias = GlobalAlias::create(OrigName, transferFunc);
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Final Transfer:\n";
                        transferFunc->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                        CommonHAKCAnalysis::getWriter() << "Alias: ";
                        alias->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
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

    void HAKCModuleAnalysis::compartmentalizeModule() {
        if (!isModuleCompartmentalized()) {
            return;
        }

        moveGlobalsToPMCSection();

        SmallVector < Function * > SortedFunctions(AnalysisFunctions.begin(), AnalysisFunctions.end());
        llvm::sort(SortedFunctions.begin(), SortedFunctions.end(),
                   [](Function *LHS, Function *RHS) { return LHS->getName().str() < RHS->getName().str(); });

        for (auto *F: SortedFunctions) {
            debug_output = (F->getName() == getHAKCDebugName());
            if(debug_output) {
                IsCompartmentalizedAndContainsDebugName = true;
            }
            CompartmentalizeFunction(F);
        }
        addCompartmentMetadata();

        transferModuleParams();

        CreateInitGlobalMemberTransfers();
    }

    bool HAKCModuleAnalysis::ConstantStructTransferIsNeeded(ConstantStruct *ConstStruct) {
        bool Result = false;
        if(debug_output) {
            CommonHAKCAnalysis::getWriter() << "TransferIsNeeded Checking " << *ConstStruct << "\n";
        }
        for (auto &Member: ConstStruct->operands()) {
            auto *Def = getDef(Member.get(), false, debug_output);
            if(debug_output) {
                CommonHAKCAnalysis::getWriter() << "Checking struct member " << *Member << " with Def " << *Def << "\n";
            }
            if (isa<ConstantPointerNull>(Def)) {
                continue;
            }
            if (auto *GlobalVal = dyn_cast<GlobalValue>(Def)) {
                Result = !IsKernelSymbol(GlobalVal);
            } else if (auto *StructMember = dyn_cast<ConstantStruct>(Def)) {
                Result = ConstantStructTransferIsNeeded(StructMember);
            }

            if (Result) {
                break;
            }
        }

        if(debug_output) {
            CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " Result: " << std::to_string(Result) << "\n";
        }
        return Result;
    }

    bool HAKCModuleAnalysis::TransferIsNeeded(GlobalVariable *GlobalVar) {
        bool Result = GlobalVar->hasInitializer() && !IsKernelSymbol(GlobalVar);
        if (Result) {
            if (auto *ConstStruct = dyn_cast<ConstantStruct>(GlobalVar->getInitializer())) {
                Result = ConstantStructTransferIsNeeded(ConstStruct);
            } else {
                if (auto *GlobalVal = dyn_cast<GlobalValue>(GlobalVar->getInitializer())) {
                    Result = !IsKernelSymbol(GlobalVal);
                } else if(isa<ConstantPointerNull>(GlobalVar->getInitializer())) {
                    Result = false;
                } else {
                    Result = IsPointerLikeType(GlobalVar->getInitializer()->getType());
                }
            }
        }

        return Result;
    }

    void HAKCModuleAnalysis::CreateInitGlobalMemberTransfers() {
        std::set < GlobalVariable * > GlobalsToModifyDuringInit;
        for (auto &GV: M.getGlobalList()) {
            if (TransferIsNeeded(&GV)) {
                GlobalsToModifyDuringInit.insert(&GV);
            }
        }

        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Creating Init transfer functions for "
                                            << std::to_string(GlobalsToModifyDuringInit.size()) << " globals:\n";
            for (auto *GlobToTransfer: GlobalsToModifyDuringInit) {
                CommonHAKCAnalysis::getWriter() << GlobToTransfer->getName() << "\n";
            }
        }

        for (auto *GlobToTransfer: GlobalsToModifyDuringInit) {
            auto *InitTransfer = CreateInitTransfer(GlobToTransfer);
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Created InitTransfer " << InitTransfer->getName() << "\n";
            }
        }
    }

    StringRef HAKCModuleAnalysis::GlobalInitTransferPrefix() const {
        return "hakc_glob_init_xfer_";
    }


    Function *HAKCModuleAnalysis::CreateInitTransfer(GlobalVariable *GlobalVar) {
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
            PopulateGlobalInitTransferFunc(GlobalInitFunc, GlobalVar);
            if(debug_output) {
                CommonHAKCAnalysis::getWriter() << "Finished Populating Global Init Transfer\n";
                GlobalInitFunc->print(CommonHAKCAnalysis::getWriter(), nullptr);
                CommonHAKCAnalysis::getWriter() << "\n";
            }
        }

        return GlobalInitFunc;
    }

    StringRef HAKCModuleAnalysis::GlobalInitTransferSectionName() const {
        return ".hakc.global_init";
    }

    std::string HAKCModuleAnalysis::GlobalVariableROSectionName(GlobalVariable *GlobalVar) {
        auto CompartmentID = getTransformer().getGlobalCompartmentID(GlobalVar);
        std::string SectionName = ".hakc.";
        SectionName += std::to_string(CompartmentID);
        SectionName += ".ro_data";

        return SectionName;
    }

    void HAKCModuleAnalysis::PopulateGlobalInitTransferFunc(Function *GlobTransfer, GlobalVariable *GlobalVar) {
        if(debug_output) {
            CommonHAKCAnalysis::getWriter() << "Populating Global Init Transfer Function " << GlobTransfer->getName() << "\n";
        }

        GlobTransfer->setSection(GlobalInitTransferSectionName());
        if (!GlobTransfer->empty()) {
            return;
        }

        if (GlobalVar->isConstant()) {
            GlobalVar->setConstant(false);
            GlobalVar->setSection(GlobalVariableROSectionName(GlobalVar));
        }

        if(debug_output) {
            CommonHAKCAnalysis::getWriter() << "Starting Global Init Population\n";
        }
        getTransformer().PopulateGlobalTransfer(GlobTransfer, GlobalVar, debug_output);
        if (llvm::verifyFunction(*GlobTransfer, &CommonHAKCAnalysis::getWriter())) {
            CommonHAKCAnalysis::getWriter() << "Faulty Global Transfer function "
                                            << GlobTransfer->getName() << "\n";
            getModule().print(CommonHAKCAnalysis::getWriter(), nullptr);
            throw std::exception();
        }
    }

    void HAKCModuleAnalysis::addCompartmentMetadata() {
        for (auto CompartmentId: used_compartments) {
            getTransformer().AddCompartmentMetadataEntry(CompartmentId);
        }
    }

    HAKCTransformer &HAKCModuleAnalysis::getTransformer() {
        if (!transformer) {
            transformer = CreateTransformer();
        }
        return *transformer;
    }

    bool HAKCModuleAnalysis::isModuleTransformed() {
        return moduleModified;
    }

    void HAKCModuleAnalysis::CompartmentalizeFunction(Function *F) {
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Compartmentalizing " << F->getName() << "\n";
        }
        getTransformer().getSystemInformation().SetDebugActive(debug_output);
        auto compartment = getTransformer().getFunctionCompartmentID(F);
        registerUsedCompartment(compartment);

        HAKCFunctionAnalysis *functionAnalysis = GetFunctionTransformation(F);
        functionAnalysis->InstrumentCompartmentalizedCode();
        moduleModified |= functionAnalysis->modifiedFunction();

        totalCodeChecks += functionAnalysis->GetCodeAuthenticationCount();
        totalDataChecks += functionAnalysis->GetDataAuthenticationCount();
        totalTransfers += functionAnalysis->GetCompartmentTransferCount();

        if(debug_output) {
            CommonHAKCAnalysis::getWriter() << "Finished Compartmentalizing " << F->getName() << "\n";
        }

        getTransformer().getSystemInformation().SetDebugActive(false);
        delete functionAnalysis;
    }

    std::set <StringRef> HAKCModuleAnalysis::GetSafeTransitionFunctions() {
        std::set <StringRef> base =
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

    StringRef HAKCModuleAnalysis::HAKCSignWithColorName() {
        return "hakc_sign_pointer_with_color";
    }

    StringRef hakc::HAKCModuleAnalysis::HAKCPerCPUCompartmentTransferName() {
        return "hakc_transfer_percpu_to_clique";
    }

    StringRef hakc::HAKCModuleAnalysis::HAKCEntryTokenName() {
        return "HAKCEntryToken";
    }

    Module &HAKCModuleAnalysis::GetModule() {
        return M;
    }
}// namespace hakc
