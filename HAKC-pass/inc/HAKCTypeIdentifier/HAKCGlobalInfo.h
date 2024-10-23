//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCGLOBALINFO_H
#define HAKC_HAKCGLOBALINFO_H

#include "llvm/IR/GlobalObject.h"
#include "HAKCInfo.h"

namespace hakc {

    class HAKCTypeIdentifier;

    class HAKCInfo;

    class HAKCGlobalInfo : public HAKCInfo {
    public:
        HAKCGlobalInfo(GlobalObject *GO, HAKCTypeIdentifier &identifier, StringRef directory, StringRef file,
                       unsigned line);

        ~HAKCGlobalInfo() = default;

        void addEscapingSymbol(std::string escapingSymbol);

        bool isDefinedInCU();

        GlobalObject *getValue();

        std::string getTypeStringRepresentation() ;

        std::string getYaml();

        std::string getName();

        void setUsedInIndirectCalls();

        void setUsedInUnknownOriginStore();

    protected:
        GlobalObject *GO;
        std::set<std::string> escapingSymbols;
        bool usedInIndirectCalls;
        bool usedInUnknownOriginStore;
    };

} // hakc

#endif //HAKC_HAKCGLOBALINFO_H
