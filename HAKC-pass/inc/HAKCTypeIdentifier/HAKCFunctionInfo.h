//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCFUNCTIONINFO_H
#define HAKC_HAKCFUNCTIONINFO_H

#include "llvm/IR/Instructions.h"
#include "HAKCGlobalInfo.h"

using namespace llvm;

namespace hakc {

    class HAKCTypeIdentifier;

    class HAKCFunctionInfo : public HAKCGlobalInfo {
    public:
        HAKCFunctionInfo(Function *F, HAKCTypeIdentifier &identifier);

        void addEscapingMemberOffset(CallInst *call);

        void addCall(CallInst *call);

        std::string getYaml();

    protected:
        std::set<std::pair<std::string, std::string>> indirectCalls;
        std::set<std::string> directCalls;
    };

} // hakc

#endif //HAKC_HAKCFUNCTIONINFO_H
