//
// Created by derrick on 8/20/21.
//

#ifndef PMC_HAKC_DEFS_H
#define PMC_HAKC_DEFS_H

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"

/* Macro value defined in CheriBSD sys/module.h */
#define HAKC_CHERIBSD_COMPARTMENT_METADATA_TYPE 5
#define HACK_CHERIBSD_DEFAULT_VERSION 1

#define HAKC_CONTEXT_COMPARTMENT_SHIFT 16

#define DIVISION_ID_BIT_LENGTH 32
#define COMPARTMENT_ID_BIT_LENGTH 32

constexpr size_t BITS_PER_BYTE = 8;

using namespace llvm;

namespace hakc {

    typedef uint64_t hakc_compartment_id_t;
    typedef uint64_t hakc_access_token_t;
    typedef uint64_t hakc_compartment_division_t;

    typedef ConstantInt *HAKC_Compartment_ID;
    typedef ConstantInt *HAKC_Access_Token;
    typedef ConstantInt *HAKC_Division_ID;

    const StringRef OUTSIDE_TRANSFER_PREFIX = "HAKC_XFER_";
    const StringRef ORIGINAL_FUNCTION_PREFIX = "HAKC_ORIG_";
    const StringRef VARIADIC_TRANSFER_PREFIX = "HAKC_VARF_";
    const StringRef CAPABILITY_REASSIGNMENT_PREFIX = "_hakc_reassignment_";
    const StringRef MODPARAM_GETCTX_PREFIX = "hakc_modparam_getctx_";

    const uint64_t user_space_end = 0x0000ffffffffffff;

    const StringRef HAKC_SECTION_PREFIX = ".hakc.";
    const StringRef HAKC_MODPARAM_TEXT_SECTION = ".hakc.modparam_ctx.text";
    const StringRef HAKC_MODPARAM_FUNCP_SECTION = ".hakc.modparam_ctx_fp";

    /* Environment Variables */
    // const StringRef COMPARTMENT_PATH_ENV_VAR = "HAKC_COMPARTMENT_PATH";
    // const StringRef HAKC_DEBUG_ENV_VAR = "HAKC_DEBUG_NAME";
    // const StringRef HAKC_DB_PATH_ENV_VAR = "HAKC_DB_PATH";
    // const StringRef DAG_ANALYSIS_ROOT_ENV_VAR = "HAKC_DAG_ANALYSIS_ROOT";
    // const StringRef HAKC_ENV_VAR = "HAKC_ANALYSIS";
    // const StringRef HAKC_NO_KERNEL_TRANSFERS = "HAKC_NO_KERNEL_TRANSFERS";
    // const StringRef HAKC_MORELLO_HYBRID_ENV_VAR = "HAKC_MORELLO_HYBRID";
    // const StringRef HAKC_SOURCE_PATH = "HAKC_SOURCE_PATH";
    // const StringRef HAKC_BUILD_PATH = "HAKC_BUILD_PATH";

    const StringRef HAKC_SOURCE_PATH_REPLACEMENT = "_HAKC_SOURCE_PATH_";
    const StringRef HAKC_BUILD_PATH_REPLACEMENT = "_HAKC_BUILD_PATH_";

    typedef enum {
        SILVER_CLIQUE,
        GREEN_CLIQUE,
        RED_CLIQUE,
        ORANGE_CLIQUE,
        YELLOW_CLIQUE,
        PURPLE_CLIQUE,
        BLUE_CLIQUE,
        GREY_CLIQUE,
        PINK_CLIQUE,
        BROWN_CLIQUE,
        WHITE_CLIQUE,
        BLACK_CLIQUE,
        TEAL_CLIQUE,
        VIOLET_CLIQUE,
        CRIMSON_CLIQUE,
        GOLD_CLIQUE,
        NO_CLIQUE
    } sym_color_t;

    typedef enum {
        hakc_global_scope,
        hakc_local_scope
    } hakc_scope_t;

    const hakc_compartment_id_t KERNEL_COMPARTMENT = 0;
    const hakc_compartment_division_t KERNEL_DIVISION = NO_CLIQUE;
    const hakc_access_token_t KERNEL_ACCESS_TOKEN = 0;
}

#endif//PMC_HAKC_DEFS_H
