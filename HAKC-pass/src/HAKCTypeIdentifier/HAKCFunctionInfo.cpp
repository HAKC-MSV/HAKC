//
// Created by de29664 on 5/2/23.
//

#include "HAKCTypeIdentifier/HAKCFunctionInfo.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"

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

    void HAKCFunctionInfo::AddIndirectCall(const std::shared_ptr<HAKCTypeInfo> &HAKCType) {
        if(!HAKCType) {
            CommonHAKCAnalysis::getWriter() << "Trying to add null indirect call\n";
            throw std::exception();
        }
        IndirectCalls.insert(HAKCType);
    }

    void HAKCFunctionInfo::AddDirectCall(const std::shared_ptr<HAKCFunctionInfo> &DirectCall) {
        if (!DirectCall) {
            CommonHAKCAnalysis::getWriter() << "Trying to add null Direct Call\n";
            throw std::exception();
        }
        DirectCalls.insert(DirectCall);
    }

    std::string HAKCFunctionInfo::GetYaml() {
        auto Yaml = HAKCSymbolInfo::GetYaml();
        llvm::raw_string_ostream sstream(Yaml);

        sstream.indent(HAKCInfo::IndentSpaces()) << "Direct-Calls:\n";
        unsigned Count = 0;
        for (auto &Symbol: DirectCalls) {
            sstream << Symbol->GetYamlHeader(3 * HAKCInfo::IndentSpaces());
            if(++Count != DirectCalls.size()) {
                sstream << "\n";
            }
        }
        Count = 0;
        sstream.indent(HAKCInfo::IndentSpaces()) << "Indirect-Calls:\n";
        for(auto &Ty : IndirectCalls) {
            sstream.indent(2*HAKCInfo::IndentSpaces()) << "-\n";
            sstream.indent(3*HAKCInfo::IndentSpaces()) << "Type: " << Ty->GetName() << "\n";
        }
        return Yaml;
    }
} // hakc
