//
// Created by de29664 on 6/21/23.
//

#ifndef HAKC_HAKCCUSTOMTRANSFER_H
#define HAKC_HAKCCUSTOMTRANSFER_H

#include <llvm/IR/IRBuilder.h>
#include "llvm/IR/Module.h"
#include "HAKCTransferFunction.h"
#include "HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"
#include "HAKCAnalysis/ManagedHAKCPointer.h"

using namespace llvm;

namespace hakc {
    class HAKCCustomTransfer : public HAKCTransferFunction {
    public:
        HAKCCustomTransfer(Module &M, StringRef TypeName, StringRef TransferFunctionName, Type *ReturnTy,
                           ArrayRef<Type *> ArgTys, unsigned SignedPtrIdx, unsigned CompartmentIdIdx, int DivisionIdx);

        HAKCCustomTransfer(Module &M, StringRef TypeName, StringRef TransferFunctionName, Type *ReturnTy,
                           ArrayRef<Type *> ArgTys, unsigned SignedPtrIdx, unsigned CompartmentIdIdx);

        virtual ~HAKCCustomTransfer() = default;

        Type *GetType() const;

        Function *GetFunction() const;

        virtual Instruction *CreateTransfer(IRBuilder<> &HAKCIRBuilder, HAKCCompartmentDivision &CompartmentDivision,
                                            hakc::ManagedHAKCPointerP HAKCPointer, Value *Size,
                                            bool IsData) = 0;

        virtual Instruction *
        CreateTransferWithCasts(IRBuilder<> &HAKCIRBuilder, HAKCCompartmentDivision &CompartmentDivision,
                                Value *HAKCPointer, Value *Size, Type *srcTy,
                                Type *dstTy) = 0;

    protected:
        Type *TargetType;
        Function *CustomTransfer;

        void FindTargetTypeAndTransfer(Module &M, StringRef TransferFunctionName, StringRef TypeName,
                                       Type *ReturnTy, ArrayRef<Type *> ArgTys);
    };

    typedef std::shared_ptr<HAKCCustomTransfer> hakc_custom_transfer_def_t;
}


#endif //HAKC_HAKCCUSTOMTRANSFER_H
