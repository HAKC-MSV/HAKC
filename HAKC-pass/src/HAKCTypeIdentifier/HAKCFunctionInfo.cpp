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

    void HAKCFunctionInfo::AddIndirectCall(const std::shared_ptr<HAKCIndirectCallSource> &Source) {
        if (!Source) {
            CommonHAKCAnalysis::getWriter() << "Trying to add null indirect call source\n";
            throw std::exception();
        }
        IndirectCalls.insert(Source);
    }

    void HAKCFunctionInfo::AddDirectCall(const std::shared_ptr<HAKCFunctionInfo> &DirectCall) {
        if (!DirectCall) {
            CommonHAKCAnalysis::getWriter() << "Trying to add null Direct Call\n";
            throw std::exception();
        }
        DirectCalls.insert(DirectCall);
    }

    std::string HAKCFunctionInfo::GetYaml(unsigned Indents) {
        auto Yaml = HAKCSymbolInfo::GetYaml(Indents);
        llvm::raw_string_ostream sstream(Yaml);

        unsigned Count;
        if (!DirectCalls.empty()) {
            sstream << "\n";
            sstream.indent(Indents + 2) << "Direct-Calls:\n";
            Count = 0;
            for (auto &Symbol: DirectCalls) {
                sstream << Symbol->GetYamlHeader(Indents + HAKCInfo::IndentSpaces());
                if (++Count != DirectCalls.size()) {
                    sstream << "\n";
                }
            }
        }
        if (!IndirectCalls.empty()) {
            Count = 0;
            sstream << "\n";
            sstream.indent(Indents + 2) << "Indirect-Calls:\n";
            for (auto &IndirectSource: IndirectCalls) {
                sstream << IndirectSource->GetYaml(Indents + HAKCInfo::IndentSpaces());
                if (++Count != IndirectCalls.size()) {
                    sstream << "\n";
                }
            }
        }
        return Yaml;
    }
} // hakc
