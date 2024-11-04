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
#include "HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"

hakc::HAKCTransformer::HAKCTransformer(HAKCCompartmentalizationPolicy &Policy, HAKCModuleAnalysis &HAKCAnalysis, HAKCTypeIdentifier &TypeIdentifier) :
        HAKCIRBuilder(HAKCAnalysis.GetModule().getContext()),
        CompartmentalizationPolicy(Policy),
        ModuleAnalysis(HAKCAnalysis),
        TypeIdentifier(TypeIdentifier),
        VariadicTransferFunctions() {

}

Module &hakc::HAKCTransformer::getModule() {
    return ModuleAnalysis.GetModule();
}


void hakc::HAKCTransformer::ValidateLocation(Instruction *I) {
    if (I == nullptr) {
        CommonHAKCAnalysis::getWriter() << "I is null\n";
        throw std::exception();
    }
    HAKCIRBuilder.SetInsertPoint(I);
}

void hakc::HAKCTransformer::ValidateHAKCPointer(ManagedHAKCPointerP HAKCPointer) {
    if (!HAKCPointer) {
        CommonHAKCAnalysis::getWriter() << "ManagedHAKCPointer is null\n";
        throw std::exception();
    }

//    bool IsPointerLike = (HAKCPointer->getType()->isPointerTy() || ValidateHAKCIntegerPointerSize(HAKCPointer));
//
//    if (!IsPointerLike) {
//        CommonHAKCAnalysis::getWriter() << "ManagedHAKCPointer ";
//        HAKCPointer->print(CommonHAKCAnalysis::getWriter());
//        CommonHAKCAnalysis::getWriter() << " is not Pointer-like!\n";
//        for (auto *deflink: ModuleAnalysis.findDefChain(HAKCPointer, false, true)) {
//            CommonHAKCAnalysis::getWriter() << "\t";
//            deflink->print(CommonHAKCAnalysis::getWriter());
//            CommonHAKCAnalysis::getWriter() << "\n";
//        }
//        throw std::exception();
//    }
}

//bool hakc::HAKCTransformer::ValidateHAKCIntegerPointerSize(ManagedHAKCPointerP HAKCPointer) {
//    return HAKCPointer->getType()->isIntegerTy(64);
//}

void hakc::HAKCTransformer::ValidateHAKCPointerAndLocation(const ManagedHAKCPointerP &HAKCPointer, Instruction *I) {
    try {
        ValidateHAKCPointer(HAKCPointer);
        ValidateLocation(I);
    } catch (std::exception &e) {
        if (I) {
            CommonHAKCAnalysis::getWriter() << "Validation failed for " << *HAKCPointer << " for Instruction in "
                                            << I->getFunction()->getName() << ": " << *I << "\n";
            throw e;
        }
    }
}

Value *hakc::HAKCTransformer::CreateSafePointer(ManagedHAKCPointerP HAKCPointer, Instruction *I) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);

    if (isa<PHINode>(I)) {
        CommonHAKCAnalysis::getWriter() << "Trying to insert data auth check at " << *I << " for " << *HAKCPointer
                                        << "\n" << *I->getFunction() << "\n";
        throw std::exception();
    } else if (isa<ConstantPointerNull>(HAKCPointer->GetBaseDefinition())) {
        CommonHAKCAnalysis::getWriter() << "ManagedHAKCPointer is a ConstantPointerNull: " << *HAKCPointer << "\n";
        throw std::exception();
    }

    if (HAKCPointer->GetAuthenticatedPointer()) {
        return HAKCPointer->GetAuthenticatedPointer();
    }

    auto *SafePtr = CreateSafePointer_Arch(HAKCPointer, I);
/*    if (SafePtr->getType() != HAKCPointer->getType()) {
        CommonHAKCAnalysis::getWriter() << "SafePtr and ManagedHAKCPointer are not the same Type!\n"
                                        << "SafePtr: ";
        SafePtr->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\nManagedHAKCPointer: ";
        HAKCPointer->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\n";
        throw std::exception();
    }*/
    HAKCPointer->SetAuthenticatedPointer(SafePtr);
    return SafePtr;
}

Value *hakc::HAKCTransformer::CreateDataAuthentication(ManagedHAKCPointerP HAKCPointer, Instruction *I) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);

    if (HAKCPointer->GetAuthenticatedPointer()) {
        return HAKCPointer->GetAuthenticatedPointer();
    }

    if (isa<PHINode>(I)) {
        CommonHAKCAnalysis::getWriter() << "Trying to insert data auth check at " << *I << " for " << *HAKCPointer
                                        << "\n" << *I->getFunction();
        throw std::exception();
    }

    Value *HAKCPointerBitCast;
    SmallVector<Value *> Args;
    unsigned AddrSpace = GetPointerAddrSpace(HAKCPointer);
    auto *DataAuthFuncTy = GetHAKCDataAuthenticationFunctionType(AddrSpace);
    CreateDataAuthArguments(HAKCPointer, I, Args);
    for (unsigned i = 0; i < DataAuthFuncTy->getNumParams(); i++) {
        if (Args[i]->getType() != DataAuthFuncTy->getParamType(i)) {
            CommonHAKCAnalysis::getWriter() << "Types do not match at index " << std::to_string(i) << "\n"
                                            << *DataAuthFuncTy << "\n" << *Args[i] << "\n";
            throw std::exception();
        }
    }

    auto *DataAuthCall = CreateCall(ModuleAnalysis.HAKCDataAuthenticationName(), DataAuthFuncTy->getReturnType(), Args);
    HAKCPointerBitCast = CreateReturnCast(HAKCPointer, DataAuthCall);
    HAKCPointer->SetAuthenticatedPointer(HAKCPointerBitCast);

    return HAKCPointerBitCast;
}

Value *hakc::HAKCTransformer::CreateReturnCast(hakc::ManagedHAKCPointerP HAKCPointer, Value *V) {
    if (!V) {
        CommonHAKCAnalysis::getWriter() << "NULL V\n";
        throw std::exception();
    }
    if (HAKCPointer->GetBaseDefinition()->getType()->isIntegerTy()) {
        return HAKCIRBuilder.CreatePtrToInt(V, HAKCPointer->GetBaseDefinition()->getType());
    } else {
        return HAKCIRBuilder.CreateBitCast(V, HAKCPointer->GetBaseDefinition()->getType());
    }
}

Value *hakc::HAKCTransformer::CreatePointerCast(hakc::ManagedHAKCPointerP HAKCPointer, PointerType *PointerTy) {
    if (!PointerTy) {
        CommonHAKCAnalysis::getWriter() << "NULL PointerTy\n";
        throw std::exception();
    }

    if (HAKCPointer->GetBaseDefinition()->getType()->isIntegerTy()) {
        return HAKCIRBuilder.CreateIntToPtr(HAKCPointer->GetBaseDefinition(), PointerTy);
    } else {
        return HAKCIRBuilder.CreateBitCast(HAKCPointer->GetBaseDefinition(), PointerTy);
    }
}

Value *hakc::HAKCTransformer::CreateCodeAuthentication(hakc::ManagedHAKCPointerP HAKCPointer, Instruction *I) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);
    auto AddrSpace = GetPointerAddrSpace(HAKCPointer);

    SmallVector<Value *> Args;
    CreateCodeAuthArguments(HAKCPointer, I, Args);
    auto *AuthResult = CreateCall(ModuleAnalysis.HACKCodeAuthenticationName(), HAKCAuthenticationRetType(AddrSpace),
                                  Args);
    auto *BitCast = HAKCIRBuilder.CreateBitCast(AuthResult, HAKCPointer->GetBaseDefinition()->getType());
    return BitCast;
}

GlobalVariable *hakc::HAKCTransformer::GetValidTargetCompartments(Function *F) {
    auto Division = CompartmentalizationPolicy.GetDivision(F);

    GlobalVariable *EntryTokenArray;
    auto CompartmentID = Division.GetHAKCCompartment().GetCompartmentID();
    std::string name = "entry_tokens_" + std::to_string(CompartmentID->getZExtValue());
    EntryTokenArray = getModule().getNamedGlobal(name);
    if (EntryTokenArray) {
        if (!EntryTokenArray->getValueType()->isArrayTy()) {
            CommonHAKCAnalysis::getWriter() << "Invalid type for " << *EntryTokenArray << "\n";
            throw std::exception();
        }
        return EntryTokenArray;
    }

    auto Targets = Division.GetHAKCCompartment().GetValidTargets();
    if (Targets.empty()) {
        CommonHAKCAnalysis::getWriter() << "No valid transitions exist for " << F->getName() << " in Compartment "
                                        << std::to_string(CompartmentID->getZExtValue()) << "\n";
        throw std::exception();
    }

    SmallVector<Constant *> EntryTokenValues;
    SmallVector<hakc_compartment_id_t> IDs;
    IDs.push_back(CompartmentID->getZExtValue());
    for (auto &t: Targets) {
        IDs.push_back(t->getZExtValue());
    }
    llvm::sort(IDs.begin(), IDs.end(),
               [](hakc_compartment_id_t LHS, hakc_compartment_id_t RHS) { return LHS < RHS; });

    for (auto ID: IDs) {
        auto TargetCompartment = CompartmentalizationPolicy.GetCompartment(ID);
        Constant *EntryToken = GetEntryToken(*TargetCompartment);
        EntryTokenValues.push_back(EntryToken);
    }

    Type *EntryTokenTy = GetEntryTokenType(GetPointerAddrSpace(*EntryTokenValues.begin()));

    for (auto *Token: EntryTokenValues) {
        if (Token->getType() != EntryTokenTy) {
            CommonHAKCAnalysis::getWriter() << "Token Type of " << *Token << " (" << *Token->getType()
                                            << ") does not match " << *EntryTokenTy << "\n";
            throw std::exception();
        }
    }

    auto *Initializer = ConstantArray::get(ArrayType::get(EntryTokenTy, EntryTokenValues.size()), EntryTokenValues);

    EntryTokenArray = dyn_cast<GlobalVariable>(getModule().getOrInsertGlobal(name, Initializer->getType()));
    EntryTokenArray->setConstant(true);
    EntryTokenArray->setLinkage(GlobalValue::InternalLinkage);
    if (DebugIsActive()) {
        CommonHAKCAnalysis::getWriter() << "Setting initializer for " << EntryTokenArray->getName() << " to be " << Initializer << " from token values ";
        for (auto *TokenValue: EntryTokenValues) {
            CommonHAKCAnalysis::getWriter() << "\n\t" << TokenValue;
        }
        CommonHAKCAnalysis::getWriter() << "\n";
    }
    EntryTokenArray->setInitializer(Initializer);

    return EntryTokenArray;
}

CallInst *hakc::HAKCTransformer::CreateCall(StringRef name, Type *RetTy, ArrayRef<Value *> Args) {
    std::vector<Type *> FunctionParamTypes;
    for (auto *Arg: Args) {
        FunctionParamTypes.push_back(Arg->getType());
    }

    FunctionType *FunctionCallTy = FunctionType::get(RetTy, FunctionParamTypes, false);

    auto Func = ModuleAnalysis.GetFunctionCalleeByName(name, FunctionCallTy);
    if (!Func) {
        CommonHAKCAnalysis::getWriter() << "Could not find function " << name << " of type " << FunctionCallTy << " to be inserted into\n" << HAKCIRBuilder.GetInsertBlock()->getParent() << "\n";
        throw std::exception();
    }
    auto *Call = HAKCIRBuilder.CreateCall(Func, Args);

    /* The LLVM function checker throws an error when an inline-able function with debug info contains a function
    * call with no debug information.  So try to set the appropriate debug info for this transfer */
    if (!Call->getDebugLoc()) {
        auto *I = &*HAKCIRBuilder.GetInsertPoint();
        if (I->getDebugLoc()) {
            Call->setDebugLoc(I->getDebugLoc());
        } else {
            /* Use the closest debug info to I */
            bool PastI = false;
            for (auto BBI = I->getParent()->begin(), BBE = I->getParent()->end(); BBI != BBE; ++BBI) {
                if (BBI->getDebugLoc()) {
                    Call->setDebugLoc(BBI->getDebugLoc());
                }
                if (&*BBI == I) {
                    PastI = true;
                }
                if (PastI && Call->getDebugLoc()) {
                    break;
                }
            }
        }
    }


    return Call;
}

Instruction *
hakc::HAKCTransformer::CreateSizedCompartmentTransfer(hakc::ManagedHAKCPointerP HAKCPointer, Instruction *I,
                                                      GlobalValue *Target,
                                                      bool IsData,
                                                      ConstantInt *Size) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);
    Instruction *Transfer;
    if (TargetIsKernel(Target)) {
        auto *V = CreateSafePointer(HAKCPointer, &*HAKCIRBuilder.GetInsertPoint());
        auto *SafePtr = dyn_cast<Instruction>(V);
        if (!SafePtr) {
            CommonHAKCAnalysis::getWriter() << "Unexpected Safe Pointer Type: " << *V << "\n";
            throw std::exception();
        }
        return SafePtr;
    }

    if (HAKCPointerHasCustomTransfer(HAKCPointer)) {
        Transfer = CreateCustomTransfer(HAKCPointer, Target, IsData, Size);
    } else {
        Transfer = CreateDefaultTransfer(HAKCPointer, Target, IsData, Size);
    }

    return Transfer;
}

Instruction *
hakc::HAKCTransformer::CreateCustomTransfer(hakc::ManagedHAKCPointerP HAKCPointer, GlobalValue *Target, bool IsData,
                                            ConstantInt *Size) {
    auto CustomTransfer = GetCustomTransferFunction(HAKCPointer);
    if (!CustomTransfer) {
        CommonHAKCAnalysis::getWriter() << "Could not find Transfer Function for "
                                        << *HAKCPointer->GetBaseDefinition()->getType() << "\n";
        throw std::exception();
    }

    auto TargetDivision = CompartmentalizationPolicy.GetDivision(Target);

    return CustomTransfer->CreateTransfer(HAKCIRBuilder, TargetDivision, HAKCPointer, Size, IsData);
}

Instruction *
hakc::HAKCTransformer::CreateSignWithColor(hakc::ManagedHAKCPointerP HAKCPointer, Instruction *I, GlobalValue *Target,
                                           bool IsData) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);
    auto AddrSpace = GetPointerAddrSpace(HAKCPointer);

    hakc_compartment_id_t CompartmentID;
    if (auto *GV = dyn_cast<GlobalValue>(HAKCPointer->GetBaseDefinition())) {
        CompartmentID = getSymbolCompartmentID(GV);
    } else {
        CompartmentID = getSymbolCompartmentID(Target);
    }

    auto *CompartmentIDValue = GetHAKCCompartmentValue(CompartmentID);
    auto *IsCodeValue = HAKCIRBuilder.getInt1(!IsData);
    auto *OperandCast = HAKCIRBuilder.CreateBitCast(HAKCPointer->GetBaseDefinition(),
                                                    HAKCIRBuilder.getPtrTy(AddrSpace));
    SmallVector<Value *> Args = {
            OperandCast, CompartmentIDValue, IsCodeValue
    };

    return CreateCallWithResultCast(ModuleAnalysis.HAKCSignWithDivisionName(), HAKCAuthenticationRetType(AddrSpace),
                                    Args, HAKCPointer->GetBaseDefinition());
}

bool hakc::HAKCTransformer::HAKCPointerHasCustomTransfer(hakc::ManagedHAKCPointerP HAKCPointer) {
    return GetCustomTransferFunction(HAKCPointer) != nullptr;
}

std::shared_ptr<hakc::HAKCCustomTransfer>
hakc::HAKCTransformer::GetCustomTransferFunctionForType(hakc::HAKCTypeP HAKCTy) {
    for (auto &it: ModuleAnalysis.GetHAKCCustomTransferFunctions()) {
        if (HAKCTy->GetLLVMType() && HAKCTy->GetLLVMType() == it->GetType()) {
            return it;
        }
    }
    return nullptr;
}

std::shared_ptr<hakc::HAKCCustomTransfer>
hakc::HAKCTransformer::GetCustomTransferFunction(hakc::ManagedHAKCPointerP HAKCPointer) {
    for (auto &it: ModuleAnalysis.GetHAKCCustomTransferFunctions()) {
        if (HAKCPointer->GetBaseDefinition()->getType() == it->GetType()) {
            return it;
        }
    }
    return nullptr;
}

Instruction *
hakc::HAKCTransformer::CreateDefaultTransfer(hakc::ManagedHAKCPointerP HAKCPointer, GlobalValue *Target, bool IsData,
                                             ConstantInt *Size) {
    SmallVector<Value*> TransferOperations;
    CreateTransferArguments(HAKCPointer, Target, IsData, Size, TransferOperations);
    auto AddrSpace = GetPointerAddrSpace(HAKCPointer);
    bool IsPerCPU = CommonHAKCAnalysis::isPerCPUPointer(HAKCPointer->GetBaseDefinition());

    StringRef FunctionToCall;
    if (IsPerCPU) {
        FunctionToCall = ModuleAnalysis.HAKCPerCPUCompartmentTransferName();
    } else {
        FunctionToCall = ModuleAnalysis.HAKCCompartmentTransferName();
    }

    return CreateCallWithResultCast(FunctionToCall, HAKCAuthenticationRetType(AddrSpace), TransferOperations,
                                    HAKCPointer->GetBaseDefinition());
}

Instruction *
hakc::HAKCTransformer::CreateCallWithResultCast(StringRef Name, Type *RetTy, ArrayRef<Value *> Args,
                                                Value *ValueToTypeMatch) {
    auto *Call = CreateCall(Name, RetTy, Args);

    Value *ResultCast;
    if (isa<PtrToIntInst>(ValueToTypeMatch) || ValueToTypeMatch->getType()->isIntegerTy()) {
        ResultCast = HAKCIRBuilder.CreatePtrToInt(Call, ValueToTypeMatch->getType());
    } else {
        ResultCast = HAKCIRBuilder.CreateBitCast(Call, ValueToTypeMatch->getType());
    }

    auto *Result = dyn_cast<Instruction>(ResultCast);
    if (!Result) {
        Result = Call;
    }

    return Result;
}

/* Called with Values of type "void *" ("i8*") */
hakc::HAKCTypeP
hakc::HAKCTransformer::FindEntryBitcast(hakc::ManagedHAKCPointerP HAKCPointer, Instruction *I, Function *Target) {
    /*
     * Checking V to see if it is an argument of the function that contains instruction I.
     * I is contained within a pass-generated HAKC_XFER function.
     *
     * If V is an argument, we determine V's argument index for the function.
     * The argument will have the same index in Target.
     */
    Argument *TargetV = nullptr;
    Type *BitcastType = nullptr;
    User *BitcastUser;
    for (auto &Arg: I->getFunction()->args()) {
        if (HAKCPointer->GetBaseDefinition() == &Arg) {
            TargetV = Target->getArg(Arg.getArgNo());
            break;
        }
    }

    /* not an argument, nothing to return */
    if (!TargetV) {
        return nullptr;
    }

    std::set<Use *> WorkingList;
    for (auto &TargetUse: TargetV->uses()) {
        WorkingList.insert(&TargetUse);
    }

    while (!WorkingList.empty()) {
        auto *CurrentUse = *WorkingList.begin();
        auto *CurrentUser = CurrentUse->getUser();
        WorkingList.erase(CurrentUse);

        if (auto *BitCastOp = dyn_cast<BitCastOperator>(CurrentUser)) {
            BitcastType = BitCastOp->getDestTy();
            BitcastUser = CurrentUser;
            break;
        } else if (auto *BitCastI = dyn_cast<BitCastInst>(CurrentUser)) {
            BitcastType = BitCastI->getDestTy();
            BitcastUser = CurrentUser;
            break;
        } else if (auto *StoreI = dyn_cast<StoreInst>(CurrentUser)) {
            /* This may be unoptimized code that does
             *     %tmp     = alloca i8*
             *                store i8* %arg, i8** %tmp
             *     %voidarg = load i8*, i8** %tmp
             *     %bcarg   = bitcast i8* %voidarg to %struct.type*
             * instead of just directly bitcasting the argument
             *
             * the following code will only handle this case correctly in the event that
             *   there is only the one level of indirection through memory
             */
            if (CurrentUse->getOperandNo() != StoreInst::getPointerOperandIndex()) {
                auto *StorePtrDef = ModuleAnalysis.getDef(StoreI->getPointerOperand(), false, DebugIsActive());
                if (isa<AllocaInst>(StorePtrDef)) {
                    for (auto *AllocaUser: StorePtrDef->users()) {
                        if (isa<LoadInst>(AllocaUser)) {
                            for (auto &LoadUse: AllocaUser->uses()) {
                                if (isa<BitCastInst>(LoadUse.getUser()) || isa<BitCastOperator>(LoadUse.getUser())) {
                                    WorkingList.insert(&LoadUse);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (DebugIsActive()) {
        CommonHAKCAnalysis::getWriter() << "Value " << *HAKCPointer;
        if (BitcastType) {
            CommonHAKCAnalysis::getWriter() << " is cast to " << *BitcastType << " by Instruction " << *BitcastUser;
        } else {
            CommonHAKCAnalysis::getWriter() << " is not bitcast";
        }
        CommonHAKCAnalysis::getWriter() << " in function " << Target->getName() << "\n";
    }

    return TypeIdentifier.FindType(BitcastType);
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
Instruction *
hakc::HAKCTransformer::CreateVoidCastCompartmentTransfer(hakc::ManagedHAKCPointerP HAKCPointer, Instruction *I,
                                                         GlobalValue *Target, HAKCTypeP TypeToUse) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);

    /* just give safe pointer to kernel targets */
    if (TargetIsKernel(Target)) {
        auto *V = CreateSafePointer(HAKCPointer, &*HAKCIRBuilder.GetInsertPoint());
        auto *SafePtr = dyn_cast<Instruction>(V);
        if (!SafePtr) {
            CommonHAKCAnalysis::getWriter() << "Unexpected Safe Pointer Type: " << *V << "\n";
            throw std::exception();
        }
        return SafePtr;
    }


    if (TypeToUse->IsPointerToPointer()) {
        if (DebugIsActive()) {
            CommonHAKCAnalysis::getWriter() << "TypeToUse " << *TypeToUse
                                            << " is a pointer to a pointer.\nAdding transfer starting at "
                                            << *HAKCIRBuilder.GetInsertPoint() << " in function "
                                            << *HAKCIRBuilder.GetInsertBlock()->getParent() << "\n";
        }
        auto *FinalLocation = &*HAKCIRBuilder.GetInsertPoint();
        auto *SafePtr = CreateSafePointer(HAKCPointer, &*HAKCIRBuilder.GetInsertPoint());
        auto *Load = HAKCIRBuilder.CreateLoad(
                PointerType::get(getModule().getContext(), GetPointerAddrSpace(HAKCPointer)), SafePtr);
        auto ManagedPointer = CreateNewManagedPointer(Load);
        CreateVoidCastCompartmentTransfer(ManagedPointer,
                                          Load->getNextNonDebugInstruction(), Target,
                                          TypeToUse->GetPointeeType());
        auto *FinalTransfer = CreateSizedCompartmentTransfer(HAKCPointer, FinalLocation,
                                                             Target, true, HAKCIRBuilder.getInt64(64));
        return FinalTransfer;
    }

    auto *size = GetObjectSizeInBytes(TypeToUse->GetPointeeType());

    if (size->equalsInt(0)) {
        CommonHAKCAnalysis::getWriter() << "Zero size for HAKCType " << *TypeToUse->GetPointeeType() << "\n";
        throw std::exception();
    }

    auto TargetDivision = CompartmentalizationPolicy.GetDivision(Target);

    /*
     * at this point, we know the dest type is a struct* and we know the actual size
     *
     * even if a custom transfer function doesn't exist for the struct type, we can do a
     * more accurate transfer than the previous void* single-byte transfer
     */
    Instruction *Transfer;
    if (DebugIsActive()) {
        CommonHAKCAnalysis::getWriter() << "LLVM type: " << *TypeToUse << "\nsize of type: " << *size << "\n";
    }

    if (auto CustomTransfer = GetCustomTransferFunctionForType(TypeToUse)) {
        /* custom transfer exists, give the most specific transfer possible */
        Transfer = CustomTransfer->CreateTransferWithCasts(HAKCIRBuilder, TargetDivision, HAKCPointer, size,
                                                           HAKCPointer->GetType(), TypeToUse);

        if (DebugIsActive()) {
            CommonHAKCAnalysis::getWriter() << "custom xfer result:\n";
        }
    } else {
        /* no custom transfer exists, give the next-most specific transfer possible, correctly-sized generic transfer */
        Transfer = CreateSizedCompartmentTransfer(HAKCPointer, I, Target, true, size);

        if (DebugIsActive()) {
            CommonHAKCAnalysis::getWriter() << "sized xfer result:\n";
        }
    }
    if (DebugIsActive()) {
        CommonHAKCAnalysis::getWriter() << *Transfer << "\n";
    }
    return Transfer;
}

Instruction *
hakc::HAKCTransformer::CreateCompartmentTransfer(ManagedHAKCPointerP HAKCPointer, Instruction *I, GlobalValue *Target,
                                                 bool IsData) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);

    auto ObjectSize = GetObjectSizeInBytes(HAKCPointer);

    /*
     * If HAKCPointer is of type "void *" ("i8*"), ObjectSize will be 1.
     * I don't think this is the only case where that happens, so we do another check.
     * Only care about data transfer.
     */
    if (ObjectSize == HAKCIRBuilder.getInt64(1) && IsData && isa<Function>(Target)) {
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
        auto EntryCastType = FindEntryBitcast(HAKCPointer, I, dyn_cast<Function>(Target));
        /*
         * If a bitcast is found in entry block, Target takes HAKCPointer as "void*" but
         * immediately uses it as if it some other type.
         * This situation is often found in functions used to run kthreads.
         */
        if (EntryCastType) {
            if (DebugIsActive()) {
                CommonHAKCAnalysis::getWriter() << Target->getName() << " has entry block bitcast from void* to:\n"
                                                << *EntryCastType << "\n";
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
            CommonHAKCAnalysis::getWriter() << "Could not get ObjectSize for " << *HAKCPointer << "\n";
        }

        ObjectSize = GetDefaultObjectSize();
    }

    return CreateSizedCompartmentTransfer(HAKCPointer, I, Target, IsData, ObjectSize);
}

Function *hakc::HAKCTransformer::GetTransferFunction(Function *F) {
    auto TransferFunctionName = ModuleAnalysis.getOutsideTransferName(F);
    auto *TransferFunction = ModuleAnalysis.GetFunctionByName(TransferFunctionName, F->getFunctionType());
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
    return hakc::HAKCModuleAnalysis::IsKernelSymbol(Target, CompartmentalizationPolicy) &&
           !CommonHAKCAnalysis::NoKernelTransferFunctionsSet();
}

void
hakc::HAKCTransformer::CreateForwardArgumentTransfers(Function *Target, Function *TransferFunction,
                                                      SmallVector<Value *> &ArgsList) {
    bool NoKernelXfers = NoKernelTransfers(Target);

    for (auto Arg = TransferFunction->arg_begin(); Arg != TransferFunction->arg_end(); Arg++) {
        if (!CommonHAKCAnalysis::argShouldTransfer(Arg) || NoKernelXfers) {
            ArgsList.push_back(Arg);
            continue;
        }
        auto ManagedPointer = CreateNewManagedPointer(Arg);
        bool IsData = !Arg->getType()->isFunctionTy();
        CreateTransferFunctionArg_PreCall(Target, TransferFunction, Arg);
        Instruction *Transfer = CreateCompartmentTransfer(ManagedPointer, &*HAKCIRBuilder.GetInsertPoint(), Target,
                                                          IsData);
        ArgsList.push_back(Transfer);
    }
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
        auto TransferName = CommonHAKCAnalysis::getVariadicTransferName(Target);
        TransferName += "_";
        TransferName += std::to_string(TargetTransferCount);

        TransferFunction = ModuleAnalysis.GetFunctionByName(TransferName, TransferType);
        PopulateTransferFunction(Target, TransferFunction);
        TransferFunction->setLinkage(GlobalValue::PrivateLinkage);
        VariadicTransferFunctions[TransferFunction] = Target;
    }

    return TransferFunction;
}

void hakc::HAKCTransformer::TransferStructMembers(ConstantStruct *ConstStruct, Function *GlobalTransfer,
                                                  GlobalValue *GlobalVar, bool Debug) {
    if (Debug) {
        CommonHAKCAnalysis::getWriter() << "Transferring " << *ConstStruct << "\n";
    }

    for (auto &Member: ConstStruct->operands()) {
        GlobalValue *Target = GlobalVar;
        if (auto *GlobalMember = dyn_cast<GlobalValue>(Member.get())) {
            Target = GlobalMember;
        }
        if (!TransferShouldBeCreated(Member.get(), Target)) {
            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "No transfer of member " << std::to_string(Member.getOperandNo())
                                                << " to " << Target << "\n";
            }
            continue;
        }

        if (auto *StructMember = dyn_cast<ConstantStruct>(Member.get())) {
            TransferStructMembers(StructMember, GlobalTransfer, GlobalVar, Debug);
            continue;
        }

        if (CommonHAKCAnalysis::IsPointerLikeType(Member->getType())) {
            Value *Transfer, *GEP, *Load;

            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "Creating Transfer of Member "
                                                << std::to_string(Member.getOperandNo()) << " " << Member.get() << "\n";
            }
            GEP = HAKCIRBuilder.CreateStructGEP(GlobalVar->getValueType(), GlobalVar, Member.getOperandNo());
            Load = HAKCIRBuilder.CreateLoad(Member->getType(), GEP);
            auto ManagedPointer = CreateNewManagedPointer(Load);
            Transfer = CreateCompartmentTransfer(ManagedPointer, GlobalTransfer->getEntryBlock().getTerminator(),
                                                 Target, !isa<Function>(Member.get()));
            HAKCIRBuilder.CreateStore(Transfer, GEP);
        }
    }
}

bool hakc::HAKCTransformer::TransferShouldBeCreated(Value *V, GlobalValue *Target) {
    bool CreateTransfer = !TargetIsKernel(Target) && !isa<ConstantPointerNull>(V) &&
                          CommonHAKCAnalysis::IsPointerLikeType(V->getType());
    if (auto *I = dyn_cast<ConstantInt>(V)) {
        CreateTransfer = !I->equalsInt(0) && !I->isMinusOne();
    }

    if (ModuleAnalysis.isIgnoredType(V->getType())) {
        CreateTransfer = false;
    }

    return CreateTransfer;
}

Function *
hakc::HAKCTransformer::PopulateGlobalTransfer(Function *GlobalTransfer, GlobalVariable *GlobalVar, bool Debug) {
    if (!GlobalTransfer->empty()) {
        return GlobalTransfer;
    }

    if (Debug) {
        CommonHAKCAnalysis::getWriter() << "Initializing New Function " << GlobalTransfer->getName() << "\n";
    }
    InitNewFunction(GlobalTransfer, "HAKCGlobalTransferEntry");
    auto *VoidRet = HAKCIRBuilder.CreateRetVoid();
    HAKCIRBuilder.SetInsertPoint(VoidRet);

    if (GlobalVar->hasInitializer()) {
        if (Debug) {
            CommonHAKCAnalysis::getWriter() << "Creating Init Transfer of " << GlobalVar << "\n";
        }
        if (auto *InitStruct = dyn_cast<ConstantStruct>(GlobalVar->getInitializer())) {
            if (Debug) {
                CommonHAKCAnalysis::getWriter() << "Transferring struct members\n";
            }
            TransferStructMembers(InitStruct, GlobalTransfer, GlobalVar, Debug);
        } else if (CommonHAKCAnalysis::IsPointerLikeType(GlobalVar->getInitializer()->getType())) {
            GlobalValue *Target = GlobalVar;
            if (auto *FuncPtr = dyn_cast<Function>(GlobalVar->getInitializer())) {
                Target = FuncPtr;
            }

            if (TransferShouldBeCreated(GlobalVar->getInitializer(), Target)) {
                if (Debug) {
                    CommonHAKCAnalysis::getWriter() << "Creating Transfer of " << Target << "\n";
                }
                auto ManagedPointer = CreateNewManagedPointer(GlobalVar->getInitializer());
                auto *Transfer = CreateCompartmentTransfer(ManagedPointer, VoidRet, Target,
                                                           !isa<Function>(GlobalVar->getInitializer()));
                HAKCIRBuilder.CreateStore(Transfer, GlobalVar);
            }
        }

    }

    if (Debug) {
        CommonHAKCAnalysis::getWriter() << "Finished initializing " << GlobalTransfer->getName() << "\n";
    }
    return GlobalTransfer;
}

void hakc::HAKCTransformer::InitNewFunction(Function *F, StringRef EntryBlockName) {
    if (!F->empty()) {
        return;
    }

    auto *EntryBB = BasicBlock::Create(getModule().getContext(), EntryBlockName, F);
    if (EntryBB != &F->getEntryBlock()) {
        CommonHAKCAnalysis::getWriter() << "Invalid Entry BasicBlock created\n";
        throw std::exception();
    }

    HAKCIRBuilder.SetInsertPoint(EntryBB);
}

Function *hakc::HAKCTransformer::PopulateTransferFunction(Function *Target, Function *TransferFunction) {
    if (!TransferFunction->empty()) {
        return TransferFunction;
    }

    InitNewFunction(TransferFunction, "HAKCTransferEntry");

    // Create a temporary terminator
    auto *Unreachable = HAKCIRBuilder.CreateUnreachable();
    HAKCIRBuilder.SetInsertPoint(Unreachable);

    SmallVector<Value *> TransferredArguments;
    CreateForwardArgumentTransfers(Target, TransferFunction, TransferredArguments);
    CallInst *TargetFunctionCall = HAKCIRBuilder.CreateCall(Target, TransferredArguments);

    if (!Target->doesNotReturn()) {
        CreateBackwardArgumentTransfers(Target, TransferFunction);
        if (!Target->getReturnType()->isVoidTy()) {
            HAKCIRBuilder.CreateRet(TargetFunctionCall);
        } else {
            HAKCIRBuilder.CreateRetVoid();
        }
        Unreachable->eraseFromParent();
    }

    CreateTransferFunctionFinalize_Arch(Target, TransferFunction);

    if (llvm::verifyFunction(*TransferFunction, &CommonHAKCAnalysis::getWriter().GetOS())) {
        CommonHAKCAnalysis::getWriter() << "Function verification for transfer function failed\n" << *TransferFunction;
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
    if (!TransferFunction->empty() || !ModuleAnalysis.TransferFunctionShouldBeCreated(F, CompartmentalizationPolicy)) {
        return TransferFunction;
    }

    return PopulateTransferFunction(F, TransferFunction);
}

Value *hakc::HAKCTransformer::CreateBitCast(hakc::ManagedHAKCPointerP Operand, Type *TargetType, Instruction *I) {
    ValidateHAKCPointerAndLocation(Operand, I);
    auto AddrSpace = GetPointerAddrSpace(Operand);

    if (TargetType->isPointerTy() && TargetType->getPointerAddressSpace() != AddrSpace) {
        CommonHAKCAnalysis::getWriter() << "TargetType " << *TargetType << " has AddrSpace when casting " << *Operand
                                        << "\n" << *I->getFunction() << "\n";
        throw std::exception();
    }
    Value *BitCast;
    if (Operand->GetType()->IsIntegerType() && TargetType->isPointerTy()) {
        BitCast = HAKCIRBuilder.CreateIntToPtr(Operand->GetBaseDefinition(), TargetType);
    } else if (Operand->GetType()->IsPointerType() && TargetType->isIntegerTy()) {
        BitCast = HAKCIRBuilder.CreatePtrToInt(Operand->GetBaseDefinition(), TargetType);
    } else {
        BitCast = HAKCIRBuilder.CreateBitCast(Operand->GetBaseDefinition(), TargetType);
    }
    return BitCast;
}


ConstantInt *hakc::HAKCTransformer::GetObjectSizeInBytes(hakc::ManagedHAKCPointerP HAKCPointer) {
    return GetObjectSizeInBytes(HAKCPointer->GetType()->GetPointeeType());
}

ConstantInt *hakc::HAKCTransformer::GetObjectSizeInBytes(hakc::HAKCTypeP HAKCType) {
    auto bit_size = HAKCType->GetSizeInBits();
    return getInt64(bit_size / BITS_PER_BYTE_);
}

Type *hakc::HAKCTransformer::HAKCAuthenticationRetType(unsigned AddrSpace) {
    return HAKCIRBuilder.getPtrTy(AddrSpace);
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
    auto Compartment = CompartmentalizationPolicy.GetDivision(GV).GetHAKCCompartment();
    return Compartment.GetCompartmentIDValue();
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

bool hakc::HAKCTransformer::TargetIsKernel(GlobalValue *Target) {
    return hakc::HAKCModuleAnalysis::IsKernelSymbol(Target, CompartmentalizationPolicy);
}

ConstantInt *hakc::HAKCTransformer::GetHAKCCompartmentValue(hakc_compartment_id_t CompartmentID) {
    return HAKCIRBuilder.getIntN(COMPARTMENT_ID_BIT_LENGTH, CompartmentID);
}

void hakc::HAKCTransformer::CreateTransferFunctionFinalize_Arch(Function *Original, Function *Transfer) {}

void hakc::HAKCTransformer::CreateTransferFunctionArg_PreCall(Function *F, Function *TransferFunction, Value *Arg) {}

void hakc::HAKCTransformer::CreateTransferFunctionArg_PostCall(Function *F, Function *TransformFunction, Value *Arg) {}

unsigned hakc::HAKCTransformer::GetPointerAddrSpace(hakc::ManagedHAKCPointerP HAKCPointer) {
    return GetPointerAddrSpace(HAKCPointer->GetBaseDefinition());
}

unsigned hakc::HAKCTransformer::GetPointerAddrSpace(Value *V) {
    unsigned AddrSpace = 0;
    if (V->getType()->isPointerTy()) {
        AddrSpace = V->getType()->getPointerAddressSpace();
    }
    return AddrSpace;
}

GlobalVariable *hakc::HAKCTransformer::AddCompartmentMetadataEntry(HAKCCompartment &Compartment) {
    return nullptr;
}

bool hakc::HAKCTransformer::DebugIsActive() {
    return HAKCIRBuilder.GetInsertPoint()->getFunction()->getName() == CommonHAKCAnalysis::getHAKCDebugName();
}

hakc::ManagedHAKCPointerP hakc::HAKCTransformer::CreateNewManagedPointer(Value *BaseDefinition) {
    auto ManagedPtr = std::make_shared<ManagedHAKCPointer>(BaseDefinition, DebugIsActive());
    auto HAKCTy = TypeIdentifier.FindType(BaseDefinition->getType());
    ManagedPtr->SetType(HAKCTy);
    return ManagedPtr;
}
