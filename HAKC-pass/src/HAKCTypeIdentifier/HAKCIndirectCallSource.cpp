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
        sstream.indent(Indents + 2) << "Type:\n" << HAKCType->GetYamlHeader(Indents + 2 + HAKCInfo::IndentSpaces());
        if (!SourcePath.empty()) {
            sstream << "\n";
            sstream.indent(Indents + 2) << "Source:\n";
            unsigned Count = 0;
            for (auto &link: SourcePath) {
                sstream << link->GetYaml(Indents + 2 + HAKCInfo::IndentSpaces());
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

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(Argument *Arg, const std::shared_ptr<HAKCTypeInfo> &HAKCType,
                                                           bool Debug) :
            HAKCInfo("Argument Indirect Call Link", Debug), LinkYamlTokens() {
        std::string Yaml = HAKCInfo::GetYamlHeader(0);
        SplitString(Yaml, 0);

        Yaml = "";
        llvm::raw_string_ostream sstream(Yaml);
        sstream.indent(2) << "Link-Type: " << "\"Argument\"";
        LinkYamlTokens.push_back(Yaml);

        Yaml = "";
        sstream.indent(2) << "ArgNumber: " << Arg->getArgNo();
        LinkYamlTokens.push_back(Yaml);

        Yaml = "";
        sstream.indent(2) << "Function: \"" << Arg->getParent()->getName() << "\"";
        LinkYamlTokens.push_back(Yaml);

        Yaml = "";
        sstream.indent(2) << "Type:";
        LinkYamlTokens.push_back(Yaml);

        SplitTypeYaml(HAKCType);
    };

    void HAKCIndirectCallSourceLink::SplitString(StringRef S, unsigned Indents) {
        llvm::SmallVector<StringRef> SplitTokens;
        S.split(SplitTokens, "\n");
        std::string Yaml;
        llvm::raw_string_ostream sstream(Yaml);
        for (auto Tok: SplitTokens) {
            Yaml = "";
            sstream.indent(Indents) << Tok;
            LinkYamlTokens.push_back(Yaml);
        }
    }

    void HAKCIndirectCallSourceLink::SplitTypeYaml(const std::shared_ptr<HAKCTypeInfo> &HAKCType) {
        auto TypeTokensStr = HAKCType->GetYamlHeader(0);
        SplitString(TypeTokensStr, HAKCInfo::IndentSpaces());
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCGlobalInfo> &GlobalInfo,
                                                           bool Debug)
            : HAKCInfo("Global Indirect Call Link", Debug), LinkYamlTokens() {
        InputHAKCSymbol(GlobalInfo);
    }



    void HAKCIndirectCallSourceLink::InputHAKCSymbol(const std::shared_ptr<HAKCSymbolInfo> &HAKCSymbol) {
        std::string Yaml = HAKCInfo::GetYamlHeader(0);
        SplitString(Yaml, 0);

        Yaml = "";
        llvm::raw_string_ostream sstream(Yaml);
        sstream.indent(2) << "Link-Type: " << "\"Global\"";
        LinkYamlTokens.push_back(Yaml);

        Yaml = "";
        sstream.indent(2) << "Name: " << "\"" << HAKCSymbol->GetGlobalObj()->getName() << "\"";
        LinkYamlTokens.push_back(Yaml);

        Yaml = "";
        sstream.indent(2) << "Type:";
        LinkYamlTokens.push_back(Yaml);

        SplitTypeYaml(HAKCSymbol->GetType());
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCSymbolInfo> &HAKCSymbol,
                                                           bool Debug) : HAKCInfo("Global Indirect Call Link", Debug),
                                                                         LinkYamlTokens() {
        InputHAKCSymbol(HAKCSymbol);
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCGlobalInfo> &GlobalInfo,
                                                           int OffsetInBits,
                                                           bool Debug) : HAKCInfo("Global Member Indirect Call Link",
                                                                                  Debug), LinkYamlTokens() {
        std::string Yaml = HAKCInfo::GetYamlHeader(0);
        SplitString(Yaml, 0);

        Yaml = "";
        llvm::raw_string_ostream sstream(Yaml);

        sstream.indent(2) << "Link-Type: " << "\"Global-Member\"";
        LinkYamlTokens.push_back(Yaml);

        Yaml = "";
        sstream.indent(2) << "Offset: " << OffsetInBits;
        LinkYamlTokens.push_back(Yaml);

        Yaml = "";
        sstream.indent(2) << "Type:";
        LinkYamlTokens.push_back(Yaml);
        SplitTypeYaml(GlobalInfo->GetType());
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCTypeInfo> &HAKCType,
                                                           int OffsetInBits, bool Debug) : HAKCInfo(
            "Type member dereference", Debug), LinkYamlTokens() {
        std::string Yaml = HAKCInfo::GetYamlHeader(0);
        SplitString(Yaml, 0);

        Yaml = "";
        llvm::raw_string_ostream sstream(Yaml);

        sstream.indent(2) << "Link-Type: " << "\"Pointer-Dereference\"";
        LinkYamlTokens.push_back(Yaml);

        Yaml = "";
        sstream.indent(2) << "Offset: " << OffsetInBits;
        LinkYamlTokens.push_back(Yaml);

        Yaml = "";
        sstream.indent(2) << "Type:";
        LinkYamlTokens.push_back(Yaml);

        SplitTypeYaml(HAKCType);
    }

    void HAKCIndirectCallSourceLink::InputLinkType(StringRef LinkType) {
        std::string Yaml = "";
        llvm::raw_string_ostream sstream(Yaml);

        sstream.indent(2) << "Link-Type: " << "\"" << LinkType << "\"";
        LinkYamlTokens.push_back(Yaml);
    }

    void HAKCIndirectCallSourceLink::InputType(const std::shared_ptr<HAKCTypeInfo> &HAKCType) {

    }

    void HAKCIndirectCallSourceLink::InputGlobalObject(GlobalObject *GlobalObj) {

    }

    void HAKCIndirectCallSourceLink::InputYamlHeader() {
        std::string Yaml = HAKCInfo::GetYamlHeader(0);
        SplitString(Yaml, 0);
    }

    std::string HAKCIndirectCallSourceLink::GetYaml(unsigned int Indents) {
        std::string Yaml;
        llvm::raw_string_ostream sstream(Yaml);

        unsigned Count = 0;
        for (const auto &YamlLine: LinkYamlTokens) {
            sstream.indent(Indents) << YamlLine;
            if (++Count < LinkYamlTokens.size()) {
                sstream << "\n";
            }
        }

        return Yaml;
    }
} // hakc
