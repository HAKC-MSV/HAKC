//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCTRANSFORMER_H
#define HAKC_HAKCTRANSFORMER_H

#include <map>

#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"

#include "HAKCSystemInformation.h"
#include "HAKCTypeIdentifier/HAKCTypeIdentifier.h"
#include "HAKCFunctionDefinition/HAKCCustomTransfer.h"

using namespace llvm;

namespace hakc {

    class HAKCModuleAnalysis;

    class HAKCTypeIdentifier;

    /**
     * A virtual class that defines the API for creating HAKC transformations.
     * Must be subclassed to provide architecture specific functionality.
     */
    class HAKCTransformer {
    public:
        HAKCTransformer(Module &Module, HAKCModuleAnalysis *HAKCAnalysis);

        virtual ~HAKCTransformer() = default;

        /**
         * Create a pointer suitable for dereferencing
         * @param HAKCPointer
         * @param I
         * @return The last Instruction created, placed immediately prior to I
         */
        virtual Value *CreateSafePointer(Value *HAKCPointer, Instruction *I);

        /**
         * Create a HAKC Pointer check at I
         * @param HAKCPointer
         * @param I
         * @return
         */
        virtual Value *CreateDataAuthentication(Value *HAKCPointer, Instruction *I);

        /**
         * Create a HAKC Code Pointer check at I
         * @param HAKCPointer
         * @param I
         * @return
         */
        virtual Value *CreateCodeAuthentication(Value *HAKCPointer, Instruction *I);

        /**
         * Computes the size of the transfer and then calls CreateSizedCompartmentTransfer
         * @param HAKCPointer
         * @param I
         * @param Target
         * @param IsData
         * @return
         */
        virtual Instruction *CreateCompartmentTransfer(Value *HAKCPointer,
                                                       Instruction *I,
                                                       Function *Target,
                                                       bool IsData);

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
        virtual Instruction *CreateSizedCompartmentTransfer(Value *HAKCPointer,
                                                            Instruction *I,
                                                            Function *Target,
                                                            bool IsData,
                                                            ConstantInt *Size);

        /**
         * Creates a BitCastInst of Operand to TargetType at I
         * @param Operand
         * @param TargetType
         * @param I
         * @return
         */
        virtual Value *CreateBitCast(Value *Operand, Type *TargetType, Instruction *I);


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

        virtual bool FunctionIsExported(Function *F);

        virtual Module &getModule();

        virtual HAKCSystemInformation &getSystemInformation();

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
        virtual bool ValidateHAKCIntegerPointerSize(Value *HAKCPointer);

        virtual unsigned GetPointerAddrSpace(Value *V);

        /**
         * Creates metadata associated with a Compartment for proper loading by the kernel
         * @param CompartmentID
         * @return
         */
        virtual GlobalVariable *AddCompartmentMetadataEntry(hakc_compartment_id_t CompartmentID);

    protected:
        IRBuilder<> HAKCIRBuilder;
        HAKCDebugInfoProcessor DebugInfoProcessor;
        HAKCSystemInformation SystemInformation;
        HAKCModuleAnalysis *HAKCAnalysis;
        std::map<Function *, Function *> VariadicTransferFunctions;

        /**
         * Checks that ManagedHAKCPointer and I are valid, and sets the HAKCIRBuilder location to I
         * @param HAKCPointer
         * @param I
         */
        void ValidateHAKCPointerAndLocation(Value *HAKCPointer, Instruction *I);

        /**
         * Performs the transformations needed for creating a safe pointer
         * @param HAKCPointer
         * @param I
         * @return
         */
        virtual Value *CreateSafePointer_Arch(Value *HAKCPointer, Instruction *I) = 0;

        /**
         * Creates a Call to the specified function
         * @param name
         * @param RetTy
         * @param Args
         * @return
         */
        virtual CallInst *CreateCall(StringRef name, Type *RetTy, ArrayRef<Value *> Args);

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
        virtual Constant *GetEntryToken(hakc_compartment_id_t CompartmentID) = 0;

        virtual ConstantInt *GetObjectSizeInBytes(Value *V);

        virtual FunctionType *GetHAKCDataAuthenticationFunctionType(unsigned AddrSpace) = 0;

        /**
         * Create the argument set for a HAKC data check
         * @param HAKCPointer
         * @param I
         * @return
         */
        virtual std::vector<Value *> CreateDataAuthArguments(Value *HAKCPointer, Instruction *I) = 0;

        /**
         * Create the argument set for a HAKC code check
         * @param HAKCPointer
         * @param I
         * @return
         */
        virtual std::vector<Value *> CreateCodeAuthArguments(Value *HAKCPointer, Instruction *I) = 0;

        /**
         * Create the argument set for a HAKC Compartment transfer
         * @param HAKCPointer
         * @param Target
         * @param IsData
         * @param Size
         * @return
         */
        virtual std::vector<Value *> CreateTransferArguments(Value *HAKCPointer, Function *Target, bool IsData,
                                                             ConstantInt *Size) = 0;


        /**
         * Create a normal HAKC Compartment transfer for objects that do not have a custom transfer function
         * @param HAKCPointer
         * @param Target
         * @param IsData
         * @param Size
         * @return
         */
        virtual Instruction *CreateDefaultTransfer(Value *HAKCPointer,
                                                   Function *Target,
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
        virtual Instruction *CreateCustomTransfer(Value *HAKCPointer,
                                                  Function *Target,
                                                  bool IsData,
                                                  ConstantInt *Size);

        bool HAKCPointerHasCustomTransfer(Value *HAKCPointer);

        /**
         * Return the custom transfer function if one exists
         * @param HAKCPointer
         * @return
         */
        virtual std::shared_ptr<HAKCCustomTransfer> GetCustomTransferFunction(Value *HAKCPointer);

        virtual void ValidateLocation(Instruction *I);

        virtual void ValidateHAKCPointer(Value *HAKCPointer);

        virtual hakc_compartment_id_t getSymbolCompartmentID(GlobalValue *GV);

        virtual Function *CreateNonVariadicTransferFunction(Function *F);

        virtual Function *PopulateTransferFunction(Function *Target, Function *TransferFunction);

        virtual Function *GetTransferFunction(Function *F);

        virtual std::vector<Value *> CreateForwardArgumentTransfers(Function *Target, Function *TransferFunction);

        virtual void CreateBackwardArgumentTransfers(Function *Target, Function *TransferFunction);

        virtual bool TargetIsKernel(Function *Target);

        virtual bool DebugIsActive();

        virtual Type *FindEntryBitcast(Value *V, Instruction *I, Function *Target);

        virtual std::shared_ptr<hakc::HAKCCustomTransfer> GetCustomTransferFunctionForType(Type *HAKCType);

        virtual Instruction *
        CreateVoidCastCompartmentTransfer(Value *HAKCPointer, Instruction *I, Function *Target, Type *TypeToUse);

        virtual bool NoKernelTransfers(Function *Target);
    };
}

#endif //HAKC_HAKCTRANSFORMER_H
