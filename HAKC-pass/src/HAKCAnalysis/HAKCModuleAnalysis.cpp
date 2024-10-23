//
// Created by derrick on 8/20/21.
//
#include <iostream>
#include <sstream>
#include <tuple>

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Regex.h"

#include "HAKCAnalysis/HAKCModuleAnalysis.h"
#include "HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "HAKCSystemInformation.h"
#include <stdio.h>
#include <string.h>
#include <bits/stdc++.h>

// #include "HAKCFunctionDefinition/HAKCTransferFunction.h"
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
              moduleModified(false),
              M(M),
              AnalysisFunctions(),
              transformer(nullptr),
              Transfers(), 
              NonTransferHAKCFunctions(),
              totalDataChecks(0),
              totalCodeChecks(0), totalTransfers(0), 
              SysInfo(HAKCSystemInformation(M)) {
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
        std::set<GlobalVariable *> globalsToChange;

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
        return getTransformer().getSystemInformation().ContainsCompartmentalizedSymbols(getModule());
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
        } else if (isa<ConstantStruct>(U.getUser())) {
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
        } else if (isa<SelectInst>(U.getUser())) {
            return true;
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

    void HAKCModuleAnalysis::performTransformations() {
        if (isModuleCompartmentalized()) {
            std::map<Function *, std::set<CallInst *>> transferFunctionCalls;
            for (auto &F: getModule().getFunctionList()) {
                for (auto it = inst_begin(F); it != inst_end(F); ++it) {
                    Instruction *inst = &*it;

                    if (auto *call = dyn_cast<CallInst>(inst)) {
                        if (call->getCalledFunction()) {
                            if (IsHAKCTransferFunction(call->getCalledFunction())) {
                                transferFunctionCalls[&F].insert(call);
                            }
                        }
                    }
                }
            }

            updateCallParameters(transferFunctionCalls);
            updateCallParameters(HAKCFunctions);

            compartmentalizeModule();

        } else {
            removeSignatures();
        }

        addTransferFunctions();
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
            // delete functionAnalysis;
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

    void HAKCModuleAnalysis::RegisterCustomTransfer(hakc_custom_transfer_def_t CustomTransfer) {
        if (CustomTransfer->GetFunction() == nullptr || CustomTransfer->GetType() == nullptr) {
            return;
        }

        CustomTransfers.push_back(CustomTransfer);
    }

    void HAKCModuleAnalysis::RegisterHAKCTransfer(hakc_transfer_def_t Transfer) {
        if (Transfer) {
            Transfers.insert(Transfer);
        }
    }

    std::set<StringRef> HAKCModuleAnalysis::GetSeparateNamespacePaths() {
        return SysInfo.METHODS["GetSeparateNamespacePaths"];
    }

    std::set<StringRef> HAKCModuleAnalysis::GetHAKCSourcePaths() {
        return SysInfo.METHODS["GetHAKCSourcePaths"];
    }


    void HAKCModuleAnalysis::RegisterNonTransferHAKCFunction(hakc_function_def_t HAKCFunction) {
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

    void HAKCModuleAnalysis::addTransferFunctions() {
        std::vector<Function *> FuncsNeedingTransfers;
        for (auto &F: getModule().getFunctionList()) {
            if (functionIsTransferCandidate(&F)) {
                FuncsNeedingTransfers.push_back(&F);
            }
        }
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

        for (auto *F: AnalysisFunctions) {
            debug_output = (F->getName() == getHAKCDebugName());
            CompartmentalizeFunction(F);
        }
        addCompartmentMetadata();

        transferModuleParams();
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

    std::shared_ptr<HAKCTransformer> HAKCModuleAnalysis::CreateTransformer(){
        return std::make_shared<HAKCTransformer>(M, this); 
    }

    bool HAKCModuleAnalysis::isModuleTransformed() {
        return moduleModified;
    }

    void HAKCModuleAnalysis::CompartmentalizeFunction(Function *F) {
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Compartmentalizing " << F->getName() << "\n";
        }
        auto compartment = getTransformer().getFunctionCompartmentID(F);
        registerUsedCompartment(compartment);

        HAKCFunctionAnalysis *functionAnalysis = GetFunctionTransformation(F);
        functionAnalysis->InstrumentCompartmentalizedCode();
        moduleModified |= functionAnalysis->modifiedFunction();

        totalCodeChecks += functionAnalysis->GetCodeAuthenticationCount();
        totalDataChecks += functionAnalysis->GetDataAuthenticationCount();
        totalTransfers += functionAnalysis->GetCompartmentTransferCount();

        // delete functionAnalysis;
    }

    std::set<StringRef> HAKCModuleAnalysis::GetSafeTransitionFunctions() {
        return SysInfo.METHODS["GetSafeTransitionFunctions_Arch"];
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

    StringRef hakc::HAKCModuleAnalysis::HAKCPerCPUCompartmentTransferName() {
        return "hakc_transfer_percpu_to_clique";
    }

    StringRef hakc::HAKCModuleAnalysis::HAKCEntryTokenName() {
        return "HAKCEntryToken";
    }

    HAKCFunctionAnalysis *HAKCModuleAnalysis::GetFunctionTransformation(Function *F) {
        return new HAKCFunctionAnalysis(F, this);
    }

    bool HAKCModuleAnalysis::functionIsTransferCandidate(Function *F) {
        // linux
        if (F->getName().contains("__SCT")) {
            /* Handle trampolines */
            return false;
        }
        return HAKCModuleAnalysis::functionIsTransferCandidate(F);
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
        auto Symbol = getTransformer().getSystemInformation().findSymbol(GV);
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
               return nullptr; // someone passed us a struct that wasn't a kernel param struct
           }
        }
        else {
            return nullptr; // this is not good, don't give non-structs to this function
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
        PointerType* PointerTy = PointerType::get(IntegerType::get(M.getContext(), 8), 0);

        // two args
        std::vector<Type*>FuncTy_args;
        // first arg points to param
        FuncTy_args.push_back(PointerTy);
        // second arg is int64_t flag (0 to return param's access token, 1 to return param's color)
        FuncTy_args.push_back(IntegerType::get(M.getContext(),64));

        // type of function that returns int64_t, takes (void *, int64_t)
        FunctionType* FuncTy = FunctionType::get(IntegerType::get(M.getContext(),64),
                                                 FuncTy_args,
                                                 false);

        // create a function named "hakc_modparam_getctx_paramname"
        auto c = M.getOrInsertFunction(MODPARAM_GETCTX_PREFIX.str() + kernparam->getName().str(),
                                       FuncTy);

        auto *constc = dyn_cast<Constant>(c.getCallee());
        Function* getctx = cast<Function>(constc);
        getctx->setCallingConv(CallingConv::C);

        // put "hakc_modparam_getctx_paramname" in a special text section in the module
        getctx->setSection(HAKC_MODPARAM_TEXT_SECTION);

        Function::arg_iterator args = getctx->arg_begin();
        // param pointer
        Value *pointerArg = args++;
        // 0 to return context, 1 to return color
        Value *returnTypeArg = args++;

        // create entry basic block in our new function
        BasicBlock* block = BasicBlock::Create(M.getContext(), "entry", getctx);
        //
        IRBuilder<> builder(block);
        // constant zero for compare/select
        Value *czero = ConstantInt::get(IntegerType::get(M.getContext(),64), 0);

        // get HAKC symbol for the kernel parameter Value
        auto Symbol = getTransformer().getSystemInformation().findSymbol(kernparam);

        // find the color of the HAKC symbol
        ConstantInt* Color;
        if (!Symbol) {
            CommonHAKCAnalysis::getWriter() << "Could not find HAKC Symbol for kernel param global: \n";
            kernparam->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            Color = getTransformer().getInt64(KERNEL_COLOR);
            throw std::exception();
        }
        else {
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
        ConstantInt* tok = builder.getInt64(AccessToken);

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
        GlobalVariable* gcfp = new GlobalVariable(M,
                                                  getctx->getType(),
                                                  true, // const
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
        for (auto &pair: calls_map) {
            Function *F = pair.first;
            debug_output = (F->getName() == getHAKCDebugName());

            for (auto &call: pair.second) {
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
                        CommonHAKCAnalysis::getWriter() << "Updating index " << std::to_string
                                (HAKCTransferFunction->GetCompartmentIdIdx()) << " (";
                        call->getArgOperand(HAKCTransferFunction->GetCompartmentIdIdx())->print(
                                CommonHAKCAnalysis::getWriter());
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


//     bool HAKCModuleAnalysisLinux::FunctionNeedsAnalysis(Function *F) {
//         if (F->getName().contains("static_branch_")) {
//             /* These functions call inline assembly that needs to be
//              * constant at compile time, so we can't analyze them.
//              * We ensure that any pointer passed to these functions have
//              * no signature in argNeedsAnalysis.
//              */
//             if (debug_output) {
//                 CommonHAKCAnalysis::getWriter() << F->getName() << " does not need analysis\n";
//             }
//             return false;
//         }
//         return HAKCModuleAnalysis::FunctionNeedsAnalysis(F);
//     }

    std::map<StringRef, std::tuple<void*, std::vector<int>>> HAKCModuleAnalysis::GetKernelAllocationSizeMap() {
        // linux
        return SysInfo.KernelAllocationSizeMap;
    }

    std::set<StringRef> HAKCModuleAnalysis::GetIgnoredGlobals() {
        return SysInfo.METHODS["GetIgnoredGlobals"];
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

    std::set<StringRef> HAKCModuleAnalysis::GetIgnoredTypes() {
        return SysInfo.METHODS["GetIgnoredTypes"];
    }

    std::set<StringRef> HAKCModuleAnalysis::GetNoTransferFunctions() {
        return SysInfo.METHODS["GetNoTransferFunctions"];
    }

    sym_color_t HAKCModuleAnalysis::GetMajoritySymbolColor() {
        // linux
        if (!MajorityColorSet) {
            std::map<ConstantInt *, unsigned> ColorCounts;
            std::set<GlobalValue *> symbols;
            for (auto &Global: M.getGlobalList()) {
                symbols.insert(&Global);
            }
            for (auto &F: M.getFunctionList()) {
                symbols.insert(&F);
            }

            for (auto *GV: symbols) {
                auto color = getSymbolColor(GV);
                if (!color->equalsInt(hakc::KERNEL_COLOR)) {
                    if (ColorCounts.find(color) == ColorCounts.end()) {
                        ColorCounts[color] = 0;
                    }
                    ColorCounts[color] += 1;
                }
            }

            unsigned MaxSymbolCount = 0;
            for (auto &it: ColorCounts) {
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



}// namespace hakc
