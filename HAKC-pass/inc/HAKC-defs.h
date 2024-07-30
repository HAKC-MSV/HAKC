//
// Created by derrick on 8/20/21.
//

#ifndef PMC_HAKC_DEFS_H
#define PMC_HAKC_DEFS_H

#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"

#include <set>
#include <vector>

/* Macro value defined in CheriBSD sys/module.h */
#define HAKC_CHERIBSD_COMPARTMENT_METADATA_TYPE 5
#define HACK_CHERIBSD_DEFAULT_VERSION           1

#define HAKC_CONTEXT_COMPARTMENT_SHIFT  16

#define CLIQUE_COLOR_BIT_LENGTH     32
#define COMPARTMENT_ID_BIT_LENGTH   32

#define BITS_PER_BYTE               8

using namespace llvm;

namespace hakc {

    typedef int64_t hakc_compartment_id_t;
    typedef int64_t hakc_access_token_t;

    typedef ConstantInt* HAKCCompartment;

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
    const StringRef COMPARTMENT_PATH_ENV_VAR = "HAKC_COMPARTMENT_PATH";
    const StringRef HAKC_DEBUG_ENV_VAR = "HAKC_DEBUG_NAME";
    const StringRef DAG_ANALYSIS_ROOT_ENV_VAR = "HAKC_DAG_ANALYSIS_ROOT";
    const StringRef HAKC_ENV_VAR = "HAKC_ANALYSIS";
    const StringRef HAKC_NO_KERNEL_TRANSFERS = "HAKC_NO_KERNEL_TRANSFERS";
    const StringRef HAKC_MORELLO_HYBRID_ENV_VAR = "HAKC_MORELLO_HYBRID";
    const StringRef HAKC_SOURCE_PATH = "HAKC_SOURCE_PATH";
    const StringRef HAKC_BUILD_PATH = "HAKC_BUILD_PATH";

    const StringRef HAKC_SOURCE_PATH_REPLACEMENT = "$HAKC_SOURCE_PATH$";
    const StringRef HAKC_BUILD_PATH_REPLACEMENT = "$HAKC_BUILD_PATH$";

    typedef enum {
        SILVER_CLIQUE = 0xF0,
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

    const unsigned KERNEL_COMPARTMENT = 0;
    const sym_color_t KERNEL_COLOR = NO_CLIQUE;

}

#endif//PMC_HAKC_DEFS_H
