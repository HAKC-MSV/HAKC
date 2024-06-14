//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCFUNCTIONINFO_H
#define HAKC_HAKCFUNCTIONINFO_H

#include "HAKCSymbolInfo.h"
#include "llvm/IR/Function.h"

using namespace llvm;

namespace hakc {

    class HAKCFunctionInfo : public HAKCSymbolInfo {
    public:
        HAKCFunctionInfo(StringRef Name, bool DebugActive);
        void SetFunction(Function *F);
        Function* GetFunction();

    protected:
        std::set<std::shared_ptr<HAKCSymbolInfo>> DirectCalls;
        std::set<std::shared_ptr<HAKCTypeInfo>> IndirectCalls;
    };

} // hakc

#endif //HAKC_HAKCFUNCTIONINFO_H
