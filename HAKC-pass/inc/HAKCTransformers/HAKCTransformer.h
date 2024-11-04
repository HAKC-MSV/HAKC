//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCTRANSFORMER_H
#define HAKC_HAKCTRANSFORMER_H

#include <map>

#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"

#include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"
#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"
#include "HAKCAnalysis/HAKCModuleAnalysis.h"
#include "HAKCAnalysis/ManagedHAKCPointer.h"

using namespace llvm;

namespace hakc {

    /**
     * A virtual class that defines the API for creating HAKC transformations.
     * Must be subclassed to provide architecture specific functionality.
     */
    class HAKCTransformer {
    public:
        HAKCTransformer(HAKCCompartmentalizationPolicy &Policy, HAKCModuleAnalysis &HAKCAnalysis,
                        HAKCTypeIdentifier &TypeIdentifier);

        virtual ~HAKCTransformer() = default;

        /**
         * Create a pointer suitable for dereferencing
         * @param HAKCPointer
         * @param I
         * @return The last Instruction created, placed immediately prior to I
         */
        virtual Value *CreateSafePointer(ManagedHAKCPointerP HAKCPointer, Instruction *I);

        /**
         * Create a HAKC Pointer check at I
         * @param HAKCPointer
         * @param I
         * @return
         */
        virtual Value *CreateDataAuthentication(ManagedHAKCPointerP HAKCPointer, Instruction *I);

        /**
         * Create a HAKC Code Pointer check at I
         * @param HAKCPointer
         * @param I
         * @return
         */
        virtual Value *CreateCodeAuthentication(ManagedHAKCPointerP HAKCPointer, Instruction *I);

        /**
         * Computes the size of the transfer and then calls CreateSizedCompartmentTransfer
         * @param HAKCPointer
         * @param I
         * @param Target
         * @param IsData
         * @return
         */
        virtual Instruction *
        CreateCompartmentTransfer(ManagedHAKCPointerP HAKCPointer, Instruction *I, GlobalValue *Target, bool IsData);

        /**
         * Creates a Compartment Transfer of ManagedHAKCPointer at I. The arguments to the transfer function are:
         *  0. ManagedHAKCPointer
         *  1. Size of transfer
         *  2. IsData
         *  3. Target CompartmentID
         *  4. OtherArgs
         *
         *  This order is to allow for additional architecture specific information to be passed if needed
         *
         * @param HAKCPointer
         * @param I
         * @param Target
         * @param IsPerCPU
         * @param IsData
         * @param Size
         * @return
         */
        virtual Instruction *
        CreateSizedCompartmentTransfer(ManagedHAKCPointerP HAKCPointer, Instruction *I, GlobalValue *Target,
                                       bool IsData, ConstantInt *Size);

        /**
         * Creates a BitCastInst of Operand to TargetType at I
         * @param Operand
         * @param TargetType
         * @param I
         * @return
         */
        virtual Value *CreateBitCast(hakc::ManagedHAKCPointerP Operand, Type *TargetType, Instruction *I);


        /**
         * Create a signed pointer using the color of HAKCPointer
         * @param HAKCPointer
         * @param I
         * @param Target
         * @param IsData
         * @return
         */
        virtual Instruction *
        CreateSignWithColor(ManagedHAKCPointerP HAKCPointer, Instruction *I, GlobalValue *Target, bool IsData);


        /**
         * Create a Outside Transfer Function
         * @param F
         * @return
         */
        virtual Function *CreateTransferFunction(Function *F);

        /**
         * Create a transfer function to a variadic function in a different compartment
         * @param Call
         * @return
         */
        virtual Function *CreateTransferToVariadic(CallInst *Call);

        /**
         * Create Architecture specific transformations for a new transfer function
         * @param Original
         * @param Transfer
         */
        virtual void CreateTransferFunctionFinalize_Arch(Function *Original, Function *Transfer);

        /**
         * Perform architecture specific transformations prior to an argument transfer to a target compartment
         * @param F
         * @param TransferFunction
         * @param Arg
         */
        virtual void CreateTransferFunctionArg_PreCall(Function *F, Function *TransferFunction, Value *Arg);

        /**
         * Perform architecture specific transformations after the cross compartment function call
         * @param F
         * @param TransformFunction
         * @param Arg
         */
        virtual void CreateTransferFunctionArg_PostCall(Function *F, Function *TransformFunction, Value *Arg);

        virtual Module &getModule();

        virtual Type *HAKCAuthenticationRetType(unsigned AddrSpace);

        virtual hakc_compartment_id_t getFunctionCompartmentID(Function *F);

        virtual hakc_compartment_id_t getGlobalCompartmentID(GlobalVariable *GV);

        virtual ConstantInt *GetHAKCCompartmentValue(hakc_compartment_id_t CompartmentID);

        virtual ConstantInt *getTrue();

        virtual ConstantInt *getFalse();

        virtual ConstantInt *getInt64(int64_t Value);

        virtual ConstantInt *getInt32(int32_t Value);

        virtual ConstantInt *GetDefaultObjectSize();

        /**
        * Returns true if ManagedHAKCPointer is an appropriately sized integer for use as a pointer
        * @param HAKCPointer
        */
//        virtual bool ValidateHAKCIntegerPointerSize(ManagedHAKCPointerP HAKCPointer);

        virtual unsigned GetPointerAddrSpace(ManagedHAKCPointerP HAKCPointer);

        virtual unsigned GetPointerAddrSpace(Value *V);

        /**
         * Creates metadata associated with a Compartment for proper loading by the kernel
         * @param CompartmentID
         * @return
         */
        virtual GlobalVariable *AddCompartmentMetadataEntry(HAKCCompartment &Compartment);

        virtual Function *PopulateGlobalTransfer(Function *GlobalTransfer, GlobalVariable *GlobalVar, bool Debug);

    protected:
        IRBuilder<> HAKCIRBuilder;
        HAKCCompartmentalizationPolicy &CompartmentalizationPolicy;
        HAKCModuleAnalysis &ModuleAnalysis;
        HAKCTypeIdentifier &TypeIdentifier;

        std::map<Function *, Function *> VariadicTransferFunctions;

        /**
         * Checks that ManagedHAKCPointer and I are valid, and sets the HAKCIRBuilder location to I
         * @param HAKCPointer
         * @param I
         */
        void ValidateHAKCPointerAndLocation(const ManagedHAKCPointerP &HAKCPointer, Instruction *I);

        /**
         * Performs the transformations needed for creating a safe pointer
         * @param HAKCPointer
         * @param I
         * @return
         */
        virtual Value *CreateSafePointer_Arch(ManagedHAKCPointerP HAKCPointer, Instruction *I) = 0;

        /**
         * Creates a Call to the specified function
         * @param name
         * @param RetTy
         * @param Args
         * @return
         */
        virtual CallInst *CreateCall(StringRef name, Type *RetTy, ArrayRef<Value *> Args);

        virtual Instruction *
        CreateCallWithResultCast(StringRef Name, Type *RetTy, ArrayRef<Value *> Args, Value *ValueToTypeMatch);

        /**
         * Gets or inserts the GlobalVariable containing the list of valid targets from the Compartment F belongs to
         * @param F
         * @return
         */
        virtual GlobalVariable *GetValidTargetCompartments(Function *F);

        /**
         * Return the type that HAKC Compartment Entry Tokens are in the source
         * @return
         */
        virtual Type *GetEntryTokenType(unsigned AddrSpace) = 0;

        /**
         * Returns the Entry Token for the given CompartmentID and Value
         * @return
         */
        virtual Constant *GetEntryToken(HAKCCompartment &CompartmentDivision) = 0;

        virtual ConstantInt *GetObjectSizeInBytes(hakc::ManagedHAKCPointerP HAKCPointer);

        virtual ConstantInt *GetObjectSizeInBytes(hakc::HAKCTypeP HAKCType);

        virtual FunctionType *GetHAKCDataAuthenticationFunctionType(unsigned AddrSpace) = 0;

        /**
         * Create the argument set for a HAKC data check
         * @param HAKCPointer
         * @param I
         * @return
         */
        virtual void
        CreateDataAuthArguments(ManagedHAKCPointerP HAKCPointer, Instruction *I, SmallVector<Value *> &ArgsList) = 0;

        /**
         * Create the argument set for a HAKC code check
         * @param HAKCPointer
         * @param I
         * @return
         */
        virtual void
        CreateCodeAuthArguments(ManagedHAKCPointerP HAKCPointer, Instruction *I, SmallVector<Value *> &ArgsList) = 0;

        /**
         * Create the argument set for a HAKC Compartment transfer
         * @param HAKCPointer
         * @param Target
         * @param IsData
         * @param Size
         * @return
         */
        virtual void
        CreateTransferArguments(ManagedHAKCPointerP HAKCPointer, GlobalValue *Target, bool IsData, ConstantInt *Size,
                                SmallVector<Value *> &Result) = 0;


        /**
         * Create a normal HAKC Compartment transfer for objects that do not have a custom transfer function
         * @param HAKCPointer
         * @param Target
         * @param IsData
         * @param Size
         * @return
         */
        virtual Instruction *CreateDefaultTransfer(hakc::ManagedHAKCPointerP HAKCPointer,
                                                   GlobalValue *Target,
                                                   bool IsData,
                                                   ConstantInt *Size);

        /**
         *
         * @param HAKCPointer
         * @param Target
         * @param IsData
         * @param Size
         * @return
         */
        virtual Instruction *CreateCustomTransfer(hakc::ManagedHAKCPointerP HAKCPointer,
                                                  GlobalValue *Target,
                                                  bool IsData,
                                                  ConstantInt *Size);

        bool HAKCPointerHasCustomTransfer(ManagedHAKCPointerP HAKCPointer);

        Value *CreatePointerCast(ManagedHAKCPointerP HAKCPointer, PointerType *PointerTy);

        Value *CreateReturnCast(ManagedHAKCPointerP HAKCPointer, Value *V);

        /**
         * Return the custom transfer function if one exists
         * @param HAKCPointer
         * @return
         */
        virtual std::shared_ptr<HAKCCustomTransfer> GetCustomTransferFunction(ManagedHAKCPointerP HAKCPointer);

        virtual void ValidateLocation(Instruction *I);

        virtual void ValidateHAKCPointer(ManagedHAKCPointerP HAKCPointer);

        virtual hakc_compartment_id_t getSymbolCompartmentID(GlobalValue *GV);

        virtual Function *CreateNonVariadicTransferFunction(Function *F);

        virtual Function *PopulateTransferFunction(Function *Target, Function *TransferFunction);

        virtual Function *GetTransferFunction(Function *F);

        virtual void
        CreateForwardArgumentTransfers(Function *Target, Function *TransferFunction, SmallVector<Value *> &ArgsList);

        virtual void CreateBackwardArgumentTransfers(Function *Target, Function *TransferFunction);

        virtual bool TargetIsKernel(GlobalValue *Target);

        virtual void
        TransferStructMembers(ConstantStruct *ConstStruct, Function *GlobalTransfer, GlobalValue *GlobalVar,
                              bool Debug);

        virtual bool TransferShouldBeCreated(Value *V, GlobalValue *Target);

        virtual bool DebugIsActive();

        virtual HAKCTypeP FindEntryBitcast(ManagedHAKCPointerP HAKCPointerP, Instruction *I, Function *Target);

        virtual std::shared_ptr<hakc::HAKCCustomTransfer> GetCustomTransferFunctionForType(HAKCTypeP HAKCType);

        virtual Instruction *
        CreateVoidCastCompartmentTransfer(ManagedHAKCPointerP HAKCPointer, Instruction *I, GlobalValue *Target,
                                          HAKCTypeP TypeToUse);

        virtual bool NoKernelTransfers(Function *Target);

        void InitNewFunction(Function *F, StringRef EntryBlockName);

        ManagedHAKCPointerP CreateNewManagedPointer(Value *BaseDefinition);
    };
}

#endif //HAKC_HAKCTRANSFORMER_H
