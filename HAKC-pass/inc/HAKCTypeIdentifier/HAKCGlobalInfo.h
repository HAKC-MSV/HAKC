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

        virtual ~HAKCGlobalInfo() = default;

        void addEscapingSymbol(std::string escapingSymbol);

        virtual bool isDefinedInCU();

        virtual GlobalObject *getValue();

        virtual std::string getTypeStringRepresentation() override;

        virtual std::string getYaml() override;

        virtual std::string getName() override;

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
