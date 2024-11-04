//
// Created by de29664 on 3/21/23.
//

#include <sstream>

#include "HAKCTransformers/Linux/HAKCTransformerLinux.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "HAKC-defs.h"
#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_sk_buff.h"

hakc::HAKCTransformerLinux::HAKCTransformerLinux(HAKCCompartmentalizationPolicy &Policy,
                                                 hakc::HAKCModuleAnalysisLinux &ModuleAnalysis,
                                                 HAKCTypeIdentifier &TypeIdentifier) :
        HAKCTransformer(Policy, ModuleAnalysis, TypeIdentifier),
        EntryTokenType(nullptr) {
    auto &Module = ModuleAnalysis.GetModule();
    Type *intTy64 = Type::getInt64Ty(Module.getContext());

    EntryTokenType = StructType::getTypeByName(Module.getContext(), ModuleAnalysis.HAKCEntryTokenName());
    if (!EntryTokenType) {
        EntryTokenType = StructType::create(Module.getContext(), {intTy64, intTy64},
                                            ModuleAnalysis.HAKCEntryTokenName());
    }
}

Value *hakc::HAKCTransformerLinux::CreateSafePointer_Arch(ManagedHAKCPointerP HAKCPointer, Instruction *I) {
    Value *voidCast;

    auto AddrSpace = GetPointerAddrSpace(HAKCPointer);

    if (HAKCPointer->GetBaseDefinition()->getType()->isIntegerTy()) {
        voidCast = HAKCIRBuilder.CreateIntToPtr(HAKCPointer->GetBaseDefinition(), HAKCIRBuilder.getPtrTy(AddrSpace));
    } else {
        voidCast = HAKCIRBuilder.CreateBitCast(HAKCPointer->GetBaseDefinition(), HAKCIRBuilder.getPtrTy(AddrSpace));
    }
    Value *maxUserAddr = HAKCIRBuilder.CreateIntToPtr(
            ConstantInt::get(HAKCIRBuilder.getInt64Ty(), 0x0000ffffffffffff),
            voidCast->getType());
    Value *addrCheck = HAKCIRBuilder.CreateICmpUGT(voidCast, maxUserAddr);
    Value *ptrToInt = HAKCIRBuilder.CreatePtrToInt(voidCast, HAKCIRBuilder.getInt64Ty());
    Value *orValue = HAKCIRBuilder.CreateOr(ptrToInt, 0xFFFF000000000000);
    Value *orCast = HAKCIRBuilder.CreateIntToPtr(orValue, HAKCPointer->GetBaseDefinition()->getType());

    return HAKCIRBuilder.CreateSelect(addrCheck, orCast, HAKCPointer->GetBaseDefinition());
}

CallInst *hakc::HAKCTransformerLinux::SaveColor(Value *V) {
    if (!V->getType()->isPointerTy() ||
        isa<ConstantPointerNull>(V)) {
        return nullptr;
    }

    auto AddrSpace = GetPointerAddrSpace(V);

    std::vector<Value *> Args = {HAKCIRBuilder.CreateBitCast(V, HAKCIRBuilder.getPtrTy(AddrSpace))};
    CallInst *SaveColorCall;
    if (CommonHAKCAnalysis::isPerCPUPointer(V)) {
        SaveColorCall = CreateCall(HAKCGetPerCPUColorName(), HAKCIRBuilder.getIntNTy(DIVISION_ID_BIT_LENGTH), Args);
    } else {
        SaveColorCall = CreateCall(HAKCGetColorName(), HAKCIRBuilder.getIntNTy(DIVISION_ID_BIT_LENGTH), Args);
    }

    return SaveColorCall;
}

void
hakc::HAKCTransformerLinux::CreateTransferFunctionArg_PreCall(Function *Target, Function *TransferFunction,
                                                              Value *Arg) {
    if (TargetIsKernel(Target)) {
        return;
    }

    auto *SaveColorInst = SaveColor(Arg);
    TransferArgumentsToRestore[Arg] = SaveColorInst;
}

void
hakc::HAKCTransformerLinux::CreateTransferFunctionArg_PostCall(Function *Target, Function *TransferFunction,
                                                               Value *Arg) {
    if (TargetIsKernel(Target)) {
        return;
    }
    CallInst *SavedColor = TransferArgumentsToRestore[Arg];
    auto ManagedPointer = CreateNewManagedPointer(Arg);
    auto *Size = GetObjectSizeInBytes(ManagedPointer);
    if (!Size) {
        Size = GetDefaultObjectSize();
    }
    auto AddrSpace = GetPointerAddrSpace(Arg);
    Value *Args[] = {
            HAKCIRBuilder.CreateBitCast(Arg, HAKCIRBuilder.getPtrTy(AddrSpace)),
            SavedColor,
            Size
    };
    CreateCall(HAKCColorAddressName(), HAKCIRBuilder.getVoidTy(), Args);
}

StringRef hakc::HAKCTransformerLinux::HAKCGetColorName() {
    return "get_hakc_address_color";
}

StringRef hakc::HAKCTransformerLinux::HAKCGetPerCPUColorName() {
    return "get_hakc_percpu_color";
}

StringRef hakc::HAKCTransformerLinux::HAKCColorAddressName() {
    return "hakc_color_address";
}

Type *hakc::HAKCTransformerLinux::GetEntryTokenType(unsigned AddrSpace) {
    return EntryTokenType;
}

Constant *hakc::HAKCTransformerLinux::GetEntryToken(HAKCCompartment &Compartment) {
    auto CompartmentID = Compartment.GetCompartmentIDValue();
    auto EntryTokenValue = Compartment.GetEntryToken()->getSExtValue();
    if (CompartmentID < 0) {
        CommonHAKCAnalysis::getWriter() << "Tried to create an Entry Token for an invalid CompartmentID: "
                                        << std::to_string(CompartmentID) <<
                                        "\n";
        throw std::exception();
    } else if ((EntryTokenValue >> HAKC_CONTEXT_COMPARTMENT_SHIFT) != CompartmentID) {
        CommonHAKCAnalysis::getWriter() << "Tried to create an Entry Token for Compartment "
                                        << std::to_string(CompartmentID)
                                        << " but encoded CompartmentID "
                                        << std::to_string((EntryTokenValue >> HAKC_CONTEXT_COMPARTMENT_SHIFT)) << "\n";
        throw std::exception();
    }
    return ConstantStruct::get(EntryTokenType, {
            Compartment.GetCompartmentID(), Compartment.GetEntryToken()
    });
}

void hakc::HAKCTransformerLinux::CreateTransferFunctionFinalize_Arch(Function *Original, Function *Transfer) {
    TransferArgumentsToRestore.clear();

    if (ModuleAnalysis.FunctionIsExported(Original)) {
        /* Exported functions are often used elsewhere. Make sure that
         * the transfer function is also exported, so it can be used in the same places.
         * According to the comment for ___EXPORT_SYMBOL in include/linux/export.h,
         * there are three things needed, but we won't use __kcrctab for simplicity
         */

        // Create kstrtab entry
        std::stringstream asm_stream;
        asm_stream << "\t.section \"__ksymtab_strings\",\"aMS\",%progbits,1\t\n"
                   << hakc::HAKCModuleAnalysisLinux::getKstrtab_entry_name(Transfer) << ":\t\t\t\t\t\n"
                   << "\t.asciz \"" << Transfer->getName().str() << "\"\n";

        // Create kstrtabns entry
        /* Namespace values for exported symbols are defined by inline assembly, which is hard to get, but
         * an empty string is valid, so hopefully this will be ok
         */
        asm_stream << hakc::HAKCModuleAnalysisLinux::getKstrtabns_entry_name(Transfer) << ":\t\t\t\t\t\n"
                   << "\t.asciz \"\"\n"
                   << "\t.previous\t\t\t\t\t\n";

        // Create kernel_symbol entry
        asm_stream << "\t.section \"___ksymtab+" << Transfer->getName().str() << "\", \"a\"\t\n"
                   << "\t.balign\t4\t\t\t\t\t\n"
                   << "__ksymtab_" << Transfer->getName().str() << ":\t\t\t\t\n"
                   << "\t.long\t" << Transfer->getName().str() << "- .\t\t\t\t\n"
                   << "\t.long\t" << hakc::HAKCModuleAnalysisLinux::getKstrtab_entry_name(Transfer) << "- .\t\t\t\n"
                   << "\t.long\t" << hakc::HAKCModuleAnalysisLinux::getKstrtabns_entry_name(Transfer) << "- .\t\t\t\n"
                   << "\t.previous\t\t\t\t\t\n";
        getModule().appendModuleInlineAsm(asm_stream.str());

        std::string unique_addressable_name = getUniqueAddressable_Name(Original);
        auto *unique_addressable = getModule().getNamedValue(unique_addressable_name);
        if (!unique_addressable) {
            CommonHAKCAnalysis::getWriter() << "Could not find unique ID global " << unique_addressable_name << "\n";
            throw std::exception();
        }
        unique_addressable_name = getUniqueAddressable_Name(Transfer);
        auto *transfer_unique_addressable = getModule().getOrInsertGlobal(unique_addressable_name,
                                                                          unique_addressable->getValueType());
        auto *transfer_unique_global = dyn_cast<GlobalVariable>(transfer_unique_addressable);
        auto *unique_addressable_global = dyn_cast<GlobalVariable>(unique_addressable);
        Constant *transfer_func_cast = ConstantExpr::getBitCast(Transfer,
                                                                PointerType::get(getModule().getContext(),
                                                                                 Transfer->getAddressSpace())
                /*transfer_unique_global->getType()->getPointerElementType()*/);
        transfer_unique_global->setSection(unique_addressable_global->getSection());
        transfer_unique_global->copyAttributesFrom(unique_addressable_global);
        transfer_unique_global->setInitializer(transfer_func_cast);
    }
}

std::string hakc::HAKCTransformerLinux::getUniqueAddressable_Name(Function *F) {
    std::string unique_addressable_name = "__UNIQUE_ID___addressable_";
    unique_addressable_name += F->getName();
    for (auto &G: getModule().globals()) {
        if (G.getName().starts_with(unique_addressable_name)) {
            return G.getName().str();
        }
    }
    return unique_addressable_name;
}

FunctionType *hakc::HAKCTransformerLinux::GetHAKCDataAuthenticationFunctionType(unsigned AddrSpace) {
    Type *RetTy = HAKCIRBuilder.getPtrTy(AddrSpace);
    Type *CompartmentType = GetHAKCCompartmentValue(0)->getType();
    Type *ArgTy[] = {
            HAKCIRBuilder.getPtrTy(AddrSpace),
            CompartmentType,
            HAKCIRBuilder.getInt64Ty()
    };

    return FunctionType::get(RetTy, ArgTy, false);
}

void hakc::HAKCTransformerLinux::CreateDataAuthArguments(ManagedHAKCPointerP HAKCPointer, Instruction *I,
                                                         SmallVector<Value *> &ArgsList) {
    auto *F = I->getFunction();
    auto CompartmentDivision = CompartmentalizationPolicy.GetDivision(F);
    auto AccessToken = CompartmentDivision.GetAccessToken();
    unsigned AddrSpace = GetPointerAddrSpace(HAKCPointer);
    auto *DataAuthFuncTy = GetHAKCDataAuthenticationFunctionType(AddrSpace);

    auto HAKCPointerBitCast = CreatePointerCast(HAKCPointer,
                                                dyn_cast<PointerType>(DataAuthFuncTy->getFunctionParamType(0)));
    ArgsList.append({HAKCPointerBitCast,
                     GetHAKCCompartmentValue(getFunctionCompartmentID(F)),
                     AccessToken});
}

void hakc::HAKCTransformerLinux::CreateCodeAuthArguments(ManagedHAKCPointerP HAKCPointer, Instruction *I,
                                                         SmallVector<Value *> &ArgsList) {
    Function *F = I->getFunction();
    auto *ExitTokens = GetValidTargetCompartments(F);
    auto CompartmentDivision = CompartmentalizationPolicy.GetDivision(F);
    auto AccessToken = CompartmentDivision.GetAccessToken();

    if (!ExitTokens->getValueType()->isArrayTy()) {
        CommonHAKCAnalysis::getWriter() << "Invalid ExitToken Type (" << *ExitTokens->getValueType() << ") for "
                                        << *ExitTokens << "\n";
        throw std::exception();
    }
    Value *FirstExitToken = HAKCIRBuilder.CreateGEP(ExitTokens->getValueType(),
                                                    ExitTokens, {
                                                            HAKCIRBuilder.getInt64(0), HAKCIRBuilder.getInt64(0)
                                                    });
    unsigned AddrSpace = GetPointerAddrSpace(FirstExitToken);
    Value *IndirectCallTarget = HAKCIRBuilder.CreateBitCast(HAKCPointer->GetBaseDefinition(),
                                                            HAKCIRBuilder.getPtrTy(AddrSpace));
    ArgsList.append({
                            IndirectCallTarget,
                            GetHAKCCompartmentValue(getFunctionCompartmentID(F)),
                            AccessToken,
                            FirstExitToken,
                            HAKCIRBuilder.getInt64(ExitTokens->getValueType()->getArrayNumElements())
                    });
}

void
hakc::HAKCTransformerLinux::CreateTransferArguments(ManagedHAKCPointerP HAKCPointer, GlobalValue *Target, bool IsData,
                                                    ConstantInt *Size, SmallVector<Value *> &Result) {
    Value *OperandCast;
    auto AddrSpace = GetPointerAddrSpace(HAKCPointer);
    bool IsPerCPU = CommonHAKCAnalysis::isPerCPUPointer(HAKCPointer->GetBaseDefinition());
    auto Division = CompartmentalizationPolicy.GetDivision(Target);

    OperandCast = HAKCIRBuilder.CreateBitOrPointerCast(HAKCPointer->GetBaseDefinition(),
                                                       HAKCIRBuilder.getPtrTy(AddrSpace));

    Result.push_back(OperandCast);
    Result.push_back(Size);
    Result.push_back(Division.GetHAKCCompartment().GetCompartmentID());
    Result.push_back(Division.GetDivisionID());
    if (!IsPerCPU) {
        /* Function signature uses is_code which is !isData */
        Result.push_back(IsData ? getFalse() : getTrue());
    }
}

ConstantInt *hakc::HAKCTransformerLinux::GetColorValue(hakc::sym_color_t Color) {
    return HAKCIRBuilder.getIntN(DIVISION_ID_BIT_LENGTH, Color);
}
