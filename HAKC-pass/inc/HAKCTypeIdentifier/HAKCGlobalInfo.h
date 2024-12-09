//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCGLOBALINFO_H
#define HAKC_HAKCGLOBALINFO_H

#include "llvm/IR/GlobalVariable.h"
// #include "HAKC-defs.h"
#include "HAKCSymbolInfo.h"
// #include "HAKCAnalysis/CommonHAKCAnalysis.h"

namespace hakc {

    class HAKCGlobalInfo : public HAKCSymbolInfo {
    public:
        HAKCGlobalInfo(StringRef Name, bool DebugActive);
        // HAKCGlobalInfo(CommonHAKCAnalysis &Analysis, StringRef Name, bool DebugActive);

        void SetGlobalVariable(GlobalVariable *GV);

        GlobalVariable *GetGlobalVariable();

        StringRef GetYamlIdentifier() const override;
    };

} // hakc

#endif //HAKC_HAKCGLOBALINFO_H
