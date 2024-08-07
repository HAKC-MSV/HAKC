//
// Created by de29664 on 6/26/23.
//

#include "HAKCFunctionDefinition/SingleFunctionCustomTransfer.h"
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
                                                              HAKCCompartment &TargetCompartment,
                                                              HAKC_Division_ID TargetDivision,
                                                              Value *HAKCPointer, Value *Size, bool IsData) {
        return HAKCIRBuilder.CreateCall(GetFunction(), {
                HAKCPointer, TargetCompartment.GetCompartmentID(), TargetDivision
        });
    }

    Instruction *SingleFunctionCustomTransfer::CreateTransferWithCasts(IRBuilder<> &HAKCIRBuilder,
                                                                       HAKCCompartment &TargetCompartment,
                                                                       HAKC_Division_ID TargetDivision,
                                                                       Value *HAKCPointer, Value *Size,
                                                                       Type *SrcTy, Type *DestTy) {
        /* cast void* HAKCPointer to DestTy */
        Value *BitcastArgForTransferCall = HAKCIRBuilder.CreateBitCast(HAKCPointer, DestTy);
        /* Call transfer function with DestTy HAKCPointer */
        Value *TransferCall = HAKCIRBuilder.CreateCall(GetFunction(), {
                BitcastArgForTransferCall, TargetCompartment.GetCompartmentID(), TargetDivision
        });
        /* cast DestTy HAKCPointer back to void* */
        Value *BitcastArgForTargetCall = HAKCIRBuilder.CreateBitCast(TransferCall, SrcTy);

        auto *Result = dyn_cast<Instruction>(BitcastArgForTargetCall);
        return Result;
    }
} // hakc
