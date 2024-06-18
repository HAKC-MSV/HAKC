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
        void AddDirectCall(const std::shared_ptr<HAKCFunctionInfo>& DirectCall);
        void AddIndirectCall(const std::shared_ptr<HAKCTypeInfo>& HAKCType);

        std::string GetYaml() override;

    protected:
        std::set<std::shared_ptr<HAKCFunctionInfo>> DirectCalls;
        std::set<std::shared_ptr<HAKCTypeInfo>> IndirectCalls;
    };

} // hakc

#endif //HAKC_HAKCFUNCTIONINFO_H
