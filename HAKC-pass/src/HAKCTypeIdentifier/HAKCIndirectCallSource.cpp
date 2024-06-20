//
// Created by de29664 on 6/18/24.
//

#include "HAKCTypeIdentifier/HAKCIndirectCallSource.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"

namespace hakc {
    HAKCIndirectCallSource::HAKCIndirectCallSource(std::vector<std::shared_ptr<HAKCIndirectCallSourceLink>> SourcePath,
                                                   std::shared_ptr<HAKCTypeInfo> HAKCType,
                                                   bool debug) : HAKCInfo(HAKCType->GetName(), debug),
                                                                 HAKCType(HAKCType), SourcePath(SourcePath) {
    }

    std::string HAKCIndirectCallSource::GetYaml(unsigned Indents) {
        std::string Yaml;
        llvm::raw_string_ostream sstream(Yaml);

        sstream.indent(Indents) << "- Type: " << GetName() << "\n";
        if (!SourcePath.empty()) {
            sstream.indent(Indents + 2) << "Source:\n";
            unsigned Count = 0;
            for (auto &link: SourcePath) {
                sstream << link->GetYaml(Indents + 3 * HAKCInfo::IndentSpaces());
                if (++Count != SourcePath.size()) {
                    sstream << "\n";
                }
            }
        }

        return Yaml;
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(Argument *Arg, bool Debug) : HAKCInfo(
            "Argument Indirect Call Link", Debug),
                                                                                        LinkYaml() {
        llvm::raw_string_ostream sstream(LinkYaml);

        sstream << "- { Link-Type: \"Argument\", ArgNumber: " << Arg->getArgNo() << ", Function: \""
                << Arg->getParent()->getName() << "\" }";
    };

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCGlobalInfo> &GlobalInfo,
                                                           bool Debug)
            : HAKCInfo("Global Indirect Call Link", Debug), LinkYaml() {
        llvm::raw_string_ostream sstream(LinkYaml);

        sstream << "- { Link-Type: \"Global\", Name: \"" << GlobalInfo->GetName() << "\", Type: \""
                << GlobalInfo->GetType()->GetName() << "\" }";
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCGlobalInfo> &GlobalInfo,
                                                           int OffsetInBits,
                                                           bool Debug) : HAKCInfo("Global Member Indirect Call Link",
                                                                                  Debug), LinkYaml() {
        llvm::raw_string_ostream sstream(LinkYaml);

        sstream << "- { Link-Type: \"Global-Member\", Offset: " << OffsetInBits << ", Name: \"" << GlobalInfo->GetName()
                << "\", Type: \"" << GlobalInfo->GetType()->GetName() << "\" }";
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCTypeInfo> &HAKCType,
                                                           int OffsetInBits, bool Debug) : HAKCInfo(
            "Type member dereference", Debug), LinkYaml() {
        llvm::raw_string_ostream sstream(LinkYaml);

        sstream << "- { Link-Type: \"Pointer-Dereference\", Offset: " << OffsetInBits << ", Type: \""
                << HAKCType->GetName() << "\" }";
    }

    std::string HAKCIndirectCallSourceLink::GetYaml(unsigned int Indents) {
        std::string Yaml;
        llvm::raw_string_ostream sstream(Yaml);

        sstream.indent(Indents) << LinkYaml;

        return Yaml;
    }
} // hakc
