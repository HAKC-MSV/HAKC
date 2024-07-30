//
// Created by de29664 on 6/26/23.
//

#ifndef HAKC_SINGLEFUNCTIONCUSTOMTRANSFER_H
#define HAKC_SINGLEFUNCTIONCUSTOMTRANSFER_H

#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"

using namespace llvm;

namespace hakc {

    class SingleFunctionCustomTransfer : public HAKCCustomTransfer {
    public:
        SingleFunctionCustomTransfer(Module &M, unsigned CompartmentStorageSizeInBits, StringRef TypeName, StringRef
        TransferFunctionName, Type *ReturnTy, ArrayRef<Type *> ArgTys, unsigned
                                     SignedPtrIdx, unsigned CompartmentIDIdx, int ColorIdx);

        SingleFunctionCustomTransfer(Module &M, unsigned CompartmentStorageSizeInBits, StringRef TypeName, StringRef
        TransferFunctionName, Type *ReturnTy, ArrayRef<Type *> ArgTys, unsigned
                                     SignedPtrIdx, unsigned CompartmentIDIdx);

        Instruction *CreateTransfer(IRBuilder<> &HAKCIRBuilder, std::shared_ptr<HAKCSymbolInfo> TargetCompartment,
                                    Value *HAKCPointer, Value *Size, bool IsData) override;

        Instruction *CreateTransferWithCasts(IRBuilder<> &HAKCIRBuilder,
                                             std::shared_ptr<HAKCSymbolInfo> TargetCompartment,
                                             Value *HAKCPointer, Value *Size, Type *SrcTy, Type *DestTy) override;


    protected:
        unsigned CompartmentStoreageSizeInBits;

    };

} // hakc

#endif //HAKC_SINGLEFUNCTIONCUSTOMTRANSFER_H
