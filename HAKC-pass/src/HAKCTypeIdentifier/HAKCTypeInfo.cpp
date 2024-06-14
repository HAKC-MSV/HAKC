//
// Created by de29664 on 5/2/23.
//

#include "HAKCTypeIdentifier/HAKCTypeInfo.h"

#include "llvm/Support/raw_ostream.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"

namespace hakc {
    HAKCTypeInfo::HAKCTypeInfo(StringRef Name, bool DebugActive) : HAKCInfo(Name, DebugActive), Members(),
                                                                   SizeInBits(0) {

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

    std::string hakc::HAKCTypeInfo::GetYaml() {
        std::string Yaml;
        llvm::raw_string_ostream sstream(Yaml);

        sstream << "-\n";
        sstream.indent(HAKCInfo::IndentSpaces()) << "Name: " << GetName() << "\n";
        if (!Members.empty()) {
            std::vector<unsigned> SortedBitOffsets;
            SortedBitOffsets.reserve(Members.size());
            for (auto &it: Members) {
                SortedBitOffsets.push_back(it.first);
            }

            llvm::sort(SortedBitOffsets.begin(), SortedBitOffsets.end());
            sstream.indent(HAKCInfo::IndentSpaces()) << "Members:\n";
            for (auto BitOffset: SortedBitOffsets) {
                auto MemberSet = Members[BitOffset];
                sstream.indent(2 * HAKCInfo::IndentSpaces()) << "-\n";
                sstream.indent(2 * HAKCInfo::IndentSpaces()) << "  Offset: " << BitOffset << "\n";
                sstream.indent(2 * HAKCInfo::IndentSpaces()) << "  Type:\n";
                for(auto &Member : MemberSet) {
                    sstream.indent(3 * HAKCInfo::IndentSpaces()) << "-\n";
                    sstream.indent(3 * HAKCInfo::IndentSpaces()) << Member->GetName() << "\n";
                }
            }
        }

        return Yaml;
    }
} // hakc
