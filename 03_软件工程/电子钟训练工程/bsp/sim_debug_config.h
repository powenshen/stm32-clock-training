#ifndef SIM_DEBUG_CONFIG_H
#define SIM_DEBUG_CONFIG_H

/*
 * Enable this switch for Keil simulator / non-hardware debugging.
 * It can also be overridden from the compiler command line.
 */
#ifndef APP_CLOCK_SIM_ENABLED
#define APP_CLOCK_SIM_ENABLED    1U
#endif

/*
 * When the simulation entry is enabled, run the built-in regression script
 * automatically after boot so the user can inspect the report in the debugger.
 */
#ifndef APP_CLOCK_SIM_AUTO_RUN
#define APP_CLOCK_SIM_AUTO_RUN   1U
#endif

#endif
