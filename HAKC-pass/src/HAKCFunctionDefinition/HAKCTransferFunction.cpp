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
        CreateIndexes(SignedPtrIdx, CompartmentIdIdx, DivisionIdx, SizeIdx);
    }

    HAKCTransferFunction::HAKCTransferFunction(Function *F, unsigned int SignedPtrIdx, unsigned int CompartmentIdIdx,
                                               unsigned int DivisionIdx) : HAKCFunctionDefinition(F),
                                                                           SignedPtrIdx(nullptr),
                                                                           CompartmentIdIdx(nullptr),
                                                                           DivisionIdIdx(nullptr), SizeIdx(nullptr) {
        CreateIndexes(SignedPtrIdx, CompartmentIdIdx, DivisionIdx, MissingIdx);
    }

    void HAKCTransferFunction::CreateIndexes(unsigned int PtrIdx, unsigned int CompartmentIdx,
                                             unsigned int DivisionIdx, unsigned int SzIdx) {
        unsigned IndexBitSize = 64;
        if (PtrIdx != MissingIdx) {
            this->SignedPtrIdx = ConstantInt::get(IntegerType::get(F->getContext(), IndexBitSize), PtrIdx);
        }
        if (CompartmentIdx != MissingIdx) {
            this->CompartmentIdIdx = ConstantInt::get(IntegerType::get(F->getContext(), IndexBitSize), CompartmentIdx);
        }
        if (DivisionIdx != MissingIdx) {
            this->DivisionIdIdx = ConstantInt::get(IntegerType::get(F->getContext(), IndexBitSize), DivisionIdx);
        }
        if (SzIdx != MissingIdx) {
            this->SizeIdx = ConstantInt::get(IntegerType::get(F->getContext(), IndexBitSize), SzIdx);
        }
    }

    ConstantInt *HAKCTransferFunction::GetSignedPtrIdx() const {
        return SignedPtrIdx;
    }

    ConstantInt *HAKCTransferFunction::GetCompartmentIdIdx() const {
        return CompartmentIdIdx;
    }

    ConstantInt *HAKCTransferFunction::GetDivisionIdIdx() const {
        return DivisionIdIdx;
    }

    ConstantInt *HAKCTransferFunction::GetSizeIdx() const {
        return SizeIdx;
    }
} // hakc
