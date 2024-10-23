//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCTYPEINFO_H
#define HAKC_HAKCTYPEINFO_H

#include "HAKCTypeIdentifier/HAKCInfo.h"
#include "llvm/IR/DebugInfo.h"
#include "HAKCTypeIdentifier.h"

namespace hakc {
    class HAKCTypeIdentifier;

    class HAKCTypeInfo : public HAKCInfo {
    public:
        HAKCTypeInfo(const DIType *DiType, Type *Ty, HAKCTypeIdentifier &identifier);

        HAKCTypeInfo(Type *Ty, HAKCTypeIdentifier &identifier);

        ~HAKCTypeInfo() = default;

        std::string getTypeStringRepresentation();

        std::string getYaml();

        std::string getName();

        std::set<GlobalObject *> getUsers();

        void addUser(GlobalObject *User);

        void addSubmemberEscape(std::string escaper, int64_t memberOffset);

        const DIType *getDiType();

        Type *getType();

        std::string getHash();

        static std::string hashType(Type *Ty);

        static std::string getTypeString(Type *Ty);

    protected:
        std::set<GlobalObject *> users;
        Type *Ty;
        const DIType *DiType;
        std::set<std::pair<std::string, int64_t>> escapingOffsets;
        std::string name;

        const DIType *getBaseType(const DIType *diType);
    };

} // hakc

#endif //HAKC_HAKCTYPEINFO_H
