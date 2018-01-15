#target/appl/fdev/src

SOURCE:= appl_if.c appl_res.c appl_dia.c appl_nrv.c appl_dlh.c \
    appl_svi_swdl.c mnhart2ff.c mncharactff.c mnctllimff.c lui_ipc_variables.c \
    histogram.c ctrl_param_set.c ui_language.c app_mode.c mndoswff.c \
    pressure_range.c mnptbexec.c appl_alert.c mnfaultsimport.c \
    mnptberrors.c mn_cycle_counter_alert.c mn_travel_accumulation.c \
    mn_trends.c eep_blk2.c mn_clear_status.c mn_actuator_3.c \
    mn_near_closed_alert.c mnfindstopsff.c mn_travel_units.c \
    mn_working_times.c mn_grant_deny.c mndiagprocff.c mntbmspff.c mn_fstate.c \
    mn_xd_errors.c mn_errorbits.c mn_ui_access_control.c mnipcff.c \
    mn_supply_pressure_alert.c mn_pressure_extremes.c \
    mn_ip_drive_current_alert.c mn_factory_defaults.c appl_ptb.c ffactoryuse.c \
    mn_temperature_alert.c mn_remote_sensor.c mn_checkproc.c \
    mn_fwdl.c mn_fwdl_hart.c \
    mn_hdr_validate.c controller_pid.c mn_block_header.c \
    mn_deviation_alert.c mn_advanced.c mn_diagnostic_configuration.c \
    eep_blk3.c mn_failed_state.c mn_flash_selftest.c \
    ffp_hmac.c \
    ../../../../../FIRMWARE/services/fault2alert.c

#unused hm_tools.c mn_decomp.c

LintArg_appl_if.c = -e960
#/* Bitwise operator applied to signed underlying type */
LintArg_controller_pid.c = -esym(960,12.7)
#/* previously declared */
LintArg_mn_hdr_validate.c = -esym(960,8.8)