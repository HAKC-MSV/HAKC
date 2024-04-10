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

hakc::HAKCTransformer::HAKCTransformer(Module &Module, HAKCModuleAnalysis *HAKCAnalysis) :
        HAKCIRBuilder(Module.getContext()),
        DebugInfoProcessor(Module),
        SystemInformation(Module),
        HAKCAnalysis(HAKCAnalysis),
        VariadicTransferFunctions() {

}

Module &hakc::HAKCTransformer::getModule() {
    return HAKCAnalysis->GetModule();
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

        auto Targets = Compartment->getTargets();
        if (Targets.empty()) {
            CommonHAKCAnalysis::getWriter() << "No valid transitions exist for " << F->getName() << " in Compartment "
                                            << std::to_string(CompartmentID) << "\n";
            throw std::exception();
        }

        SmallVector<Constant *> EntryTokenValues;
        SmallVector<hakc_compartment_id_t> IDs;
        IDs.push_back(CompartmentID);
        for (auto &t: Targets) {
            IDs.push_back(t->getID());
        }
        llvm::sort(IDs.begin(), IDs.end(),
                   [](hakc_compartment_id_t LHS, hakc_compartment_id_t RHS) { return LHS < RHS; });

        for (auto ID: IDs) {
            Constant *EntryToken = GetEntryToken(ID);
            EntryTokenValues.push_back(EntryToken);
        }

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

        auto *Initializer = ConstantArray::get(ArrayType::get(EntryTokenTy, EntryTokenValues.size()), EntryTokenValues);

        EntryTokenArray = dyn_cast<GlobalVariable>(getModule().getOrInsertGlobal(name, Initializer->getType()));
        EntryTokenArray->setConstant(true);
        EntryTokenArray->setLinkage(GlobalValue::InternalLinkage);
        if (DebugIsActive()) {
            CommonHAKCAnalysis::getWriter() << "Setting initializer for " << EntryTokenArray->getName() << " to be ";
            Initializer->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " from token values ";
            for (auto *TokenValue: EntryTokenValues) {
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
    Argument *TargetV = nullptr;
    Type *BitcastType = nullptr;
    User *BitcastUser;
    for (auto &Arg: I->getFunction()->args()) {
        if (V == &Arg) {
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
                auto *StorePtrDef = HAKCAnalysis->getDef(StoreI->getPointerOperand(), false, DebugIsActive());
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
        CommonHAKCAnalysis::getWriter() << "Value ";
        V->print(CommonHAKCAnalysis::getWriter());
        if (BitcastType) {
            CommonHAKCAnalysis::getWriter() << " is cast to ";
            BitcastType->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " by Instruction ";
            BitcastUser->print(CommonHAKCAnalysis::getWriter());
        } else {
            CommonHAKCAnalysis::getWriter() << " is not bitcast";
        }
        CommonHAKCAnalysis::getWriter() << " in function ";
        Target->print(CommonHAKCAnalysis::getWriter());
        CommonHAKCAnalysis::getWriter() << "\n";
    }
    return BitcastType;
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
hakc::HAKCTransformer::CreateVoidCastCompartmentTransfer(Value *HAKCPointer, Instruction *I, Function *Target,
                                                         Type *TypeToUse) {
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

    if (TypeToUse->isPointerTy() && TypeToUse->getPointerElementType()->isPointerTy()) {
        if (DebugIsActive()) {
            CommonHAKCAnalysis::getWriter() << "TypeToUse ";
            TypeToUse->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " is a pointer to a pointer.\n";
            CommonHAKCAnalysis::getWriter() << "Adding transfer starting at ";
            HAKCIRBuilder.GetInsertPoint()->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << " in function ";
            HAKCIRBuilder.GetInsertBlock()->getParent()->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }
        auto *FinalLocation = &*HAKCIRBuilder.GetInsertPoint();
        auto *SafePtr = CreateSafePointer(HAKCPointer, &*HAKCIRBuilder.GetInsertPoint());
        auto *Load = HAKCIRBuilder.CreateLoad(TypeToUse->getPointerElementType(), SafePtr);
        CreateVoidCastCompartmentTransfer(Load,
                                          Load->getNextNonDebugInstruction(), Target,
                                          TypeToUse->getPointerElementType());
        auto *FinalTransfer = CreateSizedCompartmentTransfer(HAKCPointer, FinalLocation,
                                                             Target, true, HAKCIRBuilder.getInt64(64));
        return FinalTransfer;
    }

    uint64_t size = 64;

    /* the type has to be a struct */
    auto *StructTy = dyn_cast<StructType>(TypeToUse->getPointerElementType());
    if (StructTy) {
        auto *StructDITy = DebugInfoProcessor.findDiType(StructTy);
        if (StructDITy) {
            size = DebugInfoProcessor.getDITypeSizeInBits(StructDITy);
        } else {
            size = StructTy->getScalarSizeInBits();
        }
    }

    if (size == 0) {
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
    Instruction *Transfer;
    if (DebugIsActive()) {
        CommonHAKCAnalysis::getWriter() << "LLVM type: " << *TypeToUse << "\n";
        CommonHAKCAnalysis::getWriter() << "size of type: " << size << "\n";
    }

    if (auto CustomTransfer = GetCustomTransferFunctionForType(TypeToUse)) {
        /* custom transfer exists, give the most specific transfer possible */
        Transfer = CustomTransfer->CreateTransferWithCasts(HAKCIRBuilder, TargetCompartment, HAKCPointer,
                                                           HAKCIRBuilder.getInt64(size / BITS_PER_BYTE),
                                                           HAKCPointer->getType(), TypeToUse);

        if (DebugIsActive()) {
            CommonHAKCAnalysis::getWriter() << "custom xfer result:\n";
        }
    } else {
        /* no custom transfer exists, give the next-most specific transfer possible, correctly-sized generic transfer */
        Transfer = CreateSizedCompartmentTransfer(HAKCPointer, I, Target, true,
                                                  HAKCIRBuilder.getInt64(size / BITS_PER_BYTE));

        if (DebugIsActive()) {
            CommonHAKCAnalysis::getWriter() << "sized xfer result:\n";
        }
    }
    if (DebugIsActive()) {
        CommonHAKCAnalysis::getWriter() << *Transfer << "\n";
    }
    return Transfer;
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
    if (ObjectSize == HAKCIRBuilder.getInt64(1) && IsData) {
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
        auto TransferName = CommonHAKCAnalysis::getVariadicTransferName(Target);
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

void hakc::HAKCTransformer::CreateTransferFunctionFinalize_Arch(Function *Original, Function *Transfer) { }

void hakc::HAKCTransformer::CreateTransferFunctionArg_PreCall(Function *F, Function *TransferFunction, Value *Arg) { }

void hakc::HAKCTransformer::CreateTransferFunctionArg_PostCall(Function *F, Function *TransformFunction, Value *Arg) { }

bool hakc::HAKCTransformer::FunctionIsExported(Function *F) {
    return false;
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
