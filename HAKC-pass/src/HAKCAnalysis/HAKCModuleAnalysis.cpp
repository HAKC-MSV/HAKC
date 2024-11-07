//
// Created by derrick on 8/20/21.
//
#include <iostream>
#include <sstream>
#include <tuple>
#include <llvm/IR/Verifier.h>

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Regex.h"
#include "llvm/IR/DIBuilder.h"

#include "HAKCAnalysis/HAKCModuleAnalysis.h"
#include "HAKCFunctionDefinition/HAKCTransferFunction.h"
#include "HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "HAKCSystemInformation.h"
#include <stdio.h>
#include <string.h>
#include <bits/stdc++.h>

// #include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_sk_buff.h"
// #include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_file.h"
// #include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_socket.h"
// #include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_fuse_mount.h"

// #include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_scsi_cmnd.h"
// #include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_usb_device.h"
// #include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_usb_interface.h"
// #include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_us_data.h"

// #include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_scsi_device.h"


namespace hakc {

    HAKCModuleAnalysis::HAKCModuleAnalysis(Module &M)
            : CommonHAKCAnalysis(false),
              IsCompartmentalizedAndContainsDebugName(false),
          M(M),
          AnalysisFunctions(),
          transformer(nullptr),
          Transfers(),
          NonTransferHAKCFunctions(),
          SysInfo(std::make_shared<HAKCSystemInformation>(M))
          {
        InitAnalysis();
    }

    std::shared_ptr<HAKCSystemInformation> HAKCModuleAnalysis::GetSysInfo() {
        return SysInfo;
    }

    Module &HAKCModuleAnalysis::getModule(){
        return M;
    }

    void HAKCModuleAnalysis::InitHAKCFunctions() {
        // HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_sk_buff, M, CommonHAKCAnalysis::getCompartmentStorageSizeInBits());
        // HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_file, M, CommonHAKCAnalysis::getCompartmentStorageSizeInBits());
        // HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_socket, M, CommonHAKCAnalysis::getCompartmentStorageSizeInBits());
        // HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_fuse_mount, M, CommonHAKCAnalysis::getCompartmentStorageSizeInBits());
        // HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_scsi_cmnd, M, CommonHAKCAnalysis::getCompartmentStorageSizeInBits());
        // HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_usb_device, M, CommonHAKCAnalysis::getCompartmentStorageSizeInBits());
        // HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_usb_interface, M, CommonHAKCAnalysis::getCompartmentStorageSizeInBits());
        // HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_us_data, M, CommonHAKCAnalysis::getCompartmentStorageSizeInBits());
        // HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_scsi_device, M, CommonHAKCAnalysis::getCompartmentStorageSizeInBits());

        HAKC_TRANSFER(HAKCCompartmentTransferName(), 2, 3);
        HAKC_TRANSFER(HAKCPerCPUCompartmentTransferName(), 2, 3);
        HAKC_TRANSFER("hakc_sign_pointer_with_color", 1, -1);
        HAKC_TRANSFER("hakc_sign_pointer", 1, 2);

        /* TODO: Make these custom transfer functions */
        HAKC_FUNCTION("hakc_transfer_nla");
        HAKC_FUNCTION("hakc_transfer_string");

        /* I couldn't find these in the kernel source, but they were listed, so I am keeping them. */
        HAKC_FUNCTION("hakc_record_common");
        HAKC_FUNCTION("hakc_transfer_to_destination");
        HAKC_FUNCTION("hakc_restore_original");
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
        auto Compartment = Policy.GetDivision(GV).GetHAKCCompartment();
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
            auto compartment = Policy.GetDivision(pGlobal).GetHAKCCompartment();
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
        for (auto &F : M.getFunctionList()) {
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

        CommonHAKCAnalysis::getWriter() << "in FunctionNeedsAnalysis " << (*(*GetSysInfo()).GetMethods()).size() << "\n";
        // linux
        // if (F->getName().contains("static_branch_")) {
        if (F->getName().contains(StringRef(*(*GetSysInfo()->GetMethods())["FunctionNeedsAnalysis"].begin()))) {
            /* These functions call inline assembly that needs to be
                * constant at compile time, so we can't analyze them.
                * We ensure that any pointer passed to these functions have
                * no signature in argNeedsAnalysis.
                */
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << F->getName() << " does not need analysis\n";
            }
            needsAnalysis = false;
        }

        if (!needsAnalysis) {
            goto out;
        }
        for (auto *user : F->users()) {
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

    bool HAKCModuleAnalysis::isModuleCompartmentalized() {
        return getTransformer().GetSysInfo()->ContainsCompartmentalizedSymbols(getModule());
    }

    bool HAKCModuleAnalysis::AliasShouldBeCreated(Function *F) {
        //         /* See note in HAKCModuleAnalysisLinux::TransferFunctionShouldBeCreated */
        //         if (F->isDeclaration() && FunctionDefinedInAssembly(F)) {
        //             return false;
        //         }
        //         return HAKCModuleAnalysisLinux::AliasShouldBeCreated(F);
        return TransferFunctionShouldBeCreated(F);
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
            for (auto &arg : call->args()) {
                if (arg.get() == U.get()) {
                    return true;
                }
            }
        } else if (auto *bc = dyn_cast<BitCastOperator>(U.getUser())) {
            for (Use &u : bc->uses()) {
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
            for (Use &u : phi->uses()) {
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
            CommonHAKCAnalysis::getWriter() << "Use " << U.get() << " in " << U.getUser();
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
        auto DBPath = CommonHAKCAnalysis::GetDBPath();
        HAKCTypeIdentifier TypeIdentifier(M, this);
        HAKCCompartmentalizationPolicy Policy(debug_output, M.getContext(), KERNEL_COMPARTMENT, KERNEL_DIVISION, DBPath);

        TransformModule(Policy);
        if (IsCompartmentalizedAndContainsDebugName) {
            CommonHAKCAnalysis::getWriter() << "Final Module After Transformations:\n" << M << "\n";
        }
    }

    bool HAKCModuleAnalysis::functionEscapes(Function *F) {
        if (F->isIntrinsic()) {
            return false;
        }
        for (auto &U : F->uses()) {
            if (useEscapes(U)) {
                return true;
            }
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "Use " << U.getUser() << " does not escape\n";
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
            auto Compartment = Policy.GetDivision(F).GetHAKCCompartment();
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
    std::set<std::string> HAKCModuleAnalysis::GetSeparateNamespacePaths() {
        return (*GetSysInfo()->GetMethods())["GetSeparateNamespacePaths"];
    }

    std::set<std::string> HAKCModuleAnalysis::GetHAKCSourcePaths() {
        return (*GetSysInfo()->GetMethods())["GetHAKCSourcePaths"];
    }


    void HAKCModuleAnalysis::RegisterNonTransferHAKCFunction(hakc_function_def_t HAKCFunction) {
        if (HAKCFunction) {
            NonTransferHAKCFunctions.insert(HAKCFunction);
        }
    }

    std::set<hakc_function_def_t> HAKCModuleAnalysis::GetHAKCFunctions() {
        std::set<hakc_function_def_t> AllFunctions;
        for (auto &p : NonTransferHAKCFunctions) {
            AllFunctions.insert(p);
        }
        for (auto &p : GetHAKCTransferFunctions()) {
            AllFunctions.insert(p);
        }
        return AllFunctions;
    }

    std::set<hakc_transfer_def_t> HAKCModuleAnalysis::GetHAKCTransferFunctions() {
        std::set<hakc_transfer_def_t> TransferFunctions;
        for (auto &p : Transfers) {
            TransferFunctions.insert(p);
        }
        for (auto &p : GetHAKCCustomTransferFunctions()) {
            TransferFunctions.insert(p);
        }
        return TransferFunctions;
    }

    std::set<hakc_custom_transfer_def_t> HAKCModuleAnalysis::GetHAKCCustomTransferFunctions() {
        std::set<hakc_custom_transfer_def_t> TransferFunctions;
        for (auto &p : CustomTransfers) {
            TransferFunctions.insert(p);
        }
        return TransferFunctions;
    }

    void HAKCModuleAnalysis::AddTransferFunctions(HAKCCompartmentalizationPolicy &Policy) {
        std::vector<Function *> FuncsNeedingTransfers;
        for (auto &F: GetModule().getFunctionList()) {
            auto Compartment = Policy.GetDivision(&F).GetHAKCCompartment();

            if (!Compartment.IsKernelCompartment() && functionIsTransferCandidate(&F, Policy) &&
                !isOutsideTransferFunc(&F) &&
                functionEscapes(&F)) {
                FuncsNeedingTransfers.push_back(&F);
            }
        }
        SortFunctionList(FuncsNeedingTransfers);
        for (auto *Funcp: FuncsNeedingTransfers) {
            Function &F = *Funcp;
            debug_output = (F.getName() == getHAKCDebugName());
            Function *transferFunc = nullptr;

            if (functionIsTransferCandidate(&F, Policy)) {
                auto Compartment = Policy.GetDivision(&F).GetHAKCCompartment();
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
                    CommonHAKCAnalysis::getWriter() << " in compartment "
                                                    << std::to_string(Compartment.GetCompartmentIDValue()) << "\n";
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

            if (valueShouldBeReplacedWithTransfer(&F, Policy)) {
                if (!IsNoTransferFunction(&F)) {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Replacing uses of " << F.getName()
                                                        << " with " << transferFunc->getName() << "\n";
                    }
                    in_debug = debug_output;
                    F.replaceUsesWithIf(transferFunc, useEscapes);
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Done\n" << GetModule() << "\n";
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
        }
    }

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
                if (isa<GlobalValue>(GlobalVar->getInitializer())) {
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
        for (auto &GV: M.globals()) {
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
            CommonHAKCAnalysis::getWriter() << GlobalVar << " has no initializer\n";
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
                CommonHAKCAnalysis::getWriter() << "Finished Populating Global Init Transfer\n" << *GlobalInitFunc << "\n";
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
        auto Compartment = Policy.GetDivision(GlobalVar).GetHAKCCompartment();
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
        if (llvm::verifyFunction(*GlobTransfer, &CommonHAKCAnalysis::getWriter().GetOS())) {
            CommonHAKCAnalysis::getWriter() << "\nFaulty Global Transfer function "
                                            << GlobTransfer->getName() << "\n" << GetModule();
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
            if (!Compartment.IsKernelCompartment()) {
                getTransformer(Policy).AddCompartmentMetadataEntry(Compartment);
            }
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

    std::set<std::string> HAKCModuleAnalysis::GetSafeTransitionFunctions() {
        return (*GetSysInfo()->GetMethods())["GetSafeTransitionFunctions"];
    }

    std::string hakc::HAKCModuleAnalysis::HAKCDataAuthenticationName() {
        return "check_hakc_data_access";
    }

    std::string hakc::HAKCModuleAnalysis::HACKCodeAuthenticationName() {
        return "check_hakc_code_access";
    }

    std::string hakc::HAKCModuleAnalysis::HAKCCompartmentTransferName() {
        return "hakc_transfer_to_clique";
    }

    StringRef HAKCModuleAnalysis::HAKCSignWithDivisionName() {
        return "hakc_sign_pointer_with_color";
    }

    StringRef hakc::HAKCModuleAnalysis::HAKCPerCPUCompartmentTransferName() {
        return "hakc_transfer_percpu_to_clique";
    }

    std::string hakc::HAKCModuleAnalysis::HAKCEntryTokenName() {
        return "HAKCEntryToken";
    }

    HAKCFunctionAnalysis *HAKCModuleAnalysis::GetFunctionTransformation(Function *F) {
        return new HAKCFunctionAnalysis(F, this, true);
    }

    bool HAKCModuleAnalysis::functionIsTransferCandidate(Function *F) {
        // linux start
        if (F->getName().contains(StringRef(*(*GetSysInfo()->GetMethods())["functionIsTransferCandidate"].begin()))) {
            /* Handle trampolines */
            return false;
        }
        // linux end

        auto NoTransferFuncs = GetNoTransferFunctions();
        return NoTransferFuncs.find(F->getName().str()) == NoTransferFuncs.end() &&
               !F->isDeclaration() &&
               !isCapabilityReassignmentFunc(F) &&
               !FunctionIsComplexVariadic(F) &&
               !functionIsModParamGetCtx(F) &&
               FunctionHasPointerArg(F) &&
               (!isOutsideTransferFunc(F) ||
                !F->hasFnAttribute(Attribute::InlineHint));
    }

    ConstantInt *HAKCModuleAnalysis::getFunctionColor(Function *F) {
        return getSymbolColor(F);
    }

    ConstantInt *HAKCModuleAnalysis::getGlobalColor(GlobalVariable *GV) {
        return getSymbolColor(GV);
    }

    ConstantInt *HAKCModuleAnalysis::GetColorValue(sym_color_t Color) {
        return getTransformer().getInt32(Color);
    }

    ConstantInt *HAKCModuleAnalysis::getSymbolColor(GlobalValue *GV) {
        // linux
        auto Symbol = getTransformer().GetSysInfo()->findSymbol(GV);
        if (!Symbol) {
            CommonHAKCAnalysis::getWriter() << "Could not find color for " << GV->getName() << "\n";
            return GetColorValue(KERNEL_COLOR);
        }
        return GetColorValue(Symbol->getCompartment()->getColor());
    }

    // Get the StructType representing a kernel (module) parameter
    StructType *HAKCModuleAnalysis::GetKernelParamType() {
        // linux
        return llvm::StructType::getTypeByName(M.getContext(), llvm::StringRef("struct.kernel_param"));
    }

    GlobalValue *HAKCModuleAnalysis::ExtractGlobalFromKernelParam(GlobalVariable *GV) {
        // the result of walking through the kernel param struct
        // until we get to the actual global value backing the parameter
        GlobalValue *kernparam;

        StructType *KernelParamType = GetKernelParamType();
        // type not found, just do nothing
        if (!KernelParamType) {
            return nullptr;
        }

        // trying to find globals of type GetKernelParamType()
        if (auto *F = dyn_cast<StructType>(GV->getValueType())) {
            if (!(F->getName().equals(KernelParamType->getName()))) {
                return nullptr;// someone passed us a struct that wasn't a kernel param struct
            }
        } else {
            return nullptr;// this is not good, don't give non-structs to this function
        }

        // we know it is a kernel param now, moving on

        // cast the value into a ConstantStruct so we can pick it apart
        auto *kp_struct = dyn_cast<ConstantStruct>(GV->getInitializer());

        // do we have struct kernel_param kp now
        if (kp_struct) {
            // the anonymous union that holds the Value we actually want
            // is the last element of the struct
            int num_ops = kp_struct->getNumOperands();
            Constant *last_op = kp_struct->getOperand(num_ops - 1);

            // this holds kp->arg
            if (last_op) {
                // cast the union into a ConstantStruct so we can pick it apart
                auto *kparg_union = dyn_cast<ConstantStruct>(last_op);

                if (kparg_union) {
                    // get the only thing in the struct, that's how unions work?
                    // this constant is kp->arg, sort of
                    Constant *kparg_val = kparg_union->getOperand(0);
                    // check that the value in there is a BitCastOperator
                    // it is bit-casting the global that backs the parameter
                    if (auto *kparg_val_bco = dyn_cast<BitCastOperator>(kparg_val)) {
                        // extract the pointer from the BitCastOperator
                        Value *gv_from_bco = kparg_val_bco->getOperand(0);

                        if (!(kernparam = dyn_cast<GlobalValue>(gv_from_bco))) {
                            // if it isn't a global value, that's bad
                            return nullptr;
                        }

                        // now we have kp->arg
                    }
                    // the thing in the union isn't a BitCastOperator, that's bad
                    else {
                        return nullptr;
                    }
                }
                // we couldn't get the union out of the union struct, that's bad
                else {
                    return nullptr;
                }
            }
            // we couldn't get the union struct at all out of the param struct, that's bad
            else {
                return nullptr;
            }
        }
        // we couldn't even get the kernel param struct as a struct, that's bad
        else {
            return nullptr;
        }

        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "processing kernel param\n";
            kernparam->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }

        return kernparam;
    }

    // we generate these for all kernel params, some may go unused by the actual module loader
    // (non-pointer params are ignored by the loader when it comes to transferring)
    void HAKCModuleAnalysis::transferModuleParams() {

        if (!isModuleCompartmentalized()) {
            return;
        }

        StructType *KernelParamType = GetKernelParamType();
        // type not found, just do nothing
        if (!KernelParamType) {
            return;
        }

        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "kernel param type is: \n";
            KernelParamType->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }

        // inspect all globals
        for (auto &Global : M.getGlobalList()) {

            // trying to find globals of type GetKernelParamType()
            if (auto *F = dyn_cast<StructType>(Global.getValueType())) {

                // if true, the type of Global matches GetKernelParamType
                if (F->getName().equals(KernelParamType->getName())) {

                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "found kernel param: \n";
                        Global.print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                    }

                    // generate a GetCtx function for the parameter and update
                    // function pointer array
                    generateModuleParamGetCtxFunction(&Global);
                }
            }
        }
    }

    void HAKCModuleAnalysis::emitModParamGetCtx(GlobalValue *kernparam) {
        // linux
        // type of void*
        PointerType *PointerTy = PointerType::get(IntegerType::get(M.getContext(), 8), 0);

        // two args
        std::vector<Type *> FuncTy_args;
        // first arg points to param
        FuncTy_args.push_back(PointerTy);
        // second arg is int64_t flag (0 to return param's access token, 1 to return param's color)
        FuncTy_args.push_back(IntegerType::get(M.getContext(), 64));

        // type of function that returns int64_t, takes (void *, int64_t)
        FunctionType *FuncTy = FunctionType::get(IntegerType::get(M.getContext(), 64),
                                                 FuncTy_args,
                                                 false);

        // create a function named "hakc_modparam_getctx_paramname"
        auto c = M.getOrInsertFunction(MODPARAM_GETCTX_PREFIX.str() + kernparam->getName().str(),
                                       FuncTy);

        auto *constc = dyn_cast<Constant>(c.getCallee());
        Function *getctx = cast<Function>(constc);
        getctx->setCallingConv(CallingConv::C);

        // put "hakc_modparam_getctx_paramname" in a special text section in the module
        getctx->setSection(HAKC_MODPARAM_TEXT_SECTION);

        Function::arg_iterator args = getctx->arg_begin();
        // param pointer
        Value *pointerArg = args++;
        // 0 to return context, 1 to return color
        Value *returnTypeArg = args++;

        // create entry basic block in our new function
        BasicBlock *block = BasicBlock::Create(M.getContext(), "entry", getctx);
        //
        IRBuilder<> builder(block);
        // constant zero for compare/select
        Value *czero = ConstantInt::get(IntegerType::get(M.getContext(), 64), 0);

        // get HAKC symbol for the kernel parameter Value
        auto Symbol = getTransformer().GetSysInfo()->findSymbol(kernparam);

        // find the color of the HAKC symbol
        ConstantInt *Color;
        if (!Symbol) {
            CommonHAKCAnalysis::getWriter() << "Could not find HAKC Symbol for kernel param global: \n";
            kernparam->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            Color = getTransformer().getInt64(KERNEL_COLOR);
            throw std::exception();
        } else {
            Color = getTransformer().getInt64(Symbol->getCompartment()->getColor());
        }

        // find the compartment ID of the HAKC symbol
        ConstantInt *compartId = getTransformer().GetHAKCCompartmentValue(Symbol->getCompartmentID());

        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "color:\n";
            CommonHAKCAnalysis::getWriter() << getColorStringFromValue(Color) << "\n";

            CommonHAKCAnalysis::getWriter() << "compartment:\n";
            compartId->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }

        // get the access token for HAKC symbol as an int64_t
        auto AccessToken = Symbol->getCompartment()->getAccessToken();
        ConstantInt *tok = builder.getInt64(AccessToken);

        // cast kernparam to a void*
        Value *voidCast;
        auto AddrSpace = getTransformer().GetPointerAddrSpace(kernparam);

        if (kernparam->getType()->isIntegerTy()) {
            voidCast = builder.CreateIntToPtr(kernparam, builder.getInt8PtrTy(AddrSpace));
        } else {
            voidCast = builder.CreateBitCast(kernparam, builder.getInt8PtrTy(AddrSpace));
        }


        // if returnTypeArg == 0, next step will use access token for return value
        // else, use color for return value
        Value *tokEqZero = builder.CreateICmpEQ(returnTypeArg, czero);
        Value *tokColSelect = builder.CreateSelect(tokEqZero, tok, Color);

        // check if the address passed in matches address of kernparam
        Value *pointerArgEq = builder.CreateICmpEQ(pointerArg, voidCast);
        // if it does, return the previously selected token/color
        // it it isn't a match, return zero
        Value *ctxSelect = builder.CreateSelect(pointerArgEq, tokColSelect, czero);
        // function is done
        builder.CreateRet(ctxSelect);

        if (debug_output) {
            getctx->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            CommonHAKCAnalysis::getWriter() << llvm::verifyFunction(*getctx, &CommonHAKCAnalysis::getWriter()) << "\n";
        }

        // generate function pointer and place in modparam fp section
        GlobalVariable *gcfp = new GlobalVariable(M,
                                                  getctx->getType(),
                                                  true,// const
                                                  GlobalValue::ExternalLinkage,
                                                  getctx,
                                                  getctx->getName().str() + "_fp");
        gcfp->setSection(HAKC_MODPARAM_FUNCP_SECTION);
    }

    // takes a KernelParam and generate a function to get the HAKC signing context
    // for the actual backing global variable
    // used to correctly transfer charp parameters
    void HAKCModuleAnalysis::generateModuleParamGetCtxFunction(GlobalVariable *GV) {
        // linux

        GlobalValue *kernparam = ExtractGlobalFromKernelParam(GV);

        if (!kernparam) {
            CommonHAKCAnalysis::getWriter() << "Could not extract global from kernel param: \n";
            GV->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            throw std::exception();
        }

        emitModParamGetCtx(kernparam);
    }

    void HAKCModuleAnalysis::updateCallParameters(std::map<Function *, std::set<CallInst *>> calls_map) {
        // linux
        for (auto &pair : calls_map) {
            Function *F = pair.first;
            debug_output = (F->getName() == getHAKCDebugName());

            for (auto &call : pair.second) {
                auto HAKCTransferFunction = GetHAKCTransferDef(call->getCalledFunction()->getName());
                if (HAKCTransferFunction) {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Updating HAKC call parameters for ";
                        call->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                    }
                    hakc_compartment_id_t id;
                    StringRef transferTargetName = F->getName();
                    if (isOutsideTransferFunc(F)) {
                        transferTargetName = F->getName().substr(OUTSIDE_TRANSFER_PREFIX.size());
                        Function *TransferTarget = M.getFunction(transferTargetName);
                        id = getTransformer().getFunctionCompartmentID(TransferTarget);
                    } else {
                        id = getTransformer().getFunctionCompartmentID(F);
                    }
                    if (id < 0) {
                        CommonHAKCAnalysis::getWriter() << "Could not find Compartment ID for function "
                                                        << transferTargetName << "\n";
                        throw std::exception();
                    }
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Updating index " << std::to_string(HAKCTransferFunction->GetCompartmentIdIdx()) << " (";
                        call->getArgOperand(HAKCTransferFunction->GetCompartmentIdIdx())->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << ") to " << std::to_string(id) << "\n";
                    }
                    call->setArgOperand(HAKCTransferFunction->GetCompartmentIdIdx(),
                                        getTransformer().GetHAKCCompartmentValue(id));

                    if (HAKCTransferFunction->HasColorIdx()) {
                        ConstantInt *color;
                        if (isOutsideTransferFunc(F)) {
                            transferTargetName = F->getName().substr(OUTSIDE_TRANSFER_PREFIX.size());
                            auto *TransferTarget = getModule().getFunction(transferTargetName);
                            color = getSymbolColor(TransferTarget);
                        } else {
                            color = getFunctionColor(F);
                        }

                        if (!color) {
                            CommonHAKCAnalysis::getWriter() << "Could not find Color for function " << F->getName()
                                                            << "\n";
                            throw std::exception();
                        }
                        call->setArgOperand(HAKCTransferFunction->GetColorIdx(), color);
                    }
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "After update call is ";
                        call->print(CommonHAKCAnalysis::getWriter());
                        CommonHAKCAnalysis::getWriter() << "\n";
                    }
                }
            }
        }
    }

    std::map<std::string, HAKCAllocationSize> HAKCModuleAnalysis::GetKernelAllocationSizeMap() {
        // linux
        CommonHAKCAnalysis::getWriter() << "METHODS size: GetKernelAllocationSizeMap " << GetSysInfo()->KernelAllocationSizeMap.size() << "\n";
        return GetSysInfo()->KernelAllocationSizeMap;
    }

    std::set<std::string> HAKCModuleAnalysis::GetIgnoredGlobals() {
        CommonHAKCAnalysis::getWriter() << "METHODS size: GetIgnoredGlobals " << (*GetSysInfo()->GetMethods())["GetIgnoredGlobals"].size() << "\n";
        return (*GetSysInfo()->GetMethods())["GetIgnoredGlobals"];
    }

    bool HAKCModuleAnalysis::valueIsReadonlyPtr(Value *value) {
        // linux
        bool result = CommonHAKCAnalysis::valueIsReadonlyPtr(value);
        if (!result) {
            if (auto *callInst = dyn_cast<CallInst>(value)) {
                /* We may have done some global transfer beforehand, so check for that */
                if (callInst->getCalledFunction() && callInst->getCalledFunction()->getName() == "hakc_sign_pointer_with_color") {
                    auto *isCode = dyn_cast<ConstantInt>(
                            callInst->getArgOperand(callInst->getNumArgOperands() - 1));
                    result = isCode->isOne();
                }
            }
        }

        return result;
    }

    std::set<std::string> HAKCModuleAnalysis::GetIgnoredTypes() {
        return (*GetSysInfo()->GetMethods())["GetIgnoredTypes"];
    }

    std::set<std::string> HAKCModuleAnalysis::GetNoTransferFunctions() {
        return (*GetSysInfo()->GetMethods())["GetNoTransferFunctions"];
    }

    sym_color_t HAKCModuleAnalysis::GetMajoritySymbolColor() {
        // linux
        if (!MajorityColorSet) {
            std::map<ConstantInt *, unsigned> ColorCounts;
            std::set<GlobalValue *> symbols;
            for (auto &Global : M.getGlobalList()) {
                symbols.insert(&Global);
            }
            for (auto &F : M.getFunctionList()) {
                symbols.insert(&F);
            }

            for (auto *GV : symbols) {
                auto color = getSymbolColor(GV);
                if (!color->equalsInt(hakc::KERNEL_COLOR)) {
                    if (ColorCounts.find(color) == ColorCounts.end()) {
                        ColorCounts[color] = 0;
                    }
                    ColorCounts[color] += 1;
                }
            }

            unsigned MaxSymbolCount = 0;
            for (auto &it : ColorCounts) {
                if (it.second > MaxSymbolCount) {
                    MajorityColor = getColorFromValue(it.first);
                }
            }
            MajorityColorSet = true;
        }

        return MajorityColor;
    }

    std::string HAKCModuleAnalysis::getColorStringFromValue(ConstantInt *color) {
        switch (color->getZExtValue()) {
            case SILVER_CLIQUE:
                return "SILVER_CLIQUE";
            case GREEN_CLIQUE:
                return "GREEN_CLIQUE";
            case RED_CLIQUE:
                return "RED_CLIQUE";
            case ORANGE_CLIQUE:
                return "ORANGE_CLIQUE";
            case YELLOW_CLIQUE:
                return "YELLOW_CLIQUE";
            case PURPLE_CLIQUE:
                return "PURPLE_CLIQUE";
            case BLUE_CLIQUE:
                return "BLUE_CLIQUE";
            case GREY_CLIQUE:
                return "GREY_CLIQUE";
            case PINK_CLIQUE:
                return "PINK_CLIQUE";
            case BROWN_CLIQUE:
                return "BROWN_CLIQUE";
            case WHITE_CLIQUE:
                return "WHITE_CLIQUE";
            case BLACK_CLIQUE:
                return "BLACK_CLIQUE";
            case TEAL_CLIQUE:
                return "TEAL_CLIQUE";
            case VIOLET_CLIQUE:
                return "VIOLET_CLIQUE";
            case CRIMSON_CLIQUE:
                return "CRIMSON_CLIQUE";
            case GOLD_CLIQUE:
                return "GOLD_CLIQUE";
            case NO_CLIQUE:
                return "NO_CLIQUE";
            default:
                CommonHAKCAnalysis::getWriter() << "number " << color->getZExtValue() << "isn't a valid color\n";
                return "INVALID_CLIQUE";
        }
    }

    sym_color_t HAKCModuleAnalysis::getColorFromValue(ConstantInt *Color) {
        switch (Color->getZExtValue()) {
            case SILVER_CLIQUE:
                return SILVER_CLIQUE;
            case GREEN_CLIQUE:
                return GREEN_CLIQUE;
            case RED_CLIQUE:
                return RED_CLIQUE;
            case ORANGE_CLIQUE:
                return ORANGE_CLIQUE;
            case YELLOW_CLIQUE:
                return YELLOW_CLIQUE;
            case PURPLE_CLIQUE:
                return PURPLE_CLIQUE;
            case BLUE_CLIQUE:
                return BLUE_CLIQUE;
            case GREY_CLIQUE:
                return GREY_CLIQUE;
            case PINK_CLIQUE:
                return PINK_CLIQUE;
            case BROWN_CLIQUE:
                return BROWN_CLIQUE;
            case WHITE_CLIQUE:
                return WHITE_CLIQUE;
            case BLACK_CLIQUE:
                return BLACK_CLIQUE;
            case TEAL_CLIQUE:
                return TEAL_CLIQUE;
            case VIOLET_CLIQUE:
                return VIOLET_CLIQUE;
            case CRIMSON_CLIQUE:
                return CRIMSON_CLIQUE;
            case GOLD_CLIQUE:
                return GOLD_CLIQUE;
            case NO_CLIQUE:
                return NO_CLIQUE;
            default:
                CommonHAKCAnalysis::getWriter() << "number " << Color->getZExtValue() << "isn't a valid color\n";
                return NO_CLIQUE;
        }
    }

    void HAKCModuleAnalysis::PrintStack() {
        // print stack debug
        void *buffer[10];
        int size = backtrace(buffer, 10);
        char **strings = backtrace_symbols(buffer, size);
        for (int i = 0; i < size; i++) {
            std::cout << strings[i] << std::endl;
        }
        free(strings);
    }

}// namespace hakc
