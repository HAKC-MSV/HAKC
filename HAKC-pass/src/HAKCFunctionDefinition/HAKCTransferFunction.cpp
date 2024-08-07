//
// Created by de29664 on 6/23/23.
//

#include "HAKCFunctionDefinition/HAKCTransferFunction.h"

using namespace llvm;

namespace hakc {
    HAKCTransferFunction::HAKCTransferFunction(StringRef Name, unsigned int SignedPtrIdx,
                                               unsigned int CompartmentIdIdx) : HAKCFunctionDefinition(Name),
                                                                                SignedPtrIdx(SignedPtrIdx),
                                                                                CompartmentIdIdx(CompartmentIdIdx),
                                                                                DivisionIdIdx(-1) {

    }

    HAKCTransferFunction::HAKCTransferFunction(StringRef Name, unsigned int SignedPtrIdx, unsigned int CompartmentIdIdx,
                                               int DivisionIdx) : HAKCFunctionDefinition(Name), SignedPtrIdx
            (SignedPtrIdx), CompartmentIdIdx(CompartmentIdIdx), DivisionIdIdx(DivisionIdx) {

    }

    int HAKCTransferFunction::GetDivisionIdx() const {
        return DivisionIdIdx;
    }

    bool HAKCTransferFunction::HasDivisionIdx() const {
        return DivisionIdIdx >= 0;
    }

    unsigned HAKCTransferFunction::GetSignedPtrIdx() const {
        return SignedPtrIdx;
    }

    unsigned HAKCTransferFunction::GetCompartmentIdIdx() const {
        return CompartmentIdIdx;
    }
} // hakc
