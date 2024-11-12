//
// Created by de29664 on 6/23/23.
//

#include "HAKCFunctionDefinition/HAKCTransferFunction.h"
#include "llvm/IR/Constants.h"

using namespace llvm;

namespace hakc {
    HAKCTransferFunction::HAKCTransferFunction(Function *F, unsigned int SignedPtrIdx, unsigned int CompartmentIdIdx,
                                               unsigned int DivisionIdx, unsigned int SizeIdx) :
            HAKCFunctionDefinition(F), SignedPtrIdx(nullptr), CompartmentIdIdx(nullptr), DivisionIdIdx(nullptr),
            SizeIdx(nullptr) {
        if(SignedPtrIdx != (unsigned)-1) {
            this->SignedPtrIdx = ConstantInt::get(IntegerType::get(F->getContext(), 64), SignedPtrIdx);
        }
        if(CompartmentIdIdx != (unsigned)-1) {
            this->CompartmentIdIdx = ConstantInt::get(IntegerType::get(F->getContext(), 64), CompartmentIdIdx);
        }
        if(DivisionIdx != (unsigned)-1) {
            this->DivisionIdIdx = ConstantInt::get(IntegerType::get(F->getContext(), 64), DivisionIdx);
        }
        if(SizeIdx != (unsigned)-1) {
            this->SizeIdx = ConstantInt::get(IntegerType::get(F->getContext(), 64), SizeIdx);
        }
    }

    ConstantInt *HAKCTransferFunction::GetSignedPtrIdx() const {
        return nullptr;
    }

    ConstantInt *HAKCTransferFunction::GetCompartmentIdIdx() const {
        return nullptr;
    }

    ConstantInt *HAKCTransferFunction::GetDivisionIdIdx() const {
        return nullptr;
    }

    ConstantInt *HAKCTransferFunction::GetSizeIdx() const {
        return nullptr;
    }
} // hakc
