//
// Created by al32163 on 10/23/2024
//

#ifndef HAKC_HAKCALLOCATIONSIZE_H
#define HAKC_HAKCALLOCATIONSIZE_H

#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/StringRef.h"

#include "HAKCCompartment.h"
#include "HAKCFile.h"
#include "HAKCSymbol.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"

using namespace llvm;

namespace hakc {
    // class CommonHAKCAnalysis;
    // using llvm data structures
    class HAKCAllocationSize {

    public:
        // input is string
        // needs to be parsed 
        // split with split stringref
        SmallVector<StringRef> tokens;
        SmallVector<unsigned> args = {0,0};

        HAKCAllocationSize(StringRef str) {
            // TODO: ask derrick why I cant print here 
            str.split(tokens, "|"); 
            if(tokens.size() == 3){
                tokens[2].getAsInteger(10, args[0]);
                // CommonHAKCAnalysis::getWriter() << "in parse found 3 tokens: " << tokens[0] << ", " << tokens[1] << ", " << tokens[2] << "\n";
            }
            else if(tokens.size() == 4){
                tokens[2].getAsInteger(10, args[0]);
                tokens[3].getAsInteger(10, args[1]);
                // CommonHAKCAnalysis::getWriter() << "in parse found 4 tokens: " << tokens[0] << ", " << tokens[1] << ", " << tokens[2] << ", " << tokens[3] << "\n";
            }
            else{
                // CommonHAKCAnalysis::getWriter() << "\t in parse found invalid tokens of size: " << tokens.size() << "\n";
            }
        }

        ConstantInt *GetSize(Value * val);
        // TODO: should these be static?
        ConstantInt *simpleArgumentSize(Value *allocation);

        ConstantInt *simpleStaticSize(Value *allocation);

        ConstantInt *staticPlusArgument(Value *allocation);

        ConstantInt *multiplyTwoArguments(Value *allocation);

        ConstantInt *argumentGEP(Value *allocation);

    };

} // hakc

#endif //HAKC_HAKCALLOCATIONSIZE_H
