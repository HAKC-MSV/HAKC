//
// Created by de29664 on 3/31/23.
//
#include "llvm/IR/Verifier.h"

#include "HAKCAnalysis/Linux/HAKCModuleAnalysisLinux.h"
#include "HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "HAKCFunctionDefinition/HAKCTransferFunction.h"
#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_sk_buff.h"
#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_file.h"
#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_socket.h"
#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_fuse_mount.h"

#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_scsi_cmnd.h"
#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_usb_device.h"
#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_usb_interface.h"
#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_us_data.h"

#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_scsi_device.h"
#include "HAKCTransformers/Linux/CustomTransfers/CustomTransfer_file_ops.h"

namespace hakc {
    HAKCModuleAnalysisLinux::HAKCModuleAnalysisLinux(Module &M) :
            HAKCModuleAnalysis(M) {
    }

    void HAKCModuleAnalysisLinux::InitHAKCFunctions() {
        HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_sk_buff, M,
                             CommonHAKCAnalysis::getCompartmentStorageSizeInBits()));
        HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_file, M,
                             CommonHAKCAnalysis::getCompartmentStorageSizeInBits()));
        HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_socket, M,
                             CommonHAKCAnalysis::getCompartmentStorageSizeInBits()));
        HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_fuse_mount, M,
                             CommonHAKCAnalysis::getCompartmentStorageSizeInBits()));

        HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_scsi_cmnd, M,
                             CommonHAKCAnalysis::getCompartmentStorageSizeInBits()));
        HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_usb_device, M,
                             CommonHAKCAnalysis::getCompartmentStorageSizeInBits()));
        HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_usb_interface, M,
                             CommonHAKCAnalysis::getCompartmentStorageSizeInBits()));
        HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_us_data, M,
                             CommonHAKCAnalysis::getCompartmentStorageSizeInBits()));

        HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_scsi_device, M,
                             CommonHAKCAnalysis::getCompartmentStorageSizeInBits()));

        HAKC_CUSTOM_TRANSFER(hakc::CustomTransfer_file_ops, M,
                             CommonHAKCAnalysis::getCompartmentStorageSizeInBits()));

        HAKC_TRANSFER(HAKCCompartmentTransferName(), 2, 3);
        HAKC_TRANSFER(HAKCPerCPUCompartmentTransferName(), 2, 3);
        HAKC_TRANSFER(HAKCSignWithDivisionName(), 1, -1);
        HAKC_TRANSFER("hakc_sign_pointer", 1, 2);

        /* TODO: Make these custom transfer functions */
        HAKC_FUNCTION("hakc_transfer_nla");
        HAKC_FUNCTION("hakc_transfer_string");

        /* I couldn't find these in the kernel source, but they were listed, so I am keeping them. */
        HAKC_FUNCTION("hakc_record_common");
        HAKC_FUNCTION("hakc_transfer_to_destination");
        HAKC_FUNCTION("hakc_restore_original");

        HAKC_FUNCTION("hakc_init_kernel_globals");
        HAKC_FUNCTION("hakc_init_globals");
    }

    HAKC_Division_ID
    HAKCModuleAnalysisLinux::getSymbolDivision(GlobalValue *GV, HAKCCompartmentalizationPolicy &Policy) {
        return Policy.GetDivisionID(GV);
    }

    bool HAKCModuleAnalysisLinux::FunctionIsExported(Function *F) {
        auto ksym_name = getKstrtab_entry_name(F);
        /* Add colon so __kstrtab_foo_1 doesn't match __kstrtab_foo */
        ksym_name += ":";
        return M.getModuleInlineAsm().find(ksym_name) != M.getModuleInlineAsm().npos;
    }

    std::string HAKCModuleAnalysisLinux::getKstrtab_entry_name(Function *F) {
        std::string ksymtab_symbol_name = "__kstrtab_";
        ksymtab_symbol_name += F->getName();
        return ksymtab_symbol_name;
    }

    std::string HAKCModuleAnalysisLinux::getKstrtabns_entry_name(Function *F) {
        std::string ksymtabns_symbol_name = "__kstrtabns_";
        ksymtabns_symbol_name += F->getName();
        return ksymtabns_symbol_name;
    }

    std::string
    HAKCModuleAnalysisLinux::getGlobalHAKCSectionName(GlobalVariable *GV, HAKCCompartmentalizationPolicy &Policy) {
        std::string sectionName = HAKC_SECTION_PREFIX.str();
        auto *symbolDivision = getSymbolDivision(GV, Policy);
        sectionName += HAKCModuleAnalysisLinux::getColorStringFromValue(symbolDivision);
        sectionName += GV->getSection().str();
        if (GV->getSection().empty()) {
            if (GV->isConstant()) {
                sectionName += ".rodata";
            } else {
                sectionName += ".data";
            }
        }
        return sectionName;
    }

    std::vector<StringRef> HAKCModuleAnalysisLinux::GetSafeTransitionFunctions_Arch() {
        return {
                "clear_page",
                "kmemdup",
                "__memcpy",
                "__pi_memcmp",
                "__pi_strcmp",
                "bcmp",
                "__arch_copy_from_user",
                "__memset",
                "memchr_inv",
                "__arch_copy_to_user",
                "xas_load",
                "xas_find_marked",
                "xa_head",
                "crc32_le",
                "__crc32c_le",
                "cpu_switch_to",
                /* all .S files in arch/arm64/lib */
                "__pi_clear_page",
                "clear_page",
                "__clear_page",

                "__pi_copy_page",
                "copy_page",
                "__copy_page",

                "__pi_memchr",
                "memchr",
                "__memchr",

                "__pi_memcpy",
                "memcpy",
                "__memcpy",

                "__pi_memmove",
                "memmove",
                "__memmove",

                "__pi_memset",
                "memset",
                "__memset",

                "__pi_strlen",
                "strlen",
                "__strlen",

                "__pi_strncmp",
                "strncmp",
                "__strncmp",

                "__pi_strnlen",
                "strnlen",
                "__strnlen",

                "__pi_strrchr",
                "strrchr",
                "__strrchr",

                "__arch_clear_user",

                "__ashlti3",

                "__ashrti3",

                "__lshrti3"
        };
    }

    // Get the StructType representing a kernel (module) parameter
    StructType *HAKCModuleAnalysisLinux::GetKernelParamType() {
        return llvm::StructType::getTypeByName(M.getContext(), llvm::StringRef("struct.kernel_param"));
    }

    void HAKCModuleAnalysisLinux::TransformModule(HAKCCompartmentalizationPolicy &Policy) {
        HAKCModuleAnalysis::TransformModule(Policy);
        TransferModuleParams(Policy);
    }

    GlobalValue *HAKCModuleAnalysisLinux::ExtractGlobalFromKernelParam(GlobalVariable *GV) {
        // the result of walking through the kernel param struct
        // until we get to the actual global value backing the parameter
        GlobalValue *kernparam;

        StructType *KernelParamType = GetKernelParamType();
        // type not found, just do nothing
        if (!KernelParamType) {
            return nullptr;
        }

        // trying to find globals of type GetKernelParamType()
        if (auto *F = dyn_cast<StructType>(GV->getValueType())) {
            if (!(F->getName().equals(KernelParamType->getName()))) {
                return nullptr; // someone passed us a struct that wasn't a kernel param struct
            }
        } else {
            return nullptr; // this is not good, don't give non-structs to this function
        }

        // we know it is a kernel param now, moving on

        // cast the value into a ConstantStruct so we can pick it apart
        auto *kp_struct = dyn_cast<ConstantStruct>(GV->getInitializer());

        // do we have struct kernel_param kp now
        if (kp_struct) {
            // the anonymous union that holds the Value we actually want
            // is the last element of the struct
            auto num_ops = kp_struct->getNumOperands();
            Constant *last_op = kp_struct->getOperand(num_ops - 1);

            // this holds kp->arg
            if (last_op) {
                // cast the union into a ConstantStruct so we can pick it apart
                auto *kparg_union = dyn_cast<ConstantStruct>(last_op);

                if (kparg_union) {
                    // get the only thing in the struct, that's how unions work?
                    // this constant is kp->arg, sort of
                    Constant *kparg_val = kparg_union->getOperand(0);
                    // check that the value in there is a BitCastOperator
                    // it is bit-casting the global that backs the parameter
                    if (auto *kparg_val_bco = dyn_cast<BitCastOperator>(kparg_val)) {
                        // extract the pointer from the BitCastOperator
                        Value *gv_from_bco = kparg_val_bco->getOperand(0);

                        if (!(kernparam = dyn_cast<GlobalValue>(gv_from_bco))) {
                            // if it isn't a global value, that's bad
                            return nullptr;
                        }

                        // now we have kp->arg
                    }
                        // the thing in the union isn't a BitCastOperator, that's bad
                    else {
                        return nullptr;
                    }
                }
                    // we couldn't get the union out of the union struct, that's bad
                else {
                    return nullptr;
                }
            }
                // we couldn't get the union struct at all out of the param struct, that's bad
            else {
                return nullptr;
            }
        }
            // we couldn't even get the kernel param struct as a struct, that's bad
        else {
            return nullptr;
        }

        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "processing kernel param\n";
            kernparam->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
        }

        return kernparam;
    }

    // we generate these for all kernel params, some may go unused by the actual module loader
    // (non-pointer params are ignored by the loader when it comes to transferring)
    void HAKCModuleAnalysisLinux::TransferModuleParams(HAKCCompartmentalizationPolicy &Policy) {
        StructType *KernelParamType = GetKernelParamType();
        // type not found, just do nothing
        if (!KernelParamType) {
            return;
        }

        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "kernel param type is: " << *KernelParamType << "\n";
        }

        std::vector<GlobalVariable *> GlobalList;
        for (auto &Global: M.getGlobalList()) {
            if (auto *StructTy = dyn_cast<StructType>(Global.getValueType())) {
                if (StructTy == KernelParamType) {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "found kernel param:" << Global << "\n";
                    }
                    GlobalList.push_back(&Global);
                }
            }
        }
        SortGlobalList(GlobalList);

        // inspect all globals
        for (auto *GlobalP: GlobalList) {
            // generate a GetCtx function for the parameter and update
            // function pointer array
            GenerateModuleParamGetCtxFunction(GlobalP, Policy);
        }
    }

    void HAKCModuleAnalysisLinux::emitModParamGetCtx(GlobalValue *kernparam, HAKCCompartmentalizationPolicy &Policy) {
        // type of void*
        PointerType *PointerTy = PointerType::get(IntegerType::get(M.getContext(), 8), 0);

        // two args
        std::vector<Type *> FuncTy_args;
        // first arg points to param
        FuncTy_args.push_back(PointerTy);
        // second arg is int64_t flag (0 to return param's access token, 1 to return param's color)
        FuncTy_args.push_back(IntegerType::get(M.getContext(), 64));

        // type of function that returns int64_t, takes (void *, int64_t)
        FunctionType *FuncTy = FunctionType::get(IntegerType::get(M.getContext(), 64),
                                                 FuncTy_args,
                                                 false);

        // create a function named "hakc_modparam_getctx_paramname"
        auto c = M.getOrInsertFunction(MODPARAM_GETCTX_PREFIX.str() + kernparam->getName().str(),
                                       FuncTy);

        auto *constc = dyn_cast<Constant>(c.getCallee());
        auto *getctx = cast<Function>(constc);
        getctx->setCallingConv(CallingConv::C);

        // put "hakc_modparam_getctx_paramname" in a special text section in the module
        getctx->setSection(HAKC_MODPARAM_TEXT_SECTION);

        Function::arg_iterator args = getctx->arg_begin();
        // param pointer
        Value *pointerArg = args++;
        // 0 to return context, 1 to return color
        Value *returnTypeArg = args++;

        // create entry basic block in our new function
        BasicBlock *block = BasicBlock::Create(M.getContext(), "entry", getctx);
        //
        IRBuilder<> builder(block);
        // constant zero for compare/select
        Value *czero = ConstantInt::get(IntegerType::get(M.getContext(), 64), 0);

        // find the color of the HAKC symbol
        auto Color = getSymbolDivision(kernparam, Policy);

        // find the compartment ID of the HAKC symbol
        auto CompartmentDivision = Policy.GetDivision(kernparam);

        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "color:\n" << getColorStringFromValue(Color) << "\n" << "compartment:\n"
                                            << std::to_string(
                                                    CompartmentDivision.GetHAKCCompartment().GetCompartmentIDValue())
                                            << "\n";
        }

        // cast kernparam to a void*
        Value *voidCast;
        auto AddrSpace = getTransformer(Policy).GetPointerAddrSpace(kernparam);

        if (kernparam->getType()->isIntegerTy()) {
            voidCast = builder.CreateIntToPtr(kernparam, builder.getInt8PtrTy(AddrSpace));
        } else {
            voidCast = builder.CreateBitCast(kernparam, builder.getInt8PtrTy(AddrSpace));
        }


        // if returnTypeArg == 0, next step will use access token for return value
        // else, use color for return value
        Value *tokEqZero = builder.CreateICmpEQ(returnTypeArg, czero);
        Value *tokColSelect = builder.CreateSelect(tokEqZero, CompartmentDivision.GetAccessToken(), Color);

        // check if the address passed in matches address of kernparam
        Value *pointerArgEq = builder.CreateICmpEQ(pointerArg, voidCast);
        // if it does, return the previously selected token/color
        // it it isn't a match, return zero
        Value *ctxSelect = builder.CreateSelect(pointerArgEq, tokColSelect, czero);
        // function is done
        builder.CreateRet(ctxSelect);

        if (debug_output) {
            getctx->print(CommonHAKCAnalysis::getWriter());
            CommonHAKCAnalysis::getWriter() << "\n";
            CommonHAKCAnalysis::getWriter() << llvm::verifyFunction(*getctx, &CommonHAKCAnalysis::getWriter()) << "\n";
        }

        // generate function pointer and place in modparam fp section
        auto *gcfp = dyn_cast<GlobalVariable>(M.getOrInsertGlobal(getctx->getName().str() + "_fp", getctx->getType()));
        gcfp->setConstant(true);
        gcfp->setLinkage(GlobalValue::ExternalLinkage);
        gcfp->setInitializer(getctx);
        gcfp->setSection(HAKC_MODPARAM_FUNCP_SECTION);
    }

    // takes a KernelParam and generate a function to get the HAKC signing context
    // for the actual backing global variable
    // used to correctly transfer charp parameters
    void HAKCModuleAnalysisLinux::GenerateModuleParamGetCtxFunction(GlobalVariable *GV,
                                                                    HAKCCompartmentalizationPolicy &Policy) {
        GlobalValue *kernparam = ExtractGlobalFromKernelParam(GV);

        if (!kernparam) {
            CommonHAKCAnalysis::getWriter() << "Could not extract global from kernel param:\n" << *GV << "\n";
            throw std::exception();
        }

        emitModParamGetCtx(kernparam, Policy);
    }

    std::set<StringRef> HAKCModuleAnalysisLinux::GetSeparateNamespacePaths() {
        return {};
    }

    std::set<StringRef> HAKCModuleAnalysisLinux::GetHAKCSourcePaths() {
        return {
                /* legacy + current path for common HAKC code */
                "kernel/hakc/hakc_common.c",
                "kernel/hakc/hakc_init_tag.c",
                /* current paths for architecture-independent HAKC */
                "kernel/hakc/hakc_noarch_tag_btree.c",
                "kernel/hakc/hakc_noarch_tag_memory.c",
                /* transfer function sources that aren't in hakc_common.c */
                "fs/fuse/transfer.c",
                "drivers/usb/storage/transfer.c",
                "kernel/hakc/hakc_global_init.c",
        };
    }

    std::map<StringRef, hakc_allocation_size_map_t> HAKCModuleAnalysisLinux::GetKernelAllocationSizeMap() {
        return {
                {"kmalloc",                simpleArgumentSize<0>},
                {"kzalloc",                simpleArgumentSize<0>},
                {"__kmalloc",              simpleArgumentSize<0>},
                {"neigh_parms_alloc",      simpleStaticSize<144>},
                {"nlmsg_new",              staticPlusArgument<64, 0>},
                {"kmemdup",                simpleArgumentSize<1>},
                {"__alloc_percpu",         simpleArgumentSize<0>},
                {"__alloc_percpu_gfp",     simpleArgumentSize<0>},
                {"kmalloc_array",          multiplyTwoArguments<0, 1>},
                {"kcalloc",                multiplyTwoArguments<0, 1>},
                {"sk_alloc",               simpleStaticSize<64>}, /* NB: This is likely not correct, but socks are allocated weirdly, and hopefully this is fine */
                {"__alloc_skb",            simpleArgumentSize<0>},
                {"kmem_cache_zalloc",      simpleArgumentSize<0>},
                {"nla_memdup",             simpleStaticSize<64>}, /* NB: same with sk_alloc */
                {"kzalloc_node",           simpleArgumentSize<0>},
                {"fib_rules_register",     simpleStaticSize<256>}, /* TODO: get the correct number */
                {"nla_strdup",             simpleStaticSize<64>}, /* NB: shares same size of nla_memdup */
                {"kvmalloc",               simpleArgumentSize<0>},
                {"kvmalloc_array",         multiplyTwoArguments<0, 1>},
                {"kvzalloc",               simpleArgumentSize<0>},
                {"kmem_cache_alloc",       argumentGEP<0, 1>}, /* TODO: Get GEP indices */
                {"kmem_cache_alloc_trace", simpleArgumentSize<2>},
                {"create_workqueue",       simpleStaticSize<320>},
                {"device_create",          simpleStaticSize<6144 / 8>},
                {"__class_create",         simpleStaticSize<960 / 8>},
                {"__vmalloc",              simpleArgumentSize<0>},
                {"alloc_chrdev_region",    CallArgumentSize<0>},
        };
    }

    std::set<StringRef> HAKCModuleAnalysisLinux::GetIgnoredGlobals() {
        std::set<StringRef> GlobalsToIgnore = {
                "kmalloc_caches",
                "current_task",
        };
        auto ExistingIgnoredGlobals = CommonHAKCAnalysis::GetIgnoredGlobals();
        return AddToSet(GlobalsToIgnore, ExistingIgnoredGlobals);
    }

    bool HAKCModuleAnalysisLinux::valueIsReadonlyPtr(Value *value) {
        bool result = CommonHAKCAnalysis::valueIsReadonlyPtr(value);
        if (!result) {
            if (auto *callInst = dyn_cast<CallInst>(value)) {
                /* We may have done some global transfer beforehand, so check for that */
                if (callInst->getCalledFunction() &&
                    callInst->getCalledFunction()->getName() == "hakc_sign_pointer_with_color") {
                    auto *isCode = dyn_cast<ConstantInt>(
                            callInst->getArgOperand(callInst->getNumArgOperands() - 1));
                    result = isCode->isOne();
                }
            }
        }

        return result;
    }

    std::set<StringRef> HAKCModuleAnalysisLinux::GetIgnoredTypes() {
        return {
                /* Types that are not handled for now
                 * TODO: Go through each type and figure out a solution
                 */
                "struct.list_head",     /* Lists are statically defined in code to point to themselves, and so
                                        * currently the next members cannot be signed before start */

                "struct.proto",         /* Static member assignments cannot be signed */

                "struct.atomic64_s",

                "struct.atomic_s",

                "struct.atomic64_t",

//                "struct.kmem_cache",

//                "struct.page",
        };
    }

    std::set<StringRef> HAKCModuleAnalysisLinux::GetNoTransferFunctions() {
        return {
                "__bpf_call_base",
                "ftrace_stub",
                "ftrace_stub_graph",
                "ftrace_call",
                "parse_memtest",
                "__stack_chk_fail",
// some of these might be macros
                "is_vmalloc_addr",
                "page_to_phys",
                "vmalloc_to_page",
                "virt_to_pfn",
/*                "init_module",
                "cleanup_module",*/
                /* The following are generated functions, and I don't want to figure
                 * out how they work */
                "raid6_int4_xor_syndrome",
                "raid6_int1_gen_syndrome",
                "raid6_int2_xor_syndrome",
                "raid6_int8_gen_syndrome",
                "raid6_int4_gen_syndrome",
                "raid6_int2_gen_syndrome",
                "raid6_int8_xor_syndrome",
                "raid6_int1_xor_syndrome",
        };
    }

    std::string HAKCModuleAnalysisLinux::getColorStringFromValue(HAKC_Division_ID color) {
        switch (color->getZExtValue()) {
            case SILVER_CLIQUE:
                return "SILVER_CLIQUE";
            case GREEN_CLIQUE:
                return "GREEN_CLIQUE";
            case RED_CLIQUE:
                return "RED_CLIQUE";
            case ORANGE_CLIQUE:
                return "ORANGE_CLIQUE";
            case YELLOW_CLIQUE:
                return "YELLOW_CLIQUE";
            case PURPLE_CLIQUE:
                return "PURPLE_CLIQUE";
            case BLUE_CLIQUE:
                return "BLUE_CLIQUE";
            case GREY_CLIQUE:
                return "GREY_CLIQUE";
            case PINK_CLIQUE:
                return "PINK_CLIQUE";
            case BROWN_CLIQUE:
                return "BROWN_CLIQUE";
            case WHITE_CLIQUE:
                return "WHITE_CLIQUE";
            case BLACK_CLIQUE:
                return "BLACK_CLIQUE";
            case TEAL_CLIQUE:
                return "TEAL_CLIQUE";
            case VIOLET_CLIQUE:
                return "VIOLET_CLIQUE";
            case CRIMSON_CLIQUE:
                return "CRIMSON_CLIQUE";
            case GOLD_CLIQUE:
                return "GOLD_CLIQUE";
            case NO_CLIQUE:
                return "NO_CLIQUE";
            default:
                CommonHAKCAnalysis::getWriter() << "number " << color->getZExtValue() << "isn't a valid color\n";
                return "INVALID_CLIQUE";
        }
    }

    bool HAKCModuleAnalysisLinux::FunctionNeedsAnalysis(Function *F) {
        if (F->getName().contains("static_branch_")) {
            /* These functions call inline assembly that needs to be
             * constant at compile time, so we can't analyze them.
             * We ensure that any pointer passed to these functions have
             * no signature in argNeedsAnalysis.
             */
            if (debug_output) {
                CommonHAKCAnalysis::getWriter() << F->getName() << " does not need analysis\n";
            }
            return false;
        }
        return HAKCModuleAnalysis::FunctionNeedsAnalysis(F);
    }
} // hakc
