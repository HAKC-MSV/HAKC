//
// Created by de29664 on 3/21/23.
//
#include "HAKCFile.h"

namespace hakc {
    HAKCFile::HAKCFile(YamlFile &file) : path(file.name),
                                         guid(file.guid) {
    }

    StringRef HAKCFile::GetPath() {
        return path;
    }

    int64_t HAKCFile::GetGUID() {
        return guid;
    }
}