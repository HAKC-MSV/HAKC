//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCTYPEINFO_H
#define HAKC_HAKCTYPEINFO_H

#include "HAKCTypeIdentifier/HAKCInfo.h"
#include "HAKCCompartmentalizationPolicy/yaml/HAKCYamlType.h"

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

        void SetDbgType(const DIType *DiDbgType);

        void SetDbgTypeName(std::string DbgTypeNameStr);

        Type *GetLLVMType();

        void SetLLVMType(Type *Ty);

        std::string GetYamlHeader(unsigned Indents) const override;

        StringRef GetYamlIdentifier() const override;

        static StringRef UnknownType;

        bool IsPointerToPointer();

        std::shared_ptr<HAKCTypeInfo> GetPointeeType();

    protected:
        std::map<unsigned, std::set<std::shared_ptr<HAKCTypeInfo>>> Members;
        unsigned SizeInBits;
        const DIType *DbgType;
        Type *LLVMType;
        std::string DbgTypeName;

        bool IsPointerToPointer(const DIType *DiType);

        const DIType* StripTypeModifiers(const DIType *DiType);

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

        friend bool operator==(const HAKCYamlType &YamlType, const std::shared_ptr<HAKCTypeInfo> &TypeInfo) {
            std::string LLVMTypeStr;
            llvm::raw_string_ostream ostr(LLVMTypeStr);
            if (TypeInfo->GetLLVMType()) {
                ostr << *TypeInfo->GetLLVMType();
            }
            return YamlType.DebugType == TypeInfo->DbgTypeName ||
                   (!LLVMTypeStr.empty() && LLVMTypeStr == YamlType.LLVMType);
        }

        friend bool operator==(const std::shared_ptr<HAKCTypeInfo> &TypeInfo, const HAKCYamlType &YamlType) {
            return (YamlType == TypeInfo);
        }

        friend bool operator!=(const std::shared_ptr<HAKCTypeInfo> &TypeInfo, const HAKCYamlType &YamlType) {
            return !(YamlType == TypeInfo);
        }

        friend bool operator!=(const HAKCYamlType &YamlType, const std::shared_ptr<HAKCTypeInfo> &TypeInfo) {
            return !(YamlType == TypeInfo);
        }
    };

    typedef std::shared_ptr<HAKCTypeInfo> HAKCTypeP;

} // hakc

#endif //HAKC_HAKCTYPEINFO_H
