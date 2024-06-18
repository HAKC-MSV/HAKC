//
// Created by de29664 on 6/11/24.
//

#include "HAKCTypeIdentifier/HAKCSymbolInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"

#include <utility>

hakc::HAKCSymbolInfo::HAKCSymbolInfo(StringRef Name, bool DebugActive) : HAKCInfo(Name, DebugActive), Type(nullptr),
                                                                         UsedSymbols(), GlobalObj(nullptr) {

}

void hakc::HAKCSymbolInfo::SetType(std::shared_ptr<HAKCTypeInfo> HAKCType) {
    Type = std::move(HAKCType);
}

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCSymbolInfo::GetType() {
    return Type;
}

void hakc::HAKCSymbolInfo::AddSymbolUse(const std::shared_ptr<HAKCSymbolInfo> &Symbol) {
    UsedSymbols.insert(Symbol);
}

std::string hakc::HAKCSymbolInfo::GetYamlHeader(unsigned Indents) {
    std::string Yaml;
    llvm::raw_string_ostream sstream(Yaml);

    sstream.indent(Indents) << "-\n";
    sstream.indent(Indents) << "Name: " << GetName() << "\n";
    sstream.indent(Indents) << "Type: ";
    if(!this->Type) {
        sstream << "@UNKNOWN@";
    } else {
        sstream << this->Type->GetName();
    }
    sstream << "\n";
    sstream.indent(Indents) << "LLVM-Type: " << *GetGlobalObj()->getType() << "\n";
    return Yaml;
}

std::string hakc::HAKCSymbolInfo::GetYaml(unsigned Indents) {
    auto Yaml = GetYamlHeader(Indents);
    llvm::raw_string_ostream sstream(Yaml);

    if(!UsedSymbols.empty()) {
        sstream.indent(Indents) << "Used-Symbols:\n";
        unsigned Count = 0;
        for(auto &Symbol : UsedSymbols) {
            sstream << Symbol->GetYamlHeader(Indents + HAKCInfo::IndentSpaces());
            if(++Count != UsedSymbols.size()) {
                sstream << "\n";
            }
        }
    }

    return Yaml;
}

void hakc::HAKCSymbolInfo::SetGlobalObj(GlobalObject *GlobalObj) {
    if(!GlobalObj) {
        CommonHAKCAnalysis::getWriter() << "Trying to set null GlobalVariable\n";
        throw std::exception();
    }
    this->GlobalObj = GlobalObj;
}

GlobalObject* hakc::HAKCSymbolInfo::GetGlobalObj() {
    return GlobalObj;
}
