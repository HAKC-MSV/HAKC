//
// Created by de29664 on 6/21/23.
//

#ifndef HAKC_HAKCDEBUGINFOPROCESSOR_H
#define HAKC_HAKCDEBUGINFOPROCESSOR_H

#include "llvm/IR/Module.h"
#include "llvm/IR/DebugInfo.h"

using namespace llvm;

namespace hakc {

    class HAKCDebugInfoProcessor {
    public:
        explicit HAKCDebugInfoProcessor(Module &M);

        const StructLayout *getStructLayout(StructType *structType);

        static bool functionShouldBeSkipped(Function *F);

        const DIType *findDiType(StructType *StructTy);

        static std::string getStructNamePrefix(const DIType *diType);

        StructType *getStructType(const DICompositeType *diCompositeType);

        uint64_t getDITypeSizeInBits(const DIType *diType);

        bool derivedTypeIsPointer(const DIType *diType);

        const DIType *unwrapDIType(const DIType *diType);

        static void printDIType(const DIType *type, unsigned indents);

        bool debugActive();

        bool isAnonStructOrUnion(const DIType *diType);

    protected:
        Module &M;
        bool debug;
        DebugInfoFinder infoFinder;

        const DIType *getBaseDefinition(const MDNode *metadata);

        DIType *getValueMetadataDIType(Value *v);

        bool isPointerToAnonStructOrUnion(const DIType *diType);

        bool isArrayOfAnonStructsOrUnions(const DIType *diType);
    };

} // hakc

#endif //HAKC_HAKCDEBUGINFOPROCESSOR_H
