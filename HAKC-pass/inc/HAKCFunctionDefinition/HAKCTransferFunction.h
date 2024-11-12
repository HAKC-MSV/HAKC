//
// Created by de29664 on 6/23/23.
//

#ifndef HAKC_HAKCTRANSFERFUNCTION_H
#define HAKC_HAKCTRANSFERFUNCTION_H

#include <llvm/IR/Instructions.h>
#include "HAKCFunctionDefinition.h"

namespace hakc {

    class HAKCTransferFunction : public HAKCFunctionDefinition {
    public:
        HAKCTransferFunction(Function *F, unsigned SignedPtrIdx, unsigned CompartmentIdIdx, unsigned DivisionIdx,
                             unsigned SizeIdx);

        ConstantInt *GetSignedPtrIdx() const;

        ConstantInt *GetCompartmentIdIdx() const;

        ConstantInt *GetDivisionIdIdx() const;

        ConstantInt *GetSizeIdx() const;

    protected:
        ConstantInt *SignedPtrIdx;
        ConstantInt *CompartmentIdIdx;
        ConstantInt *DivisionIdIdx;
        ConstantInt *SizeIdx;
    };

    typedef std::shared_ptr<HAKCTransferFunction> hakc_transfer_def_t;
} // hakc

#endif //HAKC_HAKCTRANSFERFUNCTION_H
