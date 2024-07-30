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
                                                              std::shared_ptr<HAKCSymbolInfo> TargetCompartment,
                                                              Value *HAKCPointer, Value *Size, bool IsData) {
        hakc_compartment_id_t CompartmentID;
        sym_color_t ColorID;
        if (TargetCompartment) {
            CompartmentID = TargetCompartment->getCompartmentID();
            ColorID = TargetCompartment->getCompartment()->getColor();
        } else {
            CompartmentID = KERNEL_COMPARTMENT;
            ColorID = KERNEL_COLOR;
        }
        auto *Compartment = HAKCIRBuilder.getIntN(COMPARTMENT_ID_BIT_LENGTH,
                                                  CompartmentID);
        auto *Color = HAKCIRBuilder.getIntN(CLIQUE_COLOR_BIT_LENGTH, ColorID);
        return HAKCIRBuilder.CreateCall(GetFunction(), {
                HAKCPointer, Compartment, Color
        });
    }

    Instruction *SingleFunctionCustomTransfer::CreateTransferWithCasts(IRBuilder<> &HAKCIRBuilder,
                                                                       std::shared_ptr<HAKCSymbolInfo> TargetCompartment,
                                                                       Value *HAKCPointer, Value *Size,
                                                                       Type *SrcTy, Type *DestTy) {
        hakc_compartment_id_t CompartmentID;
        sym_color_t ColorID;
        if (TargetCompartment) {
            CompartmentID = TargetCompartment->getCompartmentID();
            ColorID = TargetCompartment->getCompartment()->getColor();
        } else {
            CompartmentID = KERNEL_COMPARTMENT;
            ColorID = KERNEL_COLOR;
        }
        auto *Compartment = HAKCIRBuilder.getIntN(COMPARTMENT_ID_BIT_LENGTH,
                                                  CompartmentID);
        auto *Color = HAKCIRBuilder.getIntN(CLIQUE_COLOR_BIT_LENGTH, ColorID);

        /* cast void* HAKCPointer to DestTy */
        Value *BitcastArgForTransferCall = HAKCIRBuilder.CreateBitCast(HAKCPointer, DestTy);
        /* Call transfer function with DestTy HAKCPointer */
        Value *TransferCall = HAKCIRBuilder.CreateCall(GetFunction(), {
                BitcastArgForTransferCall, Compartment, Color
        });
        /* cast DestTy HAKCPointer back to void* */
        Value *BitcastArgForTargetCall = HAKCIRBuilder.CreateBitCast(TransferCall, SrcTy);

        auto *Result = dyn_cast<Instruction>(BitcastArgForTargetCall);
        return Result;
    }
} // hakc
