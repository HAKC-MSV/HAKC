//
// Created by de29664 on 5/2/23.
//

#include "HAKCTypeIdentifier/HAKCTypeInfo.h"

#include "llvm/Support/raw_ostream.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"

namespace hakc {
    HAKCTypeInfo::HAKCTypeInfo(StringRef Name, bool DebugActive) : HAKCInfo(Name, DebugActive), Members(),
                                                                   SizeInBits(0), DbgType(nullptr), LLVMType(nullptr),
                                                                   DbgTypeName() {

    }

    void HAKCTypeInfo::SetSizeInBits(unsigned int Size) {
        SizeInBits = Size;
    }

    unsigned HAKCTypeInfo::GetSizeInBits() {
        return SizeInBits;
    }

    const DIType *HAKCTypeInfo::GetDbgType() {
        return DbgType;
    }

    void HAKCTypeInfo::SetDbgType(const DIType *DbgType) {
        this->DbgType = DbgType;
    }

    void HAKCTypeInfo::AddMember(const std::shared_ptr<HAKCTypeInfo> &TypeUse, unsigned int BitOffset) {
        Members[BitOffset].insert(TypeUse);
    }

    void HAKCTypeInfo::SetDbgTypeName(std::string DbgTypeName) {
        this->DbgTypeName = DbgTypeName;
    }

    Type *HAKCTypeInfo::GetLLVMType() {
        return LLVMType;
    }

    void HAKCTypeInfo::SetLLVMType(Type *Ty) {
        if (!Ty) {
            CommonHAKCAnalysis::getWriter() << "Trying to set null LLVM Type for " << GetName() << "\n";
            throw std::exception();
        }
        if (LLVMType && Ty != LLVMType) {
            CommonHAKCAnalysis::getWriter() << "Trying to change LLVM Type for " << GetName() << " from " << *LLVMType
                                            << " to " << *Ty << "\n";
            throw std::exception();
        }
        LLVMType = Ty;
    }

    std::string HAKCTypeInfo::GetYamlHeader(unsigned Indents) {
        auto UnknownType = "@UNKNOWN@";

        std::string Yaml;
        llvm::raw_string_ostream sstream(Yaml);
        sstream.indent(Indents) << "- Name: \"" << GetName() << "\"\n";
        sstream.indent(Indents + 2) << "Debug-Type: \"";
        if (!DbgTypeName.empty()) {
            sstream << DbgTypeName;
        } else {
            sstream << UnknownType;
        }
        sstream << "\"\n";
        sstream.indent(Indents + 2) << "LLVM-Type: \"";
        if (LLVMType) {
            sstream << *LLVMType;
        } else {
            sstream << UnknownType;
        }
        sstream << "\"";

        return Yaml;
    }

    std::string hakc::HAKCTypeInfo::GetYaml(unsigned Indents) {
        std::string Yaml;
        llvm::raw_string_ostream sstream(Yaml);

        sstream << GetYamlHeader(Indents);
        if (!Members.empty()) {
            std::vector<unsigned> SortedBitOffsets;
            SortedBitOffsets.reserve(Members.size());
            for (auto &it: Members) {
                SortedBitOffsets.push_back(it.first);
            }

            llvm::sort(SortedBitOffsets.begin(), SortedBitOffsets.end());
            sstream.indent(Indents + 2) << "Members:\n";
            for (auto BitOffset: SortedBitOffsets) {
                auto MemberSet = Members[BitOffset];
                sstream.indent(Indents + HAKCInfo::IndentSpaces()) << "- Offset: " << BitOffset << "\n";
                sstream.indent(Indents + HAKCInfo::IndentSpaces() + 2) << "Type:\n";
                for (auto &Member: MemberSet) {
                    sstream << Member->GetYamlHeader(Indents + 2 * HAKCInfo::IndentSpaces()) << "\n";
                }
            }
        }

        return Yaml;
    }
} // hakc
