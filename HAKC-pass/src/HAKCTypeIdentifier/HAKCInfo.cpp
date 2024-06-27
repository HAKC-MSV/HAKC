//
// Created by de29664 on 5/2/23.
//

#include "HAKCTypeIdentifier/HAKCInfo.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"

#include "llvm/Support/FileSystem.h"

#include <sstream>

namespace hakc {
    StringRef HAKCInfo::GetName() const {
        return Name;
    }

    HAKCInfo::HAKCInfo(StringRef Name, bool DebugActive) : DebugActive(DebugActive), Name(Name.str()) {
        if (Name.empty()) {
            CommonHAKCAnalysis::getWriter() << "Name is empty!\n";
            throw std::exception();
        }
    }

    raw_ostream &operator<<(raw_ostream &os, HAKCInfo &Info) {
        os << Info.GetYaml(HAKCInfo::IndentSpaces());
        return os;
    }

    unsigned int HAKCInfo::IndentSpaces() {
        return 4;
    }

    unsigned int HAKCInfo::EntrySpaces() {
        return 2;
    }

    std::string HAKCInfo::GetYamlHeader(unsigned int Indents) const {
        std::string Yaml;
        llvm::raw_string_ostream sstream(Yaml);

        sstream << GetYamlIdentifier() << "\n";
        sstream.indent(Indents + EntrySpaces()) << "Name: \"" << GetName() << "\"";

        return Yaml;
    }
} // hakc
