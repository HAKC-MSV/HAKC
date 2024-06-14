//
// Created by de29664 on 5/2/23.
//

#include "HAKCTypeIdentifier/HAKCFunctionInfo.h"

namespace hakc {
    HAKCFunctionInfo::HAKCFunctionInfo(StringRef Name, bool DebugActive) : HAKCSymbolInfo(Name, DebugActive),
                                                                           DirectCalls(), IndirectCalls() {

    }

    void HAKCFunctionInfo::SetFunction(Function *F) {
        SetGlobalObj(F);
    }

    Function *HAKCFunctionInfo::GetFunction() {
        return dyn_cast<Function>(GetGlobalObj());
    }
} // hakc
