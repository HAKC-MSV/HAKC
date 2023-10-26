//
// Created by de29664 on 5/2/23.
//

#include "HAKCTypeIdentifier/HAKCTypeInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "HAKCTypeIdentifier/HAKCHash.h"

#include <sstream>

namespace hakc {
    HAKCTypeInfo::HAKCTypeInfo(const DIType *DiType, Type *Ty, HAKCTypeIdentifier &identifier) :
            HAKCInfo(identifier, DiType->getDirectory(), DiType->getFilename(), DiType->getLine()),
            users(), Ty(Ty), DiType(DiType), escapingOffsets(), name() {
        if (DiType) {
            name = getBaseType(DiType)->getName().str();
        }
        if (name.empty()) {
            name = "type_";
            name += getHash();
        }
        if (StructType *StructTy = dyn_cast<StructType>(Ty)) {
            if (StructTy->isOpaque()) {
                CommonHAKCAnalysis::getWriter() << "Opaque StructTy: ";
                StructTy->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
                throw std::exception();
            }
        }
    }

    HAKCTypeInfo::HAKCTypeInfo(Type *Ty, HAKCTypeIdentifier &identifier) :
            HAKCInfo(identifier, "", "", 0), users(), Ty(Ty), DiType(nullptr), escapingOffsets(), name() {
        if (identifier.debugActive()) {
            CommonHAKCAnalysis::getWriter() << "HAKCTypeInfo: ";
            Ty->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }
        if (auto *StructTy = dyn_cast<StructType>(Ty)) {
            if (StructTy->isOpaque()) {
                CommonHAKCAnalysis::getWriter() << "Opaque StructTy: ";
                StructTy->print(CommonHAKCAnalysis::getWriter());
                CommonHAKCAnalysis::getWriter() << "\n";
                throw std::exception();
            }
            auto *DiType = identifier.findDiType(StructTy);
            if (DiType) {
                this->directory = DiType->getDirectory();
                this->file = DiType->getFilename();
                this->line = DiType->getLine();
                name = getBaseType(DiType)->getName().str();
                return;
            }
        }
        name = "type_";
        name += getHash();
    }

    std::string HAKCTypeInfo::getTypeStringRepresentation() {
        std::stringstream out;
        if (DiType && identifier.isAnonStructOrUnion(DiType)) {
            out << getDefinitionDirectory().str() << getDefinitionFile().str() << std::to_string(getDefinitionLine());
        } else {
            out << getTypeString(Ty);
        }
        out << "\n";
        return out.str();
    }

    std::string HAKCTypeInfo::getYaml() {
        std::stringstream out;
        out << HAKCInfo::getYaml();
        out << "  Users:\n";
        for (auto &user: getUsers()) {
            out << "    - " << user->getName().str() << "\n";
        }
        out << "  EscapingMembers:\n";
        for (auto &escapePair: escapingOffsets) {
            out << "    -\n";
            out << "      escaper: " << escapePair.first << "\n";
            out << "      offset: " << std::to_string(escapePair.second) << "\n";
        }

        if (auto *structType = dyn_cast<StructType>(Ty)) {
            const StructLayout *layout =
                    identifier.getStructLayout(structType);
            if (structType->getStructNumElements() > 0) {
                out << "  MemberOffsets:\n";
            }
            for (unsigned i = 0; i < structType->getStructNumElements(); i++) {
                Type *t = structType->getStructElementType(i);
                std::string hash = HAKCTypeInfo::hashType(t);
                out << "    -\n";
                out << "      type: " << hash << "\n";
                out << "      offset: " << std::to_string(layout->getElementOffset(i))
                    << "\n";
            }
        }

        out << "\n";
        return out.str();
    }

    std::string HAKCTypeInfo::getName() {
        return name;
    }

    std::set<GlobalObject *> HAKCTypeInfo::getUsers() {
        return users;
    }

    void HAKCTypeInfo::addUser(GlobalObject *GO) {
        if (identifier.debugActive()) {
            CommonHAKCAnalysis::getWriter() << "Adding " << GO->getName() << " as a user of " << getName() << "\n";
            identifier.printDIType(DiType, 0);
            CommonHAKCAnalysis::getWriter() << "\n";
        }
        if (GO->getName().empty()) {
            GO->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            throw std::exception();
        }
        users.insert(GO);
    }

    void HAKCTypeInfo::addSubmemberEscape(std::string escaper, int64_t memberOffset) {
        escapingOffsets.insert(std::make_pair(escaper, memberOffset));
    }

    const DIType *HAKCTypeInfo::getBaseType(const DIType *diType) {
        const DIDerivedType *diDerivedType;
        switch (diType->getTag()) {
            case dwarf::DW_TAG_rvalue_reference_type:
            case dwarf::DW_TAG_reference_type:
            case dwarf::DW_TAG_atomic_type:
            case dwarf::DW_TAG_restrict_type:
            case dwarf::DW_TAG_const_type:
            case dwarf::DW_TAG_typedef:
            case dwarf::DW_TAG_volatile_type:
            case dwarf::DW_TAG_enumeration_type:
                diDerivedType = dyn_cast<DIDerivedType>(diType);
                if (diDerivedType && diDerivedType->getBaseType()) {
                    const DIType *baseType = getBaseType(diDerivedType->getBaseType());
                    if (baseType) {
                        return baseType;
                    }
                }
                /* Purposeful fallthrough */
            default:
                return diType;
        }
    }

    const DIType *HAKCTypeInfo::getDiType() {
        return DiType;
    }

    Type *HAKCTypeInfo::getType() {
        return Ty;
    }

    std::string HAKCTypeInfo::getHash() {
        return HAKCInfo::getHash();
    }

    std::string HAKCTypeInfo::hashType(Type *Ty) {
        HAKCHash result;
        result.update(getTypeString(Ty));
        result.final();
        return result.digest();
    }

    std::string HAKCTypeInfo::getTypeString(Type *Ty) {
        std::string typeStr;
        raw_string_ostream ostream(typeStr);
        if (StructType *StructTy = dyn_cast<StructType>(Ty)) {
            for (auto *ElTy: StructTy->elements()) {
                if (StructType *ElStructTy = dyn_cast<StructType>(ElTy)) {
                    if (ElStructTy->isLiteral()) {
                        ostream << "anon";
                    } else {
                        ElTy->print(ostream);
                    }
                } else {
                    ElTy->print(ostream);
                }
            }
        } else {
            Ty->print(ostream);
        }
        return typeStr;
    }
} // hakc