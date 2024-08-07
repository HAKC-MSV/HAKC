//
// Created by de29664 on 6/11/24.
//

#ifndef HAKC_HAKCSYMBOLINFO_H
#define HAKC_HAKCSYMBOLINFO_H

#include "HAKCTypeInfo.h"
#include "llvm/IR/GlobalObject.h"

namespace hakc {
    class HAKCSymbolInfo : public HAKCInfo {
    public:
        HAKCSymbolInfo(StringRef Name, bool DebugActive);

        virtual ~HAKCSymbolInfo() = default;

        void SetType(std::shared_ptr<HAKCTypeInfo> HAKCType);

        std::shared_ptr<HAKCTypeInfo> GetType();

        void AddSymbolUse(const std::shared_ptr<HAKCSymbolInfo> &Symbol);

        std::string GetYaml(unsigned Indents) override;

        std::string GetYamlHeader(unsigned Indents) const override;

        GlobalObject *GetGlobalObj();

        void SetDefiningLocation(const DIFile *File, unsigned Line);

        void SetLocalScope(const DIScope *Scope);

    protected:
        std::shared_ptr<HAKCTypeInfo> Type;
        std::set<std::shared_ptr<HAKCSymbolInfo>> UsedSymbols;
        GlobalObject *GlobalObj;
        const DIType *DbgType;
        const DIFile *DefiningLocation;
        unsigned DefiningLine;
        const DIScope *LocalScope;

        void SetGlobalObj(GlobalObject *GlobalObj);

        std::string GetTransformedPathName(const DIFile *File) const;
    };
}

#endif //HAKC_HAKCSYMBOLINFO_H
