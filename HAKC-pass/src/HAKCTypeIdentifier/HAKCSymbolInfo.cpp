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

std::string hakc::HAKCSymbolInfo::GetYaml() {
    std::string Yaml;
    llvm::raw_string_ostream sstream(Yaml);

    sstream << "-\n";
    sstream.indent(HAKCInfo::IndentSpaces()) << "Name: " << GetName() << "\n";
    sstream.indent(HAKCInfo::IndentSpaces()) << "Type: " << this->Type->GetName() << "\n";
    if(!UsedSymbols.empty()) {
        sstream.indent(HAKCInfo::IndentSpaces()) << "Used-Symbols:\n";
        for(auto &Symbol : UsedSymbols) {
            sstream.indent(2 * HAKCInfo::IndentSpaces()) << "- " << Symbol->GetName() << "\n";
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
