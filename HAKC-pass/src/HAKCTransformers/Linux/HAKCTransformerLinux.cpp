//
// Created by de29664 on 3/21/23.
//

#include <sstream>

#include "HAKCTransformers/Linux/HAKCTransformerLinux.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "HAKC-defs.h"
#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_sk_buff.h"

hakc::HAKCTransformerLinux::HAKCTransformerLinux(Module &Module,
                                                 hakc::HAKCModuleAnalysisLinux *ModuleAnalysis) :
        HAKCTransformer(Module, ModuleAnalysis),
        EntryTokenType(nullptr) {
    Type *intTy64 = Type::getInt64Ty(Module.getContext());

    EntryTokenType = StructType::getTypeByName(Module.getContext(), ModuleAnalysis->HAKCEntryTokenName());
    if (!EntryTokenType) {
        EntryTokenType = StructType::create(Module.getContext(), {intTy64, intTy64}, ModuleAnalysis->HAKCEntryTokenName
                ());
    }
}

Value *hakc::HAKCTransformerLinux::CreateSafePointer_Arch(Value *HAKCPointer, Instruction *I) {
    if (isa<PHINode>(I)) {
        CommonHAKCAnalysis::getWriter() << "Trying to insert data auth check at ";
        I->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << " for ";
        HAKCPointer->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\n";
        I->getFunction()->print(CommonHAKCAnalysis::getWriter());
        throw std::exception();
    } else if (isa<ConstantPointerNull>(HAKCPointer)) {
        CommonHAKCAnalysis::getWriter() << "ManagedHAKCPointer is a ConstantPointerNull: ";
        HAKCPointer->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\n";
        throw std::exception();
    }
    Value *voidCast;

    auto AddrSpace = GetPointerAddrSpace(HAKCPointer);

    if (HAKCPointer->getType()->isIntegerTy()) {
        voidCast = HAKCIRBuilder.CreateIntToPtr(HAKCPointer, HAKCIRBuilder.getInt8PtrTy(AddrSpace));
    } else {
        voidCast = HAKCIRBuilder.CreateBitCast(HAKCPointer, HAKCIRBuilder.getInt8PtrTy(AddrSpace));
    }
    Value *maxUserAddr = HAKCIRBuilder.CreateIntToPtr(
            ConstantInt::get(HAKCIRBuilder.getInt64Ty(), 0x0000ffffffffffff),
            voidCast->getType());
    Value *addrCheck = HAKCIRBuilder.CreateICmpUGT(voidCast, maxUserAddr);
    Value *ptrToInt = HAKCIRBuilder.CreatePtrToInt(voidCast,
                                                   HAKCIRBuilder.getInt64Ty());
    Value *orValue = HAKCIRBuilder.CreateOr(ptrToInt, 0xFFFF000000000000);
    Value *orCast = HAKCIRBuilder.CreateIntToPtr(orValue,
                                                 HAKCPointer->getType());

    return HAKCIRBuilder.CreateSelect(addrCheck, orCast, HAKCPointer);
}

CallInst *hakc::HAKCTransformerLinux::SaveColor(Value *V) {
    if (!V->getType()->isPointerTy() ||
        isa<ConstantPointerNull>(V)) {
        return nullptr;
    }

    auto AddrSpace = GetPointerAddrSpace(V);

    std::vector<Value *> Args = {HAKCIRBuilder.CreateBitCast(V, HAKCIRBuilder.getInt8PtrTy(AddrSpace))};
    CallInst *SaveColorCall;
    if (CommonHAKCAnalysis::isPerCPUPointer(V)) {
        SaveColorCall = CreateCall(HAKCGetPerCPUColorName(), HAKCIRBuilder.getIntNTy(CLIQUE_COLOR_BIT_LENGTH), Args);
    } else {
        SaveColorCall = CreateCall(HAKCGetColorName(), HAKCIRBuilder.getIntNTy(CLIQUE_COLOR_BIT_LENGTH), Args);
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
    auto *Size = GetObjectSizeInBytes(Arg);
    if (!Size) {
        Size = GetDefaultObjectSize();
    }
    auto AddrSpace = GetPointerAddrSpace(Arg);
    Value *Args[] = {
            HAKCIRBuilder.CreateBitCast(Arg, HAKCIRBuilder.getInt8PtrTy(AddrSpace)),
            SavedColor,
            Size
    };
    CreateCall(HAKCColorAddressName(), HAKCIRBuilder.getVoidTy(), Args);
}

const StringRef hakc::HAKCTransformerLinux::HAKCGetColorName() {
    return "get_hakc_address_color";
}

const StringRef hakc::HAKCTransformerLinux::HAKCGetPerCPUColorName() {
    return "get_hakc_percpu_color";
}

const StringRef hakc::HAKCTransformerLinux::HAKCColorAddressName() {
    return "hakc_color_address";
}

Type *hakc::HAKCTransformerLinux::GetEntryTokenType(unsigned AddrSpace) {
    return EntryTokenType;
}

Constant *hakc::HAKCTransformerLinux::GetEntryToken(hakc_compartment_id_t CompartmentID) {
    auto EntryTokenValue = SystemInformation.getEntryToken(CompartmentID);
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
            HAKCIRBuilder.getInt64(CompartmentID), HAKCIRBuilder.getInt64(EntryTokenValue)
    });
}

void hakc::HAKCTransformerLinux::CreateTransferFunctionFinalize_Arch(Function *Original, Function *Transfer) {
    TransferArgumentsToRestore.clear();

    if (FunctionIsExported(Original)) {
        /* Exported functions are often used elsewhere. Make sure that
         * the transfer function is also exported, so it can be used in the same places.
         * According to the comment for ___EXPORT_SYMBOL in include/linux/export.h,
         * there are three things needed, but we won't use __kcrctab for simplicity
         */

        // Create kstrtab entry
        std::stringstream asm_stream;
        asm_stream << "\t.section \"__ksymtab_strings\",\"aMS\",%progbits,1\t\n"
                   << getKstrtab_entry_name(Transfer) << ":\t\t\t\t\t\n"
                   << "\t.asciz \"" << Transfer->getName().str() << "\"\n";

        // Create kstrtabns entry
        /* Namespace values for exported symbols are defined by inline assembly, which is hard to get, but
         * an empty string is valid, so hopefully this will be ok
         */
        asm_stream << getKstrtabns_entry_name(Transfer) << ":\t\t\t\t\t\n"
                   << "\t.asciz \"\"\n"
                   << "\t.previous\t\t\t\t\t\n";

        // Create kernel_symbol entry
        asm_stream << "\t.section \"___ksymtab+" << Transfer->getName().str() << "\", \"a\"\t\n"
                   << "\t.balign\t4\t\t\t\t\t\n"
                   << "__ksymtab_" << Transfer->getName().str() << ":\t\t\t\t\n"
                   << "\t.long\t" << Transfer->getName().str() << "- .\t\t\t\t\n"
                   << "\t.long\t" << getKstrtab_entry_name(Transfer) << "- .\t\t\t\n"
                   << "\t.long\t" << getKstrtabns_entry_name(Transfer) << "- .\t\t\t\n"
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
                                                                transfer_unique_global->getType()->getPointerElementType());
        transfer_unique_global->setSection(unique_addressable_global->getSection());
        transfer_unique_global->copyAttributesFrom(unique_addressable_global);
        transfer_unique_global->setInitializer(transfer_func_cast);
    }
}

bool hakc::HAKCTransformerLinux::FunctionIsExported(Function *F) {
    auto ksym_name = getKstrtab_entry_name(F);
    /* Add colon so __kstrtab_foo_1 doesn't match __kstrtab_foo */
    ksym_name += ":";
    return getModule().getModuleInlineAsm().find(ksym_name) != getModule().getModuleInlineAsm().npos;
}

std::string hakc::HAKCTransformerLinux::getUniqueAddressable_Name(Function *F) {
    std::string unique_addressable_name = "__UNIQUE_ID___addressable_";
    unique_addressable_name += F->getName();
    for (auto &G: getModule().getGlobalList()) {
        if (G.getName().startswith(unique_addressable_name)) {
            return G.getName().str();
        }
    }
    return unique_addressable_name;
}

std::string hakc::HAKCTransformerLinux::getKstrtab_entry_name(Function *F) {
    std::string ksymtab_symbol_name = "__kstrtab_";
    ksymtab_symbol_name += F->getName();
    return ksymtab_symbol_name;
}

std::string hakc::HAKCTransformerLinux::getKstrtabns_entry_name(Function *F) {
    std::string ksymtabns_symbol_name = "__kstrtabns_";
    ksymtabns_symbol_name += F->getName();
    return ksymtabns_symbol_name;
}

FunctionType *hakc::HAKCTransformerLinux::GetHAKCDataAuthenticationFunctionType(unsigned AddrSpace) {
    Type *RetTy = HAKCIRBuilder.getInt8PtrTy(AddrSpace);
    Type *CompartmentType = GetHAKCCompartmentValue(0)->getType();
    Type *ArgTy[] = {
            HAKCIRBuilder.getInt8PtrTy(AddrSpace),
            CompartmentType,
            HAKCIRBuilder.getInt64Ty()
    };

    return FunctionType::get(RetTy, ArgTy, false);
}

std::vector<Value *> hakc::HAKCTransformerLinux::CreateDataAuthArguments(Value *HAKCPointer, Instruction *I) {
    Function *F = I->getFunction();
    Value *HAKCPointerBitCast;
    auto Symbol = SystemInformation.findSymbol(F);
    if (!Symbol) {
        CommonHAKCAnalysis::getWriter() << "Could not find symbol for function " << F->getName() << "\n";
        throw std::exception();
    }
    auto AccessToken = Symbol->getCompartment()->getAccessToken();
    unsigned AddrSpace = GetPointerAddrSpace(HAKCPointer);
    auto *DataAuthFuncTy = GetHAKCDataAuthenticationFunctionType(AddrSpace);

    if (HAKCPointer->getType()->isIntegerTy()) {
        HAKCPointerBitCast = HAKCIRBuilder.CreateIntToPtr(HAKCPointer, DataAuthFuncTy->getParamType(0));
    } else {
        HAKCPointerBitCast = HAKCIRBuilder.CreateBitCast(HAKCPointer, DataAuthFuncTy->getParamType(0));
    }
    return {HAKCPointerBitCast,
            GetHAKCCompartmentValue(getFunctionCompartmentID(F)),
            HAKCIRBuilder.getInt64(AccessToken)};
}

std::vector<Value *> hakc::HAKCTransformerLinux::CreateCodeAuthArguments(Value *HAKCPointer, Instruction *I) {
    Function *F = I->getFunction();
    auto *ExitTokens = GetValidTargetCompartments(F);
    auto Symbol = SystemInformation.findSymbol(F);
    if (!Symbol) {
        CommonHAKCAnalysis::getWriter() << "Could not find symbol for function " << F->getName() << "\n";
        throw std::exception();
    }
    auto AccessToken = Symbol->getCompartment()->getAccessToken();

    if (!ExitTokens->getValueType()->isArrayTy()) {
        CommonHAKCAnalysis::getWriter() << "Invalid ExitToken Type (";
        ExitTokens->getValueType()->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << ") for ";
        ExitTokens->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\n";
        throw std::exception();
    }
    Value *FirstExitToken = HAKCIRBuilder.CreateGEP(ExitTokens->getValueType(),
                                                    ExitTokens, {
                                                            HAKCIRBuilder.getInt64(0), HAKCIRBuilder.getInt64(0)
                                                    });
    unsigned AddrSpace = GetPointerAddrSpace(FirstExitToken);
    Value *IndirectCallTarget = HAKCIRBuilder.CreateBitCast(HAKCPointer, HAKCIRBuilder.getInt8PtrTy(AddrSpace));
    return {
            IndirectCallTarget,
            GetHAKCCompartmentValue(getFunctionCompartmentID(F)),
            HAKCIRBuilder.getInt64(AccessToken),
            FirstExitToken,
            HAKCIRBuilder.getInt64(ExitTokens->getType()->getPointerElementType()->getArrayNumElements())
    };
}

std::vector<Value *> hakc::HAKCTransformerLinux::CreateTransferArguments(Value *HAKCPointer, Function *Target,
                                                                         bool IsData,
                                                                         ConstantInt *Size) {
    std::vector<Value *> FullArgSet;

    Value *OperandCast;
    auto AddrSpace = GetPointerAddrSpace(HAKCPointer);
    bool IsPerCPU = CommonHAKCAnalysis::isPerCPUPointer(HAKCPointer);
    auto Symbol = SystemInformation.findSymbol(Target);
    hakc_compartment_id_t CompartmentID;
    sym_color_t Color;
    if (Symbol) {
        Color = Symbol->getCompartment()->getColor();
        CompartmentID = Symbol->getCompartmentID();
    } else {
        Color = KERNEL_COLOR;
        CompartmentID = KERNEL_COMPARTMENT;
    }

    OperandCast = HAKCIRBuilder.CreateBitOrPointerCast(HAKCPointer, HAKCIRBuilder.getInt8PtrTy(AddrSpace));

    FullArgSet.push_back(OperandCast);
    FullArgSet.push_back(Size);
    FullArgSet.push_back(GetHAKCCompartmentValue(CompartmentID));
    FullArgSet.push_back(GetColorValue(Color));
    if (!IsPerCPU) {
        /* Function signature uses is_code which is !isData */
        FullArgSet.push_back(IsData ? getFalse() : getTrue());
    }

    return FullArgSet;
}

ConstantInt *hakc::HAKCTransformerLinux::GetColorValue(hakc::sym_color_t Color) {
    return HAKCIRBuilder.getIntN(CLIQUE_COLOR_BIT_LENGTH, Color);
}
