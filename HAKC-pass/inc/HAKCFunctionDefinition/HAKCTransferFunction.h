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
        HAKCTransferFunction(StringRef Name, unsigned SignedPtrIdx, unsigned CompartmentIdIdx);

        HAKCTransferFunction(StringRef Name, unsigned SignedPtrIdx, unsigned CompartmentIdIdx, int DivisionIdx);

        unsigned GetSignedPtrIdx() const;

        unsigned GetCompartmentIdIdx() const;

        int GetDivisionIdx() const;

        bool HasDivisionIdx() const;

    protected:
        unsigned SignedPtrIdx;
        unsigned CompartmentIdIdx;
        int DivisionIdIdx;
    };

    typedef std::shared_ptr<HAKCTransferFunction> hakc_transfer_def_t;

} // hakc

#endif //HAKC_HAKCTRANSFERFUNCTION_H
