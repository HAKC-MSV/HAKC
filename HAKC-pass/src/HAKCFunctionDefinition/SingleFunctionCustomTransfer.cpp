//
// Created by de29664 on 6/26/23.
//

#include "HAKCFunctionDefinition/SingleFunctionCustomTransfer.h"
#include "HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include "HAKC-defs.h"
#include <iostream>

namespace hakc {
    SingleFunctionCustomTransfer::SingleFunctionCustomTransfer(Module &M, unsigned int CompartmentStorageSizeInBits,
                                                               StringRef TypeName, StringRef TransferFunctionName,
                                                               Type *ReturnTy, ArrayRef<Type *> ArgTys, unsigned
                                                               SignedPtrIdx, unsigned CompartmentIDIdx, int ColorIdx) :
            HAKCCustomTransfer(M, TypeName, TransferFunctionName,
                               ReturnTy, ArgTys, SignedPtrIdx,
                               CompartmentIDIdx, ColorIdx),
            CompartmentStoreageSizeInBits
                    (CompartmentStorageSizeInBits) {

    }

    SingleFunctionCustomTransfer::SingleFunctionCustomTransfer(Module &M, unsigned int CompartmentStorageSizeInBits,
                                                               StringRef TypeName, StringRef TransferFunctionName,
                                                               Type *ReturnTy, ArrayRef<Type *> ArgTys, unsigned
                                                               SignedPtrIdx, unsigned CompartmentIDIdx) :
            HAKCCustomTransfer(M, TypeName, TransferFunctionName,
                               ReturnTy, ArgTys, SignedPtrIdx,
                               CompartmentIDIdx),
            CompartmentStoreageSizeInBits
                    (CompartmentStorageSizeInBits) {

    }

    Instruction *SingleFunctionCustomTransfer::CreateTransfer(IRBuilder<> &HAKCIRBuilder,
                                                              HAKCCompartmentDivision &CompartmentDivision,
                                                              hakc::ManagedHAKCPointerP HAKCPointer, Value *Size,
                                                              bool IsData) {
        return HAKCIRBuilder.CreateCall(GetFunction(), {
                HAKCPointer->GetBaseDefinition(), CompartmentDivision.GetHAKCCompartment().GetCompartmentID(),
                CompartmentDivision.GetDivisionID()
        });
    }

    Instruction *SingleFunctionCustomTransfer::CreateTransferWithCasts(IRBuilder<> &HAKCIRBuilder, HAKCCompartmentDivision &CompartmentDivision,
                                                                       hakc::ManagedHAKCPointerP HAKCPointer, Value *Size, HAKCTypeP srcTy,
                                                                       HAKCTypeP dstTy) {
        /* cast void* HAKCPointer to DestTy */
        Value *BitcastArgForTransferCall = HAKCIRBuilder.CreateBitCast(HAKCPointer->GetBaseDefinition(), dstTy->GetLLVMType());
        /* Call transfer function with DestTy HAKCPointer */
        Value *TransferCall = HAKCIRBuilder.CreateCall(GetFunction(), {
                BitcastArgForTransferCall, CompartmentDivision.GetHAKCCompartment().GetCompartmentID(),
                CompartmentDivision.GetDivisionID()
        });
        /* cast DestTy HAKCPointer back to void* */
        Value *BitcastArgForTargetCall = HAKCIRBuilder.CreateBitCast(TransferCall, srcTy->GetLLVMType());

        auto *Result = dyn_cast<Instruction>(BitcastArgForTargetCall);
        return Result;
    }
} // hakc
