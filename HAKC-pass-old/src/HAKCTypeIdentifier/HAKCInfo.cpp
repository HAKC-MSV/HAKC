//
// Created by de29664 on 5/2/23.
//

#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "HAKCTypeIdentifier/HAKCInfo.h"
#include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"

namespace hakc {
    HAKCInfo::HAKCInfo(CommonHAKCAnalysis &Analysis, StringRef Name, bool DebugActive) : Analysis(Analysis),
        DebugActive(DebugActive), Name(Name.str()) {
        if (Name.empty()) {
            CommonHAKCAnalysis::getWriter() << "Name is empty!\n";
            throw std::exception();
        }
    }

    CommonHAKCAnalysis &HAKCInfo::GetCommonHAKCAnalysis() {
        return Analysis;
    }

    StringRef HAKCInfo::GetName() const {
        return Name;
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
