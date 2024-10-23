//
// Created by de29664 on 3/21/23.
//

#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/InstIterator.h"

#include "HAKCTransformers/HAKCTransformer.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "HAKCAnalysis/HAKCModuleAnalysis.h"
#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"

#include <sstream>
#include "HAKC-defs.h"
// #include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_sk_buff.h"

    Value *hakc::HAKCTransformer::CreateSafePointer_Arch(Value *HAKCPointer, Instruction *I) {
        // if (isa<PHINode>(I)) {
        //     CommonHAKCAnalysis::getWriter() << "Trying to insert data auth check at ";
        //     I->print(CommonHAKCAnalysis::getWriter());
        //     CommonHAKCAnalysis::getWriter() << " for ";
        //     HAKCPointer->print(CommonHAKCAnalysis::getWriter());
        //     CommonHAKCAnalysis::getWriter() << "\n";
        //     I->getFunction()->print(CommonHAKCAnalysis::getWriter());
        //     throw std::exception();
        // } else if (isa<ConstantPointerNull>(HAKCPointer)) {
        //     CommonHAKCAnalysis::getWriter() << "ManagedHAKCPointer is a ConstantPointerNull: ";
        //     HAKCPointer->print(CommonHAKCAnalysis::getWriter());
        //     CommonHAKCAnalysis::getWriter() << "\n";
        //     throw std::exception();
        // }
        // Value *voidCast;

        // auto AddrSpace = GetPointerAddrSpace(HAKCPointer);

        // if (HAKCPointer->getType()->isIntegerTy()) {
        //     voidCast = HAKCIRBuilder.CreateIntToPtr(HAKCPointer, HAKCIRBuilder.getInt8PtrTy(AddrSpace));
        // } else {
        //     voidCast = HAKCIRBuilder.CreateBitCast(HAKCPointer, HAKCIRBuilder.getInt8PtrTy(AddrSpace));
        // }
        // Value *maxUserAddr = HAKCIRBuilder.CreateIntToPtr(
        //         ConstantInt::get(HAKCIRBuilder.getInt64Ty(), 0x0000ffffffffffff),
        //         voidCast->getType());
        // Value *addrCheck = HAKCIRBuilder.CreateICmpUGT(voidCast, maxUserAddr);
        // Value *ptrToInt = HAKCIRBuilder.CreatePtrToInt(voidCast,
        //                                                HAKCIRBuilder.getInt64Ty());
        // Value *orValue = HAKCIRBuilder.CreateOr(ptrToInt, 0xFFFF000000000000);
        // Value *orCast = HAKCIRBuilder.CreateIntToPtr(orValue,
        //                                              HAKCPointer->getType());

        // return HAKCIRBuilder.CreateSelect(addrCheck, orCast, HAKCPointer);
    }

    CallInst *hakc::HAKCTransformer::SaveColor(Value *V) {
        // if (!V->getType()->isPointerTy() ||
        //     isa<ConstantPointerNull>(V)) {
        //     return nullptr;
        // }

        // auto AddrSpace = GetPointerAddrSpace(V);

        // std::vector<Value *> Args = {HAKCIRBuilder.CreateBitCast(V, HAKCIRBuilder.getInt8PtrTy(AddrSpace))};
        // CallInst *SaveColorCall;
        // if (CommonHAKCAnalysis::isPerCPUPointer(V)) {
        //     SaveColorCall = CreateCall(HAKCGetPerCPUColorName(), HAKCIRBuilder.getIntNTy(CLIQUE_COLOR_BIT_LENGTH), Args);
        // } else {
        //     SaveColorCall = CreateCall(HAKCGetColorName(), HAKCIRBuilder.getIntNTy(CLIQUE_COLOR_BIT_LENGTH), Args);
        // }

        // return SaveColorCall;
    }

    const StringRef hakc::HAKCTransformer::HAKCGetColorName() {
        return "get_hakc_address_color";
    }

    const StringRef hakc::HAKCTransformer::HAKCGetPerCPUColorName() {
        return "get_hakc_percpu_color";
    }

    const StringRef hakc::HAKCTransformer::HAKCColorAddressName() {
        return "hakc_color_address";
    }

    Type *hakc::HAKCTransformer::GetEntryTokenType(unsigned AddrSpace) {
        return EntryTokenType;
    }

    Constant *hakc::HAKCTransformer::GetEntryToken(hakc_compartment_id_t CompartmentID) {
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

    void hakc::HAKCTransformer::CreateTransferFunctionFinalize_Arch(Function *Original, Function *Transfer) {
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

    std::string hakc::HAKCTransformer::getUniqueAddressable_Name(Function *F) {
        std::string unique_addressable_name = "__UNIQUE_ID___addressable_";
        unique_addressable_name += F->getName();
        for (auto &G: getModule().getGlobalList()) {
            if (G.getName().startswith(unique_addressable_name)) {
                return G.getName().str();
            }
        }
        return unique_addressable_name;
    }

    std::string hakc::HAKCTransformer::getKstrtab_entry_name(Function *F) {
        std::string ksymtab_symbol_name = "__kstrtab_";
        ksymtab_symbol_name += F->getName();
        return ksymtab_symbol_name;
    }

    std::string hakc::HAKCTransformer::getKstrtabns_entry_name(Function *F) {
        std::string ksymtabns_symbol_name = "__kstrtabns_";
        ksymtabns_symbol_name += F->getName();
        return ksymtabns_symbol_name;
    }

    FunctionType *hakc::HAKCTransformer::GetHAKCDataAuthenticationFunctionType(unsigned AddrSpace) {
        Type *RetTy = HAKCIRBuilder.getInt8PtrTy(AddrSpace);
        Type *CompartmentType = GetHAKCCompartmentValue(0)->getType();
        Type *ArgTy[] = {
                HAKCIRBuilder.getInt8PtrTy(AddrSpace),
                CompartmentType,
                HAKCIRBuilder.getInt64Ty()
        };

        return FunctionType::get(RetTy, ArgTy, false);
    }

    std::vector<Value *> hakc::HAKCTransformer::CreateDataAuthArguments(Value *HAKCPointer, Instruction *I) {
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

    std::vector<Value *> hakc::HAKCTransformer::CreateCodeAuthArguments(Value *HAKCPointer, Instruction *I) {
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

    std::vector<Value *> hakc::HAKCTransformer::CreateTransferArguments(Value *HAKCPointer, Function *Target,
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

    ConstantInt *hakc::HAKCTransformer::GetColorValue(hakc::sym_color_t Color) {
        return HAKCIRBuilder.getIntN(CLIQUE_COLOR_BIT_LENGTH, Color);
    }


// moving system information into module analysis
hakc::HAKCTransformer::HAKCTransformer(Module &Module, HAKCModuleAnalysis *HAKCAnalysis) :
        HAKCIRBuilder(Module.getContext()),
        DebugInfoProcessor(Module),
        SystemInformation(Module),
        HAKCAnalysis(HAKCAnalysis),
        VariadicTransferFunctions(), 
        EntryTokenType(nullptr){
        // Type *intTy64 = Type::getInt64Ty(Module.getContext());

        // EntryTokenType = StructType::getTypeByName(Module.getContext(), ModuleAnalysis->HAKCEntryTokenName());
        // if (!EntryTokenType) {
        //     EntryTokenType = StructType::create(Module.getContext(), {intTy64, intTy64}, ModuleAnalysis->HAKCEntryTokenName
        //             ());
        // }

}

Module &hakc::HAKCTransformer::getModule() {
    return SystemInformation.getModule();
}

hakc::HAKCSystemInformation &hakc::HAKCTransformer::getSystemInformation() {
    return SystemInformation;
}

void hakc::HAKCTransformer::ValidateLocation(Instruction *I) {
    if (I == nullptr) {
        CommonHAKCAnalysis::getWriter() << "I is null\n";
        throw std::exception();
    }
    HAKCIRBuilder.SetInsertPoint(I);
}

void hakc::HAKCTransformer::ValidateHAKCPointer(Value *HAKCPointer) {
    if (HAKCPointer == nullptr) {
        CommonHAKCAnalysis::getWriter() << "ManagedHAKCPointer is null\n";
        throw std::exception();
    }

    bool IsPointerLike = (HAKCPointer->getType()->isPointerTy() || ValidateHAKCIntegerPointerSize(HAKCPointer));

    if (!IsPointerLike) {
        CommonHAKCAnalysis::getWriter() << "ManagedHAKCPointer ";
        HAKCPointer->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << " is not Pointer-like!\n";
        for (auto *deflink: HAKCAnalysis->findDefChain(HAKCPointer, false, true)) {
            CommonHAKCAnalysis::getWriter() << "\t";
            deflink->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }
        throw std::exception();
    }
}

bool hakc::HAKCTransformer::ValidateHAKCIntegerPointerSize(Value *HAKCPointer) {
    return HAKCPointer->getType()->isIntegerTy(64);
}

void hakc::HAKCTransformer::ValidateHAKCPointerAndLocation(Value *HAKCPointer, Instruction *I) {
    try {
        ValidateHAKCPointer(HAKCPointer);
        ValidateLocation(I);
    } catch (std::exception &e) {
        if (I) {
            I->getFunction()->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            throw e;
        }
    }
}

Value *hakc::HAKCTransformer::CreateSafePointer(Value *HAKCPointer, Instruction *I) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);

    auto *SafePtr = CreateSafePointer_Arch(HAKCPointer, I);
    if (SafePtr->getType() != HAKCPointer->getType()) {
        CommonHAKCAnalysis::getWriter() << "SafePtr and ManagedHAKCPointer are not the same Type!\n"
                                        << "SafePtr: ";
        SafePtr->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\nManagedHAKCPointer: ";
        HAKCPointer->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\n";
        throw std::exception();
    }
    return SafePtr;
}

Value *hakc::HAKCTransformer::CreateDataAuthentication(Value *HAKCPointer, Instruction *I) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);

    if (isa<PHINode>(I)) {
        CommonHAKCAnalysis::getWriter() << "Trying to insert data auth check at ";
        I->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << " for ";
        HAKCPointer->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\n";
        I->getFunction()->print(CommonHAKCAnalysis::getWriter());
        throw std::exception();
    }

    Value *HAKCPointerBitCast;
    unsigned AddrSpace = GetPointerAddrSpace(HAKCPointer);
    auto *DataAuthFuncTy = GetHAKCDataAuthenticationFunctionType(AddrSpace);
    auto Args = CreateDataAuthArguments(HAKCPointer, I);
    for (unsigned i = 0; i < DataAuthFuncTy->getNumParams(); i++) {
        if (Args[i]->getType() != DataAuthFuncTy->getParamType(i)) {
            CommonHAKCAnalysis::getWriter() << "Types do not match at index " << std::to_string(i) << "\n";
            DataAuthFuncTy->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            Args[i]->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            throw std::exception();
        }
    }

    auto *DataAuthCall = CreateCall(HAKCAnalysis->HAKCDataAuthenticationName(), DataAuthFuncTy->getReturnType(), Args);

    if (HAKCPointer->getType()->isIntegerTy()) {
        HAKCPointerBitCast = HAKCIRBuilder.CreatePtrToInt(DataAuthCall, HAKCPointer->getType());
    } else {
        HAKCPointerBitCast = HAKCIRBuilder.CreateBitCast(DataAuthCall, HAKCPointer->getType());
    }

    return HAKCPointerBitCast;
}

Value *hakc::HAKCTransformer::CreateCodeAuthentication(Value *HAKCPointer, Instruction *I) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);
    auto AddrSpace = GetPointerAddrSpace(HAKCPointer);

    auto Args = CreateCodeAuthArguments(HAKCPointer, I);
    auto *AuthResult = CreateCall(HAKCAnalysis->HACKCodeAuthenticationName(), HAKCAuthenticationRetType(AddrSpace),
                                  Args);
    return HAKCIRBuilder.CreateBitCast(AuthResult, HAKCPointer->getType());
}

GlobalVariable *hakc::HAKCTransformer::GetValidTargetCompartments(Function *F) {
    auto Compartment = SystemInformation.findSymbol(F)->getCompartment();

    if (Compartment) {
        GlobalVariable *EntryTokenArray;
        auto CompartmentID = Compartment->getID();
        std::string name = "entry_tokens_" + std::to_string(CompartmentID);
        EntryTokenArray = getModule().getNamedGlobal(name);
        if (EntryTokenArray) {
            if (!EntryTokenArray->getValueType()->isArrayTy()) {
                CommonHAKCAnalysis::getWriter() << "Invalid type for ";
                EntryTokenArray->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
                throw std::exception();
            }
            return EntryTokenArray;
        }

        std::set<Constant *> EntryTokenValues;

        for (auto &t: Compartment->getTargets()) {
            Constant *EntryToken = GetEntryToken(t->getID());
            EntryTokenValues.insert(EntryToken);
        }
        if (EntryTokenValues.empty()) {
            CommonHAKCAnalysis::getWriter() << "No valid transitions exist for " << F->getName() << " in Compartment "
                                            << std::to_string(CompartmentID) << "\n";
            throw std::exception();
        }
        EntryTokenValues.insert(GetEntryToken(CompartmentID));

        Type *EntryTokenTy = GetEntryTokenType(GetPointerAddrSpace(*EntryTokenValues.begin()));

        for (auto *Token: EntryTokenValues) {
            if (Token->getType() != EntryTokenTy) {
                CommonHAKCAnalysis::getWriter() << "Token Type of ";
                Token->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << " (";
                Token->getType()->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << ")";
                CommonHAKCAnalysis::getWriter() << " does not match ";
                EntryTokenTy->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
                throw std::exception();
            }
        }
        std::vector<Constant*> TokenArray(EntryTokenValues.begin(), EntryTokenValues.end());

        auto *Initializer = ConstantArray::get(ArrayType::get(EntryTokenTy,
                                                              Compartment->getTargets().size()), TokenArray);

        EntryTokenArray = dyn_cast<GlobalVariable>(getModule().getOrInsertGlobal(name, Initializer->getType()));
        EntryTokenArray->setConstant(true);
        EntryTokenArray->setLinkage(GlobalValue::InternalLinkage);
        if(DebugIsActive()) {
            CommonHAKCAnalysis::getWriter() << "Setting initializer for " << EntryTokenArray->getName() << " to be ";
            Initializer->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " from token values ";
            for(auto *TokenValue : EntryTokenValues) {
                CommonHAKCAnalysis::getWriter() << "\n\t";
                TokenValue->print(CommonHAKCAnalysis::getWriter());
            }
            CommonHAKCAnalysis::getWriter() << "\n";
        }
        EntryTokenArray->setInitializer(Initializer);

        return EntryTokenArray;
    } else {
        CommonHAKCAnalysis::getWriter() << "Couldn't find Compartment for " << F->getName() << "\n";
        throw std::exception();
    }
}

CallInst *hakc::HAKCTransformer::CreateCall(StringRef name, Type *RetTy, ArrayRef<Value *> Args) {
    std::vector<Type *> FunctionParamTypes;
    for (auto *Arg: Args) {
        FunctionParamTypes.push_back(Arg->getType());
    }

    FunctionType *FunctionCallTy = FunctionType::get(RetTy, FunctionParamTypes, false);

    auto Func = HAKCAnalysis->GetFunctionCalleeByName(name, FunctionCallTy);
    if (!Func) {
        CommonHAKCAnalysis::getWriter() << "Could not find function " << name << " of type ";
        FunctionCallTy->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << " to be inserted into\n";
        HAKCIRBuilder.GetInsertBlock()->getParent()->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\n";
        throw std::exception();
    }
    return HAKCIRBuilder.CreateCall(Func, Args);
}

Instruction *
hakc::HAKCTransformer::CreateSizedCompartmentTransfer(Value *HAKCPointer, Instruction *I, Function *Target, bool IsData,
                                                      ConstantInt *Size) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);
    if (TargetIsKernel(Target)) {
        auto *V = CreateSafePointer(HAKCPointer, &*HAKCIRBuilder.GetInsertPoint());
        auto *SafePtr = dyn_cast<Instruction>(V);
        if (!SafePtr) {
            CommonHAKCAnalysis::getWriter() << "Unexpected Safe Pointer Type: ";
            V->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            throw std::exception();
        }
        return SafePtr;
    }

    if (HAKCPointerHasCustomTransfer(HAKCPointer)) {
        return CreateCustomTransfer(HAKCPointer, Target, IsData, Size);
    } else {
        return CreateDefaultTransfer(HAKCPointer, Target, IsData, Size);
    }
}

Instruction *
hakc::HAKCTransformer::CreateCustomTransfer(Value *HAKCPointer, Function *Target, bool IsData,
                                            ConstantInt *Size) {
    auto CustomTransfer = GetCustomTransferFunction(HAKCPointer);
    if (!CustomTransfer) {
        CommonHAKCAnalysis::getWriter() << "Could not find Transfer Function for ";
        HAKCPointer->getType()->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\n";
        throw std::exception();
    }

    auto TargetCompartment = SystemInformation.findSymbol(Target);
    return CustomTransfer->CreateTransfer(HAKCIRBuilder, TargetCompartment, HAKCPointer, Size, IsData);
}

bool hakc::HAKCTransformer::HAKCPointerHasCustomTransfer(Value *HAKCPointer) {
    return GetCustomTransferFunction(HAKCPointer) != nullptr;
}

std::shared_ptr<hakc::HAKCCustomTransfer> hakc::HAKCTransformer::GetCustomTransferFunctionForType(Type *HAKCType) {
    for (auto &it: HAKCAnalysis->GetHAKCCustomTransferFunctions()) {
        if (HAKCType == it->GetType()) {
            return it;
        }
    }
    return nullptr;
}

std::shared_ptr<hakc::HAKCCustomTransfer> hakc::HAKCTransformer::GetCustomTransferFunction(Value *HAKCPointer) {
    for (auto &it: HAKCAnalysis->GetHAKCCustomTransferFunctions()) {
        if (HAKCPointer->getType() == it->GetType()) {
            return it;
        }
    }
    return nullptr;
}

Instruction *
hakc::HAKCTransformer::CreateDefaultTransfer(Value *HAKCPointer,
                                             Function *Target,
                                             bool IsData, ConstantInt *Size) {
    auto FullArgSet = CreateTransferArguments(HAKCPointer, Target, IsData, Size);
    auto AddrSpace = GetPointerAddrSpace(HAKCPointer);
    bool IsPerCPU = CommonHAKCAnalysis::isPerCPUPointer(HAKCPointer);

    CallInst *TransferCall;
    if (IsPerCPU) {
        TransferCall = CreateCall(HAKCAnalysis->HAKCPerCPUCompartmentTransferName(), HAKCAuthenticationRetType
                                          (AddrSpace),
                                  FullArgSet);
    } else {
        TransferCall = CreateCall(HAKCAnalysis->HAKCCompartmentTransferName(), HAKCAuthenticationRetType(AddrSpace),
                                  FullArgSet);
    }

    Value *ResultCast;
    if (isa<PtrToIntInst>(HAKCPointer) || HAKCPointer->getType()->isIntegerTy()) {
        ResultCast = HAKCIRBuilder.CreatePtrToInt(TransferCall, HAKCPointer->getType());
    } else {
        ResultCast = HAKCIRBuilder.CreateBitCast(TransferCall, HAKCPointer->getType());
    }

    auto *Result = dyn_cast<Instruction>(ResultCast);
    if (!Result) {
        Result = TransferCall;
    }

    return Result;
}

/* Called with Values of type "void *" ("i8*") */
Type *hakc::HAKCTransformer::FindEntryBitcast(Value *V, Instruction *I, Function *Target) {
    /*
     * Checking V to see if it is an argument of the function that contains instruction I.
     * I is contained within a pass-generated HAKC_XFER function.
     *
     * If V is an argument, we determine V's argument index for the function.
     * The argument will have the same index in Target.
     */
    size_t index = (size_t)-1;

    for(size_t i = 0; i < Target->arg_size(); i++) {
        if (I->getFunction()->getArg(i) == V) {
             index = i;
             break;
        }
    }

    /* not an argument, nothing to return */
    if (index == (size_t)-1) {
        return nullptr;
    }

    /*
     * V was an argument.
     * Get the argument from Target that has the same argument index.
     */
    Value *targetV = (Target->getArg(index));

    bool bcoOperandDefOk = false;

    /*
     * Only guarantee of a Function's BasicBlock iterator is that the entry block will be returned first.
     *
     * Get the entry block and then iterate over the Instructions in the block.
     */
    for (Function::iterator i = Target->begin(), e = Target->end(); i != e; ++i) {
        BasicBlock *b = &*i;
        for (BasicBlock::iterator bi = b->begin(), be = b->end(); bi != be; ++bi) {
            /*
             * Looking for BitCastOperators in the entry block.
             * They may be a cast of the "void*" argument.
             */
            if (BitCastOperator *bco = dyn_cast<BitCastOperator>(&*bi)) {
                /* operand 0 is the value being casted */
                Value *bcoOperand = (bco->getOperand(0));
                /* dest type is the type that value gets casted to */
                Type *bcoType = (bco->getDestTy());

                if (bcoOperand == targetV) {
                    //bcoOperandDefOk = true;// = targetV;
                    //break;
                    return bcoType;
                }

#if 0
                /*
                 * the original analysis was too simplistic
                 * must follow back up def-use chain to def to see if the def was argument
                 * follow it backward for an instruction that uses the argument
                 *
                 * this will work in the event that
                 */
                Value *bcoOperandDef = nullptr;
                //CommonHAKCAnalysis::getWriter() << " " << *bcoOperandDef << "\n";
                for (auto *deflink: HAKCAnalysis->findDefChain(bcoOperand, true, true)) {
                    //CommonHAKCAnalysis::getWriter() << " " << *deflink << "\n";
                    if(auto *instr = dyn_cast<Instruction>(deflink)) {
                        unsigned operandCount = instr->getNumOperands();
                        for (unsigned opIndex = 0; opIndex < operandCount; opIndex++) {
                            //CommonHAKCAnalysis::getWriter() << "\t\t" << *instr->getOperand(opIndex) << "\n";
                            if (instr->getOperand(opIndex) == targetV) {
                                bcoOperandDef = instr->getOperand(opIndex);
                                break;
                            }
                        }
                        if(bcoOperandDef) {
                            break;
                        }
                    }
                }
#endif
                /*
                 * first check failed, nothing in the def chain has the argument in it
                 * this may be unoptimized code that does
                 *     %tmp     = alloca i8*
                 *                store i8* %arg, i8** %tmp
                 *     %voidarg = load i8*, i8** %tmp
                 *     %bcarg   = bitcast i8* %voidarg to %struct.type*
                 * instead of just directly bitcasting the argument
                 *
                 * the following code will only handle this case correctly in the event that
                 *   there is only the one level of indirection through memory
                 */
                if (!bcoOperandDefOk) {
                    /* first, get the def for the bitcast operand */
                    Value *actualOperandDef = HAKCAnalysis->getDef(bcoOperand, true, DebugIsActive());

                    /*
                     * if the def of the bitcast operand is:
                     *   %n = alloca i8*
                     * and the arg is:
                     *   %arg
                     * try to find a:
                     *   load i8*, i8** %n
                     * in the defChain that "starts" at the bitcast operand and "ends" at the def
                     * somewhere between the def and the load should be a:
                     *   store i8* %arg, i8** %n
                     * if that store can be found:
                     *   this is a found entry block void* arg -> struct* bitcast
                     */
                    if (auto *argPointerAlloca = dyn_cast<AllocaInst>(actualOperandDef)) {
                        /*
                         * if the bitcast operand def was alloca i8* (TODO check the type):
                         *   try to find a load into the def in the bitcast operand defChain
                         */
                        LoadInst *loadArgFromDefInstr = nullptr;

                        for (auto *deflink: HAKCAnalysis->findDefChain(bcoOperand, true, true)) {
                            if(auto *instr = dyn_cast<LoadInst>(deflink)) {
                                /*
                                 * found a load in the defChain
                                 */
                                if (instr->getOperand(0) == dyn_cast<Value>(argPointerAlloca)) {
                                    /*
                                     * this is what we wanted to find:
                                     *   a load into the bitcast operand's def
                                     */
                                    loadArgFromDefInstr = instr;
                                    break;
                                }
                            }
                        }
                        if (!loadArgFromDefInstr) {
                            return nullptr;
                        }

                        /*
                         * we know:
                         *   the def of the bitcast operand as %n (def instr)
                         *   the value %arg
                         * we found:
                         *   a load into %n (load instr)
                         * we still need to find:
                         *   a store of %arg into %n, between def instr and load instr
                         */
                        Instruction *actualOpDefInstr = dyn_cast<Instruction>(actualOperandDef);
                        Instruction *next = actualOpDefInstr->getNextNode();
                        /* search from first instruction after the def, until we reach the load */
                        while (next != dyn_cast<Instruction>(loadArgFromDefInstr)) {
                            if (auto *instr = dyn_cast<StoreInst>(next)) {
                                /*
                                 * we found a store between the def and the load
                                 */
                                if ((instr->getOperand(0) == targetV) &&
                                    (instr->getOperand(1) == dyn_cast<Value>(argPointerAlloca))) {
                                    /*
                                     * this what we wanted to find:
                                     *   a store of the function argument into the bitcast operand's def
                                     */
                                    bcoOperandDefOk = true;// = targetV;
                                    break;
                                }
                            }
                            next = next->getNextNode();
                        }
                    }
                }

                /* This Bitcast is Operating on the argument from Target. */
                if (bcoOperandDefOk) {// && bcoOperandDef == targetV) {
                    //if (DebugIsActive()) {
                        CommonHAKCAnalysis::getWriter() << "Target argument is being bitcast:\n";
                        CommonHAKCAnalysis::getWriter() << " " << *targetV /* *bcoOperandDef*/ << "\n";
                        CommonHAKCAnalysis::getWriter() << " " << *bcoType << "\n";
                    //}

                    /* Dest type is what will be used to create a Typed transfer. */
                    return bcoType;
                }
            }
        }
        /* no bitcast found in first block, nothing to return */
        return nullptr;
    }

    /* control reaches end of non-void function, nothing to return */
    return nullptr;
}

/**
 * Sometimes, a function will take a "void *" ("i8*") parameter and immediately cast it to some destination type (struct).
 * Sometimes, the destination type (struct) has a custom transfer function.
 *
 * If we want to make this work, we need to do a little extra work.
 *
 * This function will try to find a custom transfer function by Type instead of from the Value (which is of type "i8*")
 * and generate a call to the custom function instead of "hakc_transfer_to_clique".
 */
Instruction *hakc::HAKCTransformer::CreateVoidCastCompartmentTransfer(Value *HAKCPointer, Instruction *I, Function *Target, Type *TypeToUse) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);

    /* just give safe pointer to kernel targets */
    if (TargetIsKernel(Target)) {
        auto *V = CreateSafePointer(HAKCPointer, &*HAKCIRBuilder.GetInsertPoint());
        auto *SafePtr = dyn_cast<Instruction>(V);
        if (!SafePtr) {
            CommonHAKCAnalysis::getWriter() << "Unexpected Safe Pointer Type: ";
            V->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            throw std::exception();
        }
        return SafePtr;
    }

    uint64_t size = 0;

    /*
     * I am sorry about this code.
     *
     * I TRIED to find a way to go from "struct.mytype*" to "struct.mytype"
     * through the LLVM API. The one thing I thought should work ("getPointerElementType")
     * would always return "i8*". I gave up and put together this ugly hack.
     */
    std::string StructTypeName;
    llvm::raw_string_ostream rso(StructTypeName);
    TypeToUse->print(rso);

    /* get rid of "%" at beginning, "*" at end */
    StructTypeName = StructTypeName.substr(1, StructTypeName.length() - 2);

    /* use StructTypeName to find StructType by name */
    Type *bcoType = StructType::getTypeByName(HAKCIRBuilder.getContext(), llvm::StringRef(StructTypeName));
    if (!bcoType) {
        CommonHAKCAnalysis::getWriter() << "Could not get StructType by name for:\n" << StructTypeName;
        CommonHAKCAnalysis::getWriter() << "\n";
        throw std::exception();
    }

    /* the type has to be a struct */
    auto *StructTy = dyn_cast<StructType>(bcoType);
    if (!StructTy) {
        CommonHAKCAnalysis::getWriter() << "Unexpected (non-struct) cast type:\n";
        TypeToUse->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\n";
        throw std::exception();
    }

    auto *StructDITy = DebugInfoProcessor.findDiType(StructTy);
    if (StructDITy) {
        size = DebugInfoProcessor.getDITypeSizeInBits(StructDITy);
    } else {
        size = StructTy->getScalarSizeInBits();
    }

    if(size == 0) {
        CommonHAKCAnalysis::getWriter() << "Unexpected (zero) dest-cast struct size:\n" << size;
        CommonHAKCAnalysis::getWriter() << "\n";
        throw std::exception();
    }

    auto TargetCompartment = SystemInformation.findSymbol(Target);

    /*
     * at this point, we know the dest type is a struct* and we know the actual size
     *
     * even if a custom transfer function doesn't exist for the struct type, we can do a
     * more accurate transfer than the previous void* single-byte transfer
     */

    if(auto CustomTransfer = GetCustomTransferFunctionForType(TypeToUse)) {
        /* custom transfer exists, give the most specific transfer possible */
        Instruction *customXfer = CustomTransfer->CreateTransferWithCasts(HAKCIRBuilder, TargetCompartment, HAKCPointer, HAKCIRBuilder.getInt64(size / BITS_PER_BYTE), HAKCPointer->getType(), TypeToUse);

        if (DebugIsActive()) {
            CommonHAKCAnalysis::getWriter() << "Struct type name: " << StructTypeName << "\n";
            CommonHAKCAnalysis::getWriter() << "LLVM type: " << *bcoType << "\n";
            CommonHAKCAnalysis::getWriter() << "size of struct: " << size << "\n";
            CommonHAKCAnalysis::getWriter() << "custom xfer result:\n";
            CommonHAKCAnalysis::getWriter() << *customXfer << "\n";
        }
        return customXfer;
    }
    else {
        /* no custom transfer exists, give the next-most specific transfer possible, correctly-sized generic transfer */
        Instruction *sizedXfer = CreateSizedCompartmentTransfer(HAKCPointer, I, Target, true, HAKCIRBuilder.getInt64(size / BITS_PER_BYTE));

        if (DebugIsActive()) {
            CommonHAKCAnalysis::getWriter() << "Struct type name: " << StructTypeName << "\n";
            CommonHAKCAnalysis::getWriter() << "LLVM type: " << *bcoType << "\n";
            CommonHAKCAnalysis::getWriter() << "size of struct: " << size << "\n";
            CommonHAKCAnalysis::getWriter() << "sized xfer result:\n";
            CommonHAKCAnalysis::getWriter() << *sizedXfer << "\n";
        }
        return sizedXfer;
    }

    /* control reaches end of non-void function, this leads to a generic single-byte transfer being created for the void* */
    return nullptr;
}

Instruction *hakc::HAKCTransformer::CreateCompartmentTransfer(Value *HAKCPointer,
                                                              Instruction *I,
                                                              Function *Target,
                                                              bool IsData) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);

    auto ObjectSize = GetObjectSizeInBytes(HAKCPointer);

    /*
     * If HAKCPointer is of type "void *" ("i8*"), ObjectSize will be 1.
     * I don't think this is the only case where that happens, so we do another check.
     * Only care about data transfer.
     */
    if(ObjectSize == HAKCIRBuilder.getInt64(1) && IsData) {
        /*
         * TODO
         *
         * This functionality could and should be extended.
         *
         * Currently it is limited to checking if a transferred "void *" function argument
         * gets used as a different type in the entry basic block of the Target function.
         *
         * This code probably only behaves as expected if there is a single bitcast of the
         * argument in the entry block.
         * I do not know what would happen if there were more than one.
         *
         * Furthermore, if the bitcast did not occur until a later block, this implementation
         * will not find it. A single-byte "hakc_transfer_to_clique" call will be emitted.
         * This is probably not the desired behavior. This should be extended to follow
         * argument's use-chain through entire Target function.
         *
         * Another issue that could arise is an argument being bitcast multiple times, 
         * to different dest types each time. This is usually due to the following:
         *
         * HAKCPointer ("argument") is a void* ("func(void *argument) {")
         * It gets bitcast to some "struct.realtype*" ("casted_argument = (struct realtype*)argument;")
         * The first field of "struct.realtype" is some other type "other_type";
         * offsetof(struct realtype, other_type_field) == 0
         * Some code in Target uses "casted_argument" AND "casted_argument->other_type_field"
         *
         * The IR will contain
         * %a = bitcast i8* %0, %struct.realtype*
         * ...
         * %b = bitcast i8* %0, %other_type
         * ...
         *
         * More sophisticated analysis would be required to figure out if this is happening
         * and only create the struct.realtype transfer.
         */

        /* look for a bitcast from i8* to a struct type in the entry basic block of Target */
        Type *EntryCastType = FindEntryBitcast(HAKCPointer, I, Target);
        /*
         * If a bitcast is found in entry block, Target takes HAKCPointer as "void*" but
         * immediately uses it as if it some other type.
         * This situation is often found in functions used to run kthreads.
         */
        if (EntryCastType) {
            if (DebugIsActive()) {
                CommonHAKCAnalysis::getWriter() << Target->getName() << " has entry block bitcast from void* to:\n";
                EntryCastType->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
            }

            /* Void* -> Struct* cast compartment transfers do things slightly differently. */
            auto *castTransfer = CreateVoidCastCompartmentTransfer(HAKCPointer, I, Target, EntryCastType);
            if (castTransfer) {
                return castTransfer;
            }
        }
    }

    if (!ObjectSize) {
        if (DebugIsActive()) {
            CommonHAKCAnalysis::getWriter() << "Could not get ObjectSize for ";
            HAKCPointer->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }

        ObjectSize = GetDefaultObjectSize();
    }

    return CreateSizedCompartmentTransfer(HAKCPointer, I, Target, IsData, ObjectSize);
}

Function *hakc::HAKCTransformer::GetTransferFunction(Function *F) {
    auto TransferFunctionName = HAKCAnalysis->getOutsideTransferName(F);
    auto *TransferFunction = HAKCAnalysis->GetFunctionByName(TransferFunctionName, F->getFunctionType());
    if (TransferFunction == nullptr) {
        CommonHAKCAnalysis::getWriter() << "Could not create HAKC transfer function " << TransferFunctionName << "\n";
        throw std::exception();
    }

    TransferFunction->setCallingConv(F->getCallingConv());
    TransferFunction->setLinkage(F->getLinkage());
    TransferFunction->copyAttributesFrom(F);
    TransferFunction->setSection(F->getSection());

    return TransferFunction;
}

bool hakc::HAKCTransformer::NoKernelTransfers(Function *Target) {
    return HAKCAnalysis->IsKernelFunction(Target) &&
           !CommonHAKCAnalysis::NoKernelTransferFunctionsSet();
}

std::vector<Value *>
hakc::HAKCTransformer::CreateForwardArgumentTransfers(Function *Target, Function *TransferFunction) {
    std::vector<Value *> TransferredArguments;

    bool NoKernelXfers = NoKernelTransfers(Target);

    for (auto Arg = TransferFunction->arg_begin(); Arg != TransferFunction->arg_end(); Arg++) {
        if (!CommonHAKCAnalysis::argShouldTransfer(Arg) || NoKernelXfers) {
            TransferredArguments.push_back(Arg);
            continue;
        }
        bool IsData = !Arg->getType()->getPointerElementType()->isFunctionTy();
        CreateTransferFunctionArg_PreCall(Target, TransferFunction, Arg);
        Instruction *Transfer = CreateCompartmentTransfer(Arg, &*HAKCIRBuilder.GetInsertPoint(), Target, IsData);
        TransferredArguments.push_back(Transfer);
    }

    return TransferredArguments;
}

void hakc::HAKCTransformer::CreateBackwardArgumentTransfers(Function *Target, Function *TransferFunction) {
    bool NoKernelXfers = NoKernelTransfers(Target);

    for (auto Arg = TransferFunction->arg_begin(); Arg != TransferFunction->arg_end(); Arg++) {
        if (!CommonHAKCAnalysis::argShouldTransfer(Arg) || NoKernelXfers) {
            continue;
        }
        CreateTransferFunctionArg_PostCall(Target, TransferFunction, Arg);
    }
}

Function *hakc::HAKCTransformer::CreateTransferFunction(Function *F) {
    if (F->isIntrinsic()) {
        CommonHAKCAnalysis::getWriter() << "Trying to create a HAKC Transfer function for " << F->getName() << "\n";
        throw std::exception();
    } else if (F->isVarArg()) {
        CommonHAKCAnalysis::getWriter() << "Trying to create HAKC Transfer function for variadic function " <<
                                        F->getName() << "\n";
        throw std::exception();
    }

    Function *TransferFunction = CreateNonVariadicTransferFunction(F);

    return TransferFunction;
}

Function *hakc::HAKCTransformer::CreateTransferToVariadic(CallInst *Call) {
    auto *Target = Call->getCalledFunction();
    if (!Target) {
        CommonHAKCAnalysis::getWriter() << "Null Call target\n";
        throw std::exception();
    }
    if (Target->isIntrinsic()) {
        CommonHAKCAnalysis::getWriter() << "Trying to create a HAKC Transfer function for " << Target->getName() <<
                                        "\n";
        throw std::exception();
    }

    std::vector<Type *> ArgTypes;
    for (auto &Arg: Call->args()) {
        ArgTypes.push_back(Arg->getType());
    }

    FunctionType *TransferType = FunctionType::get(Target->getReturnType(), ArgTypes, false);
    Function *TransferFunction = nullptr;
    unsigned TargetTransferCount = 0;

    for (auto &it: VariadicTransferFunctions) {
        Function *Transfer = it.first;
        Function *TransferTarget = it.second;
        if (TransferTarget == Target) {
            TargetTransferCount += 1;
        }

        if (Transfer->getFunctionType() == TransferType && TransferTarget == Target) {
            TransferFunction = Transfer;
        }
    }

    if (!TransferFunction) {
        auto TransferName = HAKCAnalysis->getVariadicTransferName(Target);
        TransferName += "_";
        TransferName += std::to_string(TargetTransferCount);

        TransferFunction = HAKCAnalysis->GetFunctionByName(TransferName, TransferType);
        PopulateTransferFunction(Target, TransferFunction);
        TransferFunction->setLinkage(GlobalValue::PrivateLinkage);
        VariadicTransferFunctions[TransferFunction] = Target;
    }

    return TransferFunction;
}

Function *hakc::HAKCTransformer::PopulateTransferFunction(Function *Target, Function *TransferFunction) {
    if (!TransferFunction->empty()) {
        return TransferFunction;
    }

    BasicBlock *EntryBB = BasicBlock::Create(getModule().getContext(), "HAKCTransferEntry", TransferFunction);
    if (EntryBB != &TransferFunction->getEntryBlock()) {
        CommonHAKCAnalysis::getWriter() << "Invalid Entry BasicBlock created\n";
        throw std::exception();
    }

    HAKCIRBuilder.SetInsertPoint(EntryBB);
    // Create a temporary terminator
    auto *Unreachable = HAKCIRBuilder.CreateUnreachable();
    HAKCIRBuilder.SetInsertPoint(Unreachable);

    auto TransferredArguments = CreateForwardArgumentTransfers(Target, TransferFunction);
    CallInst *TargetFunctionCall = HAKCIRBuilder.CreateCall(Target, TransferredArguments);

    CreateBackwardArgumentTransfers(Target, TransferFunction);

    if (!Target->getReturnType()->isVoidTy()) {
        HAKCIRBuilder.CreateRet(TargetFunctionCall);
    } else {
        HAKCIRBuilder.CreateRetVoid();
    }
    Unreachable->eraseFromParent();

    CreateTransferFunctionFinalize_Arch(Target, TransferFunction);

    if (llvm::verifyFunction(*TransferFunction, &CommonHAKCAnalysis::getWriter())) {
        CommonHAKCAnalysis::getWriter() << "Function verification for transfer function failed\n";
        TransferFunction->print(CommonHAKCAnalysis::getWriter(), nullptr);
        throw std::exception();
    }

    return TransferFunction;
}

Function *hakc::HAKCTransformer::CreateNonVariadicTransferFunction(Function *F) {
    if (F->isIntrinsic()) {
        CommonHAKCAnalysis::getWriter() << "Trying to create a HAKC Transfer function for " << F->getName() << "\n";
        throw std::exception();
    }

    auto *TransferFunction = GetTransferFunction(F);
    if (!TransferFunction->empty() || !HAKCAnalysis->TransferFunctionShouldBeCreated(F)) {
        return TransferFunction;
    }

    return PopulateTransferFunction(F, TransferFunction);
}

Value *hakc::HAKCTransformer::CreateBitCast(Value *Operand, Type *TargetType, Instruction *I) {
    ValidateHAKCPointerAndLocation(Operand, I);
    auto AddrSpace = GetPointerAddrSpace(Operand);

    if (TargetType->isPointerTy() && TargetType->getPointerAddressSpace() != AddrSpace) {
        CommonHAKCAnalysis::getWriter() << "TargetType ";
        TargetType->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << " has AddrSpace when casting ";
        Operand->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\n";
        I->getFunction()->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\n";
        throw std::exception();
    }
    Value *BitCast;
    if (Operand->getType()->isIntegerTy() && TargetType->isPointerTy()) {
        BitCast = HAKCIRBuilder.CreateIntToPtr(Operand, TargetType);
    } else if (Operand->getType()->isPointerTy() && TargetType->isIntegerTy()) {
        BitCast = HAKCIRBuilder.CreatePtrToInt(Operand, TargetType);
    } else {
        BitCast = HAKCIRBuilder.CreateBitCast(Operand, TargetType);
    }
    return BitCast;
}


ConstantInt *hakc::HAKCTransformer::GetObjectSizeInBytes(Value *V) {
    uint64_t size = 0;
    auto *VTy = V->getType();
    if (V->getType()->isPointerTy() && V->getType()->getPointerElementType()->isIntegerTy(8)) {
        return HAKCIRBuilder.getInt64(1);
    }

    if (auto *AllocaI = dyn_cast<AllocaInst>(V)) {
        VTy = AllocaI->getAllocatedType();
    }
    if (VTy->isPointerTy()) {
        VTy = VTy->getPointerElementType();
    }

    if (auto *StructTy = dyn_cast<StructType>(VTy)) {
        auto *StructDITy = DebugInfoProcessor.findDiType(StructTy);
        if (StructDITy) {
            size = DebugInfoProcessor.getDITypeSizeInBits(StructDITy);
        } else {
            size = StructTy->getScalarSizeInBits();
        }
    } else if (auto *IntegerTy = dyn_cast<IntegerType>(VTy)) {
        size = IntegerTy->getBitWidth();
    } else if (isa<PointerType>(VTy)) {
        size = CommonHAKCAnalysis::getCompartmentStorageSizeInBits();
    }

    if (size > 0) {
        return HAKCIRBuilder.getInt64(size / BITS_PER_BYTE);
    } else {
        return nullptr;
    }
}

Type *hakc::HAKCTransformer::HAKCAuthenticationRetType(unsigned AddrSpace) {
    return HAKCIRBuilder.getInt8PtrTy(AddrSpace);
}

hakc::hakc_compartment_id_t hakc::HAKCTransformer::getFunctionCompartmentID(Function *F) {
    return getSymbolCompartmentID(F);
}

hakc::hakc_compartment_id_t hakc::HAKCTransformer::getGlobalCompartmentID(GlobalVariable *GV) {
    return getSymbolCompartmentID(GV);
}

hakc::hakc_compartment_id_t hakc::HAKCTransformer::getSymbolCompartmentID(GlobalValue *GV) {
    if (!GV) {
        CommonHAKCAnalysis::getWriter() << "GV is null when trying to get compartment ID\n";
        throw std::exception();
    }
    auto Symbol = SystemInformation.findSymbol(GV);
    if (!Symbol) {
        return KERNEL_COMPARTMENT;
    }
    return Symbol->getCompartmentID();
}

ConstantInt *hakc::HAKCTransformer::getTrue() {
    return HAKCIRBuilder.getTrue();
}

ConstantInt *hakc::HAKCTransformer::getFalse() {
    return HAKCIRBuilder.getFalse();
}

ConstantInt *hakc::HAKCTransformer::getInt64(int64_t Value) {
    return HAKCIRBuilder.getInt64(Value);
}

ConstantInt *hakc::HAKCTransformer::getInt32(int32_t Value) {
    return HAKCIRBuilder.getInt32(Value);
}

ConstantInt *hakc::HAKCTransformer::GetDefaultObjectSize() {
    return getInt64(1);
}

bool hakc::HAKCTransformer::TargetIsKernel(Function *Target) {
    return HAKCAnalysis->IsKernelFunction(Target);
}

ConstantInt *hakc::HAKCTransformer::GetHAKCCompartmentValue(hakc_compartment_id_t CompartmentID) {
    return HAKCIRBuilder.getIntN(COMPARTMENT_ID_BIT_LENGTH, CompartmentID);
}

void hakc::HAKCTransformer::CreateTransferFunctionArg_PreCall(Function *Target, Function *TransferFunction, Value *Arg) {
        if (TargetIsKernel(Target)) {
            return;
        }

        auto *SaveColorInst = SaveColor(Arg);
        TransferArgumentsToRestore[Arg] = SaveColorInst;
    }

void hakc::HAKCTransformer::CreateTransferFunctionArg_PostCall(Function *F, Function *TransformFunction, Value *Arg) {
    // if (TargetIsKernel(Target)) {
    //     return;
    // }
    // CallInst *SavedColor = TransferArgumentsToRestore[Arg];
    // auto *Size = GetObjectSizeInBytes(Arg);
    // if (!Size) {
    //     Size = GetDefaultObjectSize();
    // }
    // auto AddrSpace = GetPointerAddrSpace(Arg);
    // Value *Args[] = {
    //         HAKCIRBuilder.CreateBitCast(Arg, HAKCIRBuilder.getInt8PtrTy(AddrSpace)),
    //         SavedColor,
    //         Size
    // };
    // CreateCall(HAKCColorAddressName(), HAKCIRBuilder.getVoidTy(), Args);
}

bool hakc::HAKCTransformer::FunctionIsExported(Function *F) {
    auto ksym_name = getKstrtab_entry_name(F);
    /* Add colon so __kstrtab_foo_1 doesn't match __kstrtab_foo */
    ksym_name += ":";
    return getModule().getModuleInlineAsm().find(ksym_name) != getModule().getModuleInlineAsm().npos;
}

unsigned hakc::HAKCTransformer::GetPointerAddrSpace(Value *V) {
    unsigned AddrSpace = 0;
    if (V->getType()->isPointerTy()) {
        AddrSpace = V->getType()->getPointerAddressSpace();
    }
    return AddrSpace;
}

GlobalVariable *hakc::HAKCTransformer::AddCompartmentMetadataEntry(hakc::hakc_compartment_id_t CompartmentID) {
    return nullptr;
}

bool hakc::HAKCTransformer::DebugIsActive() {
    return HAKCIRBuilder.GetInsertPoint()->getFunction()->getName() == CommonHAKCAnalysis::getHAKCDebugName();
}
