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
     * A  class that defines the API for creating HAKC transformations.
     * Must be subclassed to provide architecture specific functionality.
     */
    class HAKCTransformer {
    public:
        HAKCTransformer(Module &Module, HAKCModuleAnalysis *HAKCAnalysis);

         ~HAKCTransformer() = default;

        /**
         * Create a pointer suitable for dereferencing
         * @param HAKCPointer
         * @param I
         * @return The last Instruction created, placed immediately prior to I
         */
         Value *CreateSafePointer(Value *HAKCPointer, Instruction *I);

        /**
         * Create a HAKC Pointer check at I
         * @param HAKCPointer
         * @param I
         * @return
         */
         Value *CreateDataAuthentication(Value *HAKCPointer, Instruction *I);

        /**
         * Create a HAKC Code Pointer check at I
         * @param HAKCPointer
         * @param I
         * @return
         */
         Value *CreateCodeAuthentication(Value *HAKCPointer, Instruction *I);

        /**
         * Computes the size of the transfer and then calls CreateSizedCompartmentTransfer
         * @param HAKCPointer
         * @param I
         * @param Target
         * @param IsData
         * @return
         */
         Instruction *CreateCompartmentTransfer(Value *HAKCPointer,
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
         Instruction *CreateSizedCompartmentTransfer(Value *HAKCPointer,
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
         Value *CreateBitCast(Value *Operand, Type *TargetType, Instruction *I);


        /**
         * Create a Outside Transfer Function
         * @param F
         * @return
         */
         Function *CreateTransferFunction(Function *F);

        /**
         * Create a transfer function to a variadic function in a different compartment
         * @param Call
         * @return
         */
         Function *CreateTransferToVariadic(CallInst *Call);

        /**
         * Create Architecture specific transformations for a new transfer function
         * @param Original
         * @param Transfer
         */
         void CreateTransferFunctionFinalize_Arch(Function *Original, Function *Transfer);

        /**
         * Perform architecture specific transformations prior to an argument transfer to a target compartment
         * @param F
         * @param TransferFunction
         * @param Arg
         */
         void CreateTransferFunctionArg_PreCall(Function *F, Function *TransferFunction, Value *Arg);

        /**
         * Perform architecture specific transformations after the cross compartment function call
         * @param F
         * @param TransformFunction
         * @param Arg
         */
         void CreateTransferFunctionArg_PostCall(Function *F, Function *TransformFunction, Value *Arg);

         bool FunctionIsExported(Function *F);

         Module &getModule();

         HAKCSystemInformation &getSystemInformation();

         Type *HAKCAuthenticationRetType(unsigned AddrSpace);

         hakc_compartment_id_t getFunctionCompartmentID(Function *F);

         hakc_compartment_id_t getGlobalCompartmentID(GlobalVariable *GV);

         ConstantInt *GetHAKCCompartmentValue(hakc_compartment_id_t CompartmentID);

         ConstantInt *getTrue();

         ConstantInt *getFalse();

         ConstantInt *getInt64(int64_t Value);

         ConstantInt *getInt32(int32_t Value);

         ConstantInt *GetDefaultObjectSize();

        /**
        * Returns true if ManagedHAKCPointer is an appropriately sized integer for use as a pointer
        * @param HAKCPointer
        */
         bool ValidateHAKCIntegerPointerSize(Value *HAKCPointer);

         unsigned GetPointerAddrSpace(Value *V);

        /**
         * Creates metadata associated with a Compartment for proper loading by the kernel
         * @param CompartmentID
         * @return
         */
         GlobalVariable *AddCompartmentMetadataEntry(hakc_compartment_id_t CompartmentID);

    protected:
        IRBuilder<> HAKCIRBuilder;
        HAKCDebugInfoProcessor DebugInfoProcessor;
        HAKCSystemInformation SystemInformation;
        HAKCModuleAnalysis *HAKCAnalysis;
        std::map<Function *, Function *> VariadicTransferFunctions;

    StructType *EntryTokenType;

        CallInst *SaveColor(Value *V);

        const StringRef HAKCGetColorName();

        const StringRef HAKCGetPerCPUColorName();

        const StringRef HAKCColorAddressName();

        std::string getUniqueAddressable_Name(Function *F);

        std::string getKstrtab_entry_name(Function *F);

        std::string getKstrtabns_entry_name(Function *F);

        ConstantInt *GetColorValue(sym_color_t Color);

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
         Value *CreateSafePointer_Arch(Value *HAKCPointer, Instruction *I);

        /**
         * Creates a Call to the specified function
         * @param name
         * @param RetTy
         * @param Args
         * @return
         */
         CallInst *CreateCall(StringRef name, Type *RetTy, ArrayRef<Value *> Args);

        /**
         * Gets or inserts the GlobalVariable containing the list of valid targets from the Compartment F belongs to
         * @param F
         * @return
         */
        GlobalVariable *GetValidTargetCompartments(Function *F);

        /**
         * Return the type that HAKC Compartment Entry Tokens are in the source
         * @return
         */
        Type *GetEntryTokenType(unsigned AddrSpace);

        /**
         * Returns the Entry Token for the given CompartmentID and Value
         * @return
         */
         Constant *GetEntryToken(hakc_compartment_id_t CompartmentID);

         ConstantInt *GetObjectSizeInBytes(Value *V);

         FunctionType *GetHAKCDataAuthenticationFunctionType(unsigned AddrSpace);

        /**
         * Create the argument set for a HAKC data check
         * @param HAKCPointer
         * @param I
         * @return
         */
         std::vector<Value *> CreateDataAuthArguments(Value *HAKCPointer, Instruction *I);

        /**
         * Create the argument set for a HAKC code check
         * @param HAKCPointer
         * @param I
         * @return
         */
         std::vector<Value *> CreateCodeAuthArguments(Value *HAKCPointer, Instruction *I);

        /**
         * Create the argument set for a HAKC Compartment transfer
         * @param HAKCPointer
         * @param Target
         * @param IsData
         * @param Size
         * @return
         */
         std::vector<Value *> CreateTransferArguments(Value *HAKCPointer, Function *Target, bool IsData,
                                                             ConstantInt *Size);


        /**
         * Create a normal HAKC Compartment transfer for objects that do not have a custom transfer function
         * @param HAKCPointer
         * @param Target
         * @param IsData
         * @param Size
         * @return
         */
         Instruction *CreateDefaultTransfer(Value *HAKCPointer,
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
         Instruction *CreateCustomTransfer(Value *HAKCPointer,
                                                  Function *Target,
                                                  bool IsData,
                                                  ConstantInt *Size);

        bool HAKCPointerHasCustomTransfer(Value *HAKCPointer);

        /**
         * Return the custom transfer function if one exists
         * @param HAKCPointer
         * @return
         */
         std::shared_ptr<HAKCCustomTransfer> GetCustomTransferFunction(Value *HAKCPointer);

         void ValidateLocation(Instruction *I);

         void ValidateHAKCPointer(Value *HAKCPointer);

         hakc_compartment_id_t getSymbolCompartmentID(GlobalValue *GV);

         Function *CreateNonVariadicTransferFunction(Function *F);

         Function *PopulateTransferFunction(Function *Target, Function *TransferFunction);

         Function *GetTransferFunction(Function *F);

         std::vector<Value *> CreateForwardArgumentTransfers(Function *Target, Function *TransferFunction);

         void CreateBackwardArgumentTransfers(Function *Target, Function *TransferFunction);

         bool TargetIsKernel(Function *Target);

         bool DebugIsActive();

         Type *FindEntryBitcast(Value *V, Instruction *I, Function *Target);

         std::shared_ptr<hakc::HAKCCustomTransfer> GetCustomTransferFunctionForType(Type *HAKCType);

         Instruction *CreateVoidCastCompartmentTransfer(Value *HAKCPointer, Instruction *I, Function *Target, Type *TypeToUse);

         bool NoKernelTransfers(Function *Target);
    private:
        std::map<Value *, CallInst *> TransferArgumentsToRestore;
    };
}

#endif //HAKC_HAKCTRANSFORMER_H
