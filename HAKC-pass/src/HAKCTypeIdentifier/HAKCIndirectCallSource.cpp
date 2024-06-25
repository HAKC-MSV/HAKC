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
        std::string Yaml = HAKCInfo::GetYamlHeader(Indents);
        llvm::raw_string_ostream sstream(Yaml);

        sstream << "\n";
        sstream.indent(Indents + 2) << "Type:\n" << HAKCType->GetYamlHeader(Indents + 3 * HAKCInfo::IndentSpaces())
                                    << "\n";
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

    StringRef HAKCIndirectCallSource::GetYamlIdentifier() const {
        return "!HAKCIndirectSource";
    }

    StringRef HAKCIndirectCallSourceLink::GetYamlIdentifier() const {
        return "!HAKCIndirectSourceLink";
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(Argument *Arg, bool Debug) :
    HAKCInfo("Argument Indirect Call Link", Debug), LinkYamlTokens() {
        std::string Yaml = HAKCInfo::GetYamlHeader(0);
        SplitString(Yaml);

        Yaml = "";
        llvm::raw_string_ostream sstream(Yaml);
        sstream << "Link-Type: " << "\"Argument\"";
        LinkYamlTokens.push_back(Yaml);

        Yaml = "";
        sstream << "ArgNumber: " << Arg->getArgNo();
        LinkYamlTokens.push_back(Yaml);

        Yaml = "";
        sstream << "Function: " << Arg->getParent()->getName();
        LinkYamlTokens.push_back(Yaml);
    };

    void HAKCIndirectCallSourceLink::SplitString(StringRef S) {
        llvm::SmallVector<StringRef> SplitTokens;
        S.split(SplitTokens, "\n");
        for(auto Tok : SplitTokens) {
            LinkYamlTokens.push_back(Tok.str());
        }
    }

    void HAKCIndirectCallSourceLink::SplitTypeYaml(const std::shared_ptr<HAKCTypeInfo> &HAKCType) {
        auto TypeTokensStr = HAKCType->GetYamlHeader(0);
        SplitString(TypeTokensStr);
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCGlobalInfo> &GlobalInfo,
                                                           bool Debug)
            : HAKCInfo("Global Indirect Call Link", Debug), LinkYamlTokens() {
        std::string Yaml = HAKCInfo::GetYamlHeader(0);
        SplitString(Yaml);

        Yaml = "";
        llvm::raw_string_ostream sstream(Yaml);
        sstream << "Link-Type: " << "\"Global\"";
        LinkYamlTokens.push_back(Yaml);

        Yaml = "";
        sstream << "Name: " << "\"" << GlobalInfo->GetName() << "\"";
        LinkYamlTokens.push_back(Yaml);

        SplitTypeYaml(GlobalInfo->GetType());
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCGlobalInfo> &GlobalInfo,
                                                           int OffsetInBits,
                                                           bool Debug) : HAKCInfo("Global Member Indirect Call Link",
                                                                                  Debug), LinkYamlTokens() {
        std::string Yaml = HAKCInfo::GetYamlHeader(0);
        SplitString(Yaml);

        Yaml = "";
        llvm::raw_string_ostream sstream(Yaml);

        sstream << "Link-Type: " << "\"Global-Member\"";
        LinkYamlTokens.push_back(Yaml);

        Yaml = "";
        sstream << "Offset: " << OffsetInBits;
        LinkYamlTokens.push_back(Yaml);

        SplitTypeYaml(GlobalInfo->GetType());
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCTypeInfo> &HAKCType,
                                                           int OffsetInBits, bool Debug) : HAKCInfo(
            "Type member dereference", Debug), LinkYamlTokens() {
        std::string Yaml = HAKCInfo::GetYamlHeader(0);
        SplitString(Yaml);

        Yaml = "";
        llvm::raw_string_ostream sstream(Yaml);

        sstream << "Link-Type: " << "\"Pointer-Dereference\"";
        LinkYamlTokens.push_back(Yaml);

        Yaml = "";
        sstream << "Offset: " << OffsetInBits;
        LinkYamlTokens.push_back(Yaml);

        SplitTypeYaml(HAKCType);
    }

    std::string HAKCIndirectCallSourceLink::GetYaml(unsigned int Indents) {
        std::string Yaml;
        llvm::raw_string_ostream sstream(Yaml);

        unsigned Count = 0;
        for(const auto& YamlLine : LinkYamlTokens) {
            sstream.indent(Indents) << YamlLine;
            if(++Count < LinkYamlTokens.size()) {
                sstream << "\n";
            }
        }

        return Yaml;
    }
} // hakc
