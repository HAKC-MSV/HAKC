//
// Created by de29664 on 5/2/23.
//

#include "HAKCTypeIdentifier/HAKCGlobalInfo.h"
#include "HAKCTypeIdentifier/HAKCTypeInfo.h"

namespace hakc {
    HAKCGlobalInfo::HAKCGlobalInfo(StringRef Name, bool DebugActive) : HAKCSymbolInfo(Name, DebugActive) {

    }

    void HAKCGlobalInfo::SetGlobalVariable(GlobalVariable *GV) {
        HAKCSymbolInfo::SetGlobalObj(GV);
    }

    GlobalVariable *HAKCGlobalInfo::GetGlobalVariable() {
        return dyn_cast<GlobalVariable>(GetGlobalObj());
    }

    StringRef HAKCGlobalInfo::GetYamlIdentifier() const {
        return "!HAKCGlobalVariable";
    }
} // hakc
