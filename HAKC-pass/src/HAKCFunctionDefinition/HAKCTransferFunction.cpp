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
                                                                                ColorIdx(-1) {

    }

    HAKCTransferFunction::HAKCTransferFunction(StringRef Name, unsigned int SignedPtrIdx, unsigned int CompartmentIdIdx,
                                               int ColorIdx) : HAKCFunctionDefinition(Name), SignedPtrIdx
            (SignedPtrIdx), CompartmentIdIdx(CompartmentIdIdx), ColorIdx(ColorIdx) {

    }

    int HAKCTransferFunction::GetColorIdx() const {
        return ColorIdx;
    }

    bool HAKCTransferFunction::HasColorIdx() const {
        return ColorIdx >= 0;
    }

    unsigned HAKCTransferFunction::GetSignedPtrIdx() const {
        return SignedPtrIdx;
    }

    unsigned HAKCTransferFunction::GetCompartmentIdIdx() const {
        return CompartmentIdIdx;
    }
} // hakc