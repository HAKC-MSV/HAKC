//
// Created by de29664 on 10/28/24.
//

#ifndef HAKC_HAKCOSTREAM_H
#define HAKC_HAKCOSTREAM_H

#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DebugInfoMetadata.h"

#include "HAKCCompartmentalizationPolicy/HAKCCompartment.h"
#include "HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"

using namespace llvm;

namespace hakc {

    class HAKCOstream {
    public:
        HAKCOstream();

        raw_ostream &GetOS();

    protected:
        raw_ostream &os;

        void printDIType(const DIType *type, unsigned indents);


    public:
        friend HAKCOstream &operator<<(HAKCOstream &hos, llvm::Value *V) {
            if (V == nullptr) {
                hos.os << "!!nullptr!!";
            } else if (auto *F = dyn_cast<Function>(V)) {
                hos.os << "Function " << F->getName();
            } else if (auto *GV = dyn_cast<GlobalVariable>(V)) {
                hos.os << "Global " << GV->getName();
            } else if (auto *Arg = dyn_cast<Argument>(V)) {
                hos.os << "Argument " << Arg->getArgNo() << " of " << Arg->getParent()->getName();
            } else {
                hos.os << *V;
            }

            return hos;
        }

        friend HAKCOstream &operator<<(HAKCOstream &hos, llvm::Value &V) {
            hos << &V;
            return hos;
        }

        friend HAKCOstream &operator<<(HAKCOstream &hos, StringRef str) {
            hos.os << str;
            return hos;
        }

        friend HAKCOstream &operator<<(HAKCOstream &hos, unsigned int i) {
            hos.os << i;
            return hos;
        }

        friend HAKCOstream &operator<<(HAKCOstream &hos, unsigned long i) {
            hos.os << i;
            return hos;
        }

        friend HAKCOstream &operator<<(HAKCOstream &hos, bool b) {
            if(b) {
                hos.os << "True";
            } else {
                hos.os << "False";
            }
            return hos;
        }

        friend HAKCOstream &operator<<(HAKCOstream &hos, Function &F) {
            F.print(hos.os, nullptr);
            return hos;
        }

        friend HAKCOstream &operator<<(HAKCOstream &hos, Module &M) {
            M.print(hos.os, nullptr);
            return hos;
        }

        friend HAKCOstream &operator<<(HAKCOstream &hos, Module *M) {
            hos << *M;
            return hos;
        }

        friend HAKCOstream &operator<<(HAKCOstream &hos, Type *Ty) {
            hos << *Ty;
            return hos;
        }

        friend HAKCOstream &operator<<(HAKCOstream &hos, Type &Ty) {
            hos.os << Ty;
            return hos;
        }

        friend HAKCOstream &operator<<(HAKCOstream &hos, const DINode *DiNode) {
            hos << *DiNode;
            return hos;
        }

        friend HAKCOstream &operator<<(HAKCOstream &hos, const DINode &DiNode) {
            hos.os << DiNode;
            return hos;
        }

        friend HAKCOstream &operator<<(HAKCOstream &hos, const DIType *DiType) {
            hos.printDIType(DiType, 0);
            return hos;
        }

        friend HAKCOstream& operator<<(HAKCOstream &hos, const hakc::HAKCCompartment &Compartment) {
            hos.os << "Compartment " << Compartment.GetCompartmentIDValue();
            return hos;
        }

        friend HAKCOstream& operator<<(HAKCOstream &hos, const hakc::HAKCCompartmentDivision &Division) {
            hos << Division.GetHAKCCompartment();
            hos.os << " Division " << Division.GetDivisionID()->getZExtValue();
            return hos;
        }
    };


} // hakc

#endif //HAKC_HAKCOSTREAM_H
