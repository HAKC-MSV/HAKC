//
// Created by de29664 on 12/5/24.
//

#ifndef HAKCWRITER_H
#define HAKCWRITER_H

#include "HAKCAnalysis/ManagedHAKCPointer.h"

#include "HAKCTypeIdentifier/HAKCTypeInfo.h"

#include "llvm/IR/Value.h"

#include "HAKCCompartmentalizationPolicy/HAKCCompartment.h"

#include "HAKCSystem/yaml/HAKCYaml.h"

using namespace llvm;

namespace hakc {
    class HAKCWriter {
    public:
        HAKCWriter();
        raw_ostream& ostream();

    protected:
        raw_ostream &os;

        void printDIType(const DIType *type, unsigned indents);

    public:
        HAKCWriter &operator<<(llvm::Value *V);

        HAKCWriter &operator<<(llvm::Value &V);

        HAKCWriter &operator<<(StringRef str);

        HAKCWriter &operator<<(unsigned int i);

        HAKCWriter &operator<<(unsigned long i);

        //HAKCWriter &operator<<(bool b);

        HAKCWriter &operator<<(Function &F);

        HAKCWriter &operator<<(Module &M);

        HAKCWriter &operator<<(Module *M);

        HAKCWriter &operator<<(Type *Ty);

        HAKCWriter &operator<<(Type &Ty);

        HAKCWriter &operator<<(const DINode *DiNode);

        HAKCWriter &operator<<(const DINode &DiNode);

        HAKCWriter &operator<<(const DIType *DiType);

        HAKCWriter &operator<<(const hakc::HAKCCompartment &Compartment);

        HAKCWriter &operator<<(hakc::HAKCCompartmentDivision &Division);

        HAKCWriter &operator<<(hakc::HAKCTypeInfo &TypeInfo);

        HAKCWriter &operator<<(enum HAKCAllocationTypeEnum AllocationType);

        HAKCWriter &operator<<(const ManagedHAKCPointerUse &HAKCPointerUse);

        HAKCWriter &operator<<(const HAKCPointerBase &ManagedPointer);

        HAKCWriter &operator<<(const HAKCPointerBaseP &ManagedPointer);

        HAKCWriter &operator<<(HAKCFunctionInfo &HAKCFuncInfo);
    };
} // hakc

#endif //HAKCWRITER_H
