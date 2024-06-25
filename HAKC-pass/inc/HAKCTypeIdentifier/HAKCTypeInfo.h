//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCTYPEINFO_H
#define HAKC_HAKCTYPEINFO_H

#include "HAKCTypeIdentifier/HAKCInfo.h"

#include <map>
#include <llvm/IR/DebugInfoMetadata.h>

namespace hakc {

    class HAKCTypeInfo : public HAKCInfo {
    public:
        HAKCTypeInfo(StringRef Name, bool DebugActive);

        virtual ~HAKCTypeInfo() = default;

        void AddMember(const std::shared_ptr<HAKCTypeInfo> &TypeUse, unsigned BitOffset);

        std::string GetYaml(unsigned Indents) override;

        void SetSizeInBits(unsigned Size);

        unsigned GetSizeInBits();

        const DIType *GetDbgType();

        void SetDbgType(const DIType *DbgType);

        void SetDbgTypeName(std::string DbgTypeName);

        Type *GetLLVMType();

        void SetLLVMType(Type *Ty);

        std::string GetYamlHeader(unsigned Indents) const override;

        StringRef GetYamlIdentifier() const override;

    protected:
        std::map<unsigned, std::set<std::shared_ptr<HAKCTypeInfo>>> Members;
        unsigned SizeInBits;
        const DIType *DbgType;
        Type *LLVMType;
        std::string DbgTypeName;

    public:
        friend bool operator==(const HAKCTypeInfo &lhs, const HAKCTypeInfo &rhs) {
            if (lhs.DbgType && rhs.DbgType) {
                return lhs.DbgType == rhs.DbgType;
            } else if (lhs.LLVMType && rhs.LLVMType) {
                return lhs.LLVMType == rhs.LLVMType;
            }

            return lhs.GetName() == rhs.GetName();
        }

        friend bool operator!=(const HAKCTypeInfo &lhs, const HAKCTypeInfo &rhs) {
            return !(lhs == rhs);
        }

        friend bool operator==(const std::shared_ptr<HAKCTypeInfo> &lhs, const std::shared_ptr<HAKCTypeInfo> &rhs) {
            return *lhs == *rhs;
        }

        friend bool operator!=(const std::shared_ptr<HAKCTypeInfo> &lhs, const std::shared_ptr<HAKCTypeInfo> &rhs) {
            return !(*lhs == *rhs);
        }
    };

} // hakc

#endif //HAKC_HAKCTYPEINFO_H
