//
// Created by de29664 on 5/2/23.
//

#include "HAKCTypeIdentifier/HAKCInfo.h"
#include "HAKCTypeIdentifier/HAKCHash.h"

#include <sstream>

namespace hakc {
    HAKCInfo::HAKCInfo(HAKCTypeIdentifier &identifier, StringRef directory, StringRef file, unsigned int line) :
            identifier(identifier), directory(directory), file(file), line(line) {

    }

    StringRef HAKCInfo::getDefinitionDirectory() {
        return directory;
    }

    unsigned HAKCInfo::getDefinitionLine() {
        return line;
    }

    StringRef HAKCInfo::getDefinitionFile() {
        return file;
    }

    std::string HAKCInfo::getYaml() {
        std::stringstream out;
        out << "-\n";
        out << "  name: " << getName() << "\n";
        out << "  directory: " << getDefinitionDirectory().str() << "\n";
        out << "  file: " << getDefinitionFile().str() << "\n";
        out << "  line: " << std::to_string(getDefinitionLine()) << "\n";
        out << "  type: " << getHash() << "\n";
        return out.str();
    }

    std::string HAKCInfo::getHash() {
        HAKCHash hash;
        hash.update(getTypeStringRepresentation());
        hash.final();
        return hash.digest();
//        return getTypeStringRepresentation();
    }
} // hakc