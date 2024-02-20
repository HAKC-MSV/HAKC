//
// Created by de29664 on 5/2/23.
//

#include "HAKCTypeIdentifier/HAKCInfo.h"
#include "HAKCTypeIdentifier/HAKCHash.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

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
        std::error_code err;
        SmallString<256> RealPath;
        std::string InitialPath = getDefinitionDirectory().str();
        if (!getDefinitionDirectory().endswith(sys::path::get_separator())) {
            InitialPath += sys::path::get_separator();
        }
        InitialPath += getDefinitionFile();

        err = sys::fs::real_path(InitialPath, RealPath, true);
        if (err) {
            CommonHAKCAnalysis::getWriter() << "Could not get real path to " << InitialPath << "\n";
            throw std::exception();
        }

        out << "-\n";
        out << "  name: " << getName() << "\n";
        out << "  file: " <<
            (getDefinitionDirectory().empty() ? "" : HAKCTypeIdentifier::GetTransformedPath(RealPath))
            << "\n";
        out << "  line: " << std::to_string(getDefinitionLine()) << "\n";
        out << "  type: " << getHash() << "\n";
        return out.str();
    }

    std::string HAKCInfo::getHash() {
        HAKCHash hash;
        hash.update(getTypeStringRepresentation());
        hash.final();
        return hash.digest();
    }
} // hakc
