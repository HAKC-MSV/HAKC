//
// Created by de29664 on 6/23/23.
//

#ifndef HAKC_HAKCFUNCTIONDEFINITION_H
#define HAKC_HAKCFUNCTIONDEFINITION_H

#include <llvm/ADT/StringRef.h>

using namespace llvm;

namespace hakc {

    class HAKCFunctionDefinition {
    public:
        HAKCFunctionDefinition(StringRef Name);

        StringRef GetName() const;

    protected:
        StringRef Name;
    };

    typedef std::shared_ptr<HAKCFunctionDefinition> hakc_function_def_t;

} // hakc

#endif //HAKC_HAKCFUNCTIONDEFINITION_H
