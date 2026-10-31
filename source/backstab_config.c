/* backstab_config.c -- config.ini for the Backstab port. MIT. */
#include <switch.h>

#include "rt_cfg.h"
#include "util.h"

void dcr_config_load(void);

static const CfgOpt k_opts[] = {
    CFG_ROW_RESOLUTION("auto", CFG_HELP_RESOLUTION),
    CFG_ROW_BOOST(CFG_HELP_BOOST, NULL),
    CFG_ROW_GL_SELFTEST(NULL),
    CFG_ROW_BOOT_LOG(CFG_HELP_BOOT_LOG, NULL),
    CFG_ROW_LOG_JNI(CFG_HELP_LOG_JNI, NULL),
    CFG_ROW_SWAP_AB("Swap the A and B buttons.", NULL),
};

static void apply(void) {
  const RtConfig *rt = rt_config();
  debugPrintf("[config] %dx%d (%s), CPU boost %s, JNI log %s\n", rt->res_w, rt->res_h,
              rt_config_get("display", "resolution"), rt->boost ? "on" : "off", rt->log_jni ? "on" : "off");
}

static const CfgTable k_table = {
    .opts = k_opts,
    .nopts = CFG_COUNT(k_opts),
    .version = 1,
    .apply = apply,
};

void dcr_config_load(void) { rt_config_load(&k_table); }
