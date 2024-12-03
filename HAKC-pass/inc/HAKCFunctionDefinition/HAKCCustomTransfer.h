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
        HAKCCustomTransfer(Function *CustomFunction, HAKCTypeP TargetType, unsigned SignedPtrIdx,
                           unsigned CompartmentIdIdx, int DivisionIdx);

        HAKCCustomTransfer(Function *CustomFunction, HAKCTypeP TargetType, unsigned SignedPtrIdx,
                           unsigned CompartmentIdIdx, unsigned DivisionIdx, unsigned SizeIdx);

        ~HAKCCustomTransfer() = default;

        HAKCTypeP GetTargetType() const;

       virtual Instruction *CreateTransfer(IRBuilder<> &HAKCIRBuilder, HAKCCompartmentDivision &CompartmentDivision,
                                           hakc::HAKCPointerBase &HAKCPointer, Value *Size, bool IsData) = 0;

       virtual Instruction *
       CreateTransferWithCasts(IRBuilder<> &HAKCIRBuilder, HAKCCompartmentDivision &CompartmentDivision,
                               hakc::HAKCPointerBase &HAKCPointer, Value *Size, HAKCTypeP srcTy,
                               HAKCTypeP dstTy) = 0;

    protected:
        HAKCTypeP TargetType;
    };

    typedef std::shared_ptr<HAKCCustomTransfer> hakc_custom_transfer_def_t;
}


#endif //HAKC_HAKCCUSTOMTRANSFER_H
