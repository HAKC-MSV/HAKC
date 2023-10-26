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

        HAKCTransferFunction(StringRef Name, unsigned SignedPtrIdx, unsigned CompartmentIdIdx, int ColorIdx);

        unsigned GetSignedPtrIdx() const;

        unsigned GetCompartmentIdIdx() const;

        int GetColorIdx() const;

        bool HasColorIdx() const;

    protected:
        unsigned SignedPtrIdx;
        unsigned CompartmentIdIdx;
        int ColorIdx;
    };

    typedef std::shared_ptr<HAKCTransferFunction> hakc_transfer_def_t;

} // hakc

#endif //HAKC_HAKCTRANSFERFUNCTION_H
