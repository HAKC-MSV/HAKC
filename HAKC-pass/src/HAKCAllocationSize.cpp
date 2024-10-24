//
// Created by al32163 on 10/23/2024
//

#include "HAKCSystemInformation.h"
#include "HAKCCompartment.h"
#include "HAKCFile.h"
#include "HAKCSymbol.h"
#include "HAKCAnalysis/CommonHAKCAnalysis.h"
#include "HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "HAKCAllocationSize.h"
#include <memory>

#include "llvm/Support/YAMLParser.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

namespace hakc {

    ConstantInt *HAKCAllocationSize::simpleArgumentSize(Value *allocation) {
        if (CallInst *call = dyn_cast<CallInst>(allocation)) {
            IRBuilder<> irBuilder(call);
            Value *size = call->getArgOperand(args[0]);
            size = irBuilder.CreateZExtOrBitCast(size, irBuilder.getInt64Ty());
            ConstantInt* CI = dyn_cast<ConstantInt>(size);
            return CI;
        }
        return nullptr;
    }

    ConstantInt *HAKCAllocationSize::simpleStaticSize(Value *allocation) {
        return ConstantInt::get(Type::getInt64Ty(allocation->getContext()), args[0], false);
    }

    ConstantInt *HAKCAllocationSize::staticPlusArgument(Value *allocation) {
        if (CallInst *call = dyn_cast<CallInst>(allocation)) {
            ConstantInt *argumentSize = dyn_cast<ConstantInt>(call->getArgOperand(args[1]));
            return ConstantInt::get(Type::getInt64Ty(allocation->getContext()), argumentSize->getZExtValue() + args[0], false);
        }
        return nullptr;
    }

    ConstantInt *HAKCAllocationSize::multiplyTwoArguments(Value *allocation) {
        if (CallInst *call = dyn_cast<CallInst>(allocation)) {
            IRBuilder<> irBuilder(call);
            auto *int64Ty = irBuilder.getInt64Ty();
            /* Defying all reason, somehow some functions have different argument counts than
             * expected. See kmalloc_array in the IR for linereq_ioctl. So in that case, take
             * the lowest argument value.
             */
            Value *fullSize = nullptr;
            if (args[0] >= call->getNumArgOperands() || args[1] >= call->getNumArgOperands()) {
                if (args[0] <= args[1]) {
                    fullSize = call->getArgOperand(args[0]);
                } else {
                    fullSize = call->getArgOperand(args[1]);
                }
            } else {
                fullSize = irBuilder.CreateMul(
                        irBuilder.CreateZExt(call->getArgOperand(args[0]), int64Ty),
                        irBuilder.CreateZExt(call->getArgOperand(args[1]), int64Ty));
            }
            fullSize = irBuilder.CreateZExtOrBitCast(fullSize, int64Ty);
            ConstantInt* CI = dyn_cast<ConstantInt>(fullSize);
            return CI;
        }

        return nullptr;
    }

    ConstantInt *HAKCAllocationSize::argumentGEP(Value *allocation) {
        if (CallInst *call = dyn_cast<CallInst>(allocation)) {
            /*HAKCIRBuilder<> irBuilder(call);
            IntegerType *sizeTy = irBuilder.getInt64Ty();
            std::vector<Value*> indices;
            indices.push_back(ConstantInt::get(sizeTy, args[1], false));
            Value *gep = irBuilder.CreateGEP(sizeTy, call->getArgOperand(args[0]), indices);
            Value *size = irBuilder.CreateLoad(sizeTy, gep);
            return size;*/

            // TODO: Fix this
            return ConstantInt::get(Type::getInt64Ty(allocation->getContext()), 64, false);
        }

        return nullptr;
    }

    ConstantInt *HAKCAllocationSize::GetSize(Value *val){
        // set args from tokens 

        if(tokens[1] == "simpleArgumentSize"){
            return simpleArgumentSize(val);
        }
        else if(tokens[1] == "simpleStaticSize"){
            return simpleStaticSize(val);
        }
        else if(tokens[1] == "multiplyTwoArguments"){
            return multiplyTwoArguments(val);
        }
        else if(tokens[1] == "staticPlusArgument"){
            return staticPlusArgument(val);
        }
        else if(tokens[1] == "argumentGEP"){
            return argumentGEP(val);
        }
        else{
            CommonHAKCAnalysis::getWriter() << "tokens[1]: " << tokens[1] << " is not valid type\n";
            return nullptr;
        }
    }
    

} // hakc