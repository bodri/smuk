#include "smu.h"
#include "../Drivers/SMU/safety_hw.h"
#include "../Drivers/SMU/range_hw.h"
#include "../Drivers/SMU/ad5686.h"
#include "../Drivers/SMU/ads131m03.h"
#include <string.h>

static smu_context_t g;
static uint32_t state_ms;
static void enter(smu_state_t s){ g.state=s; state_ms=0; }
static void fault(uint32_t f){ g.faults|=f; smu_output_disable(); enter(SMU_STATE_FAULT); }

void smu_init(void){
 memset(&g,0,sizeof(g)); g.state=SMU_STATE_POWER_UP; g.range=SMU_RANGE_NONE;
 safety_hw_init_safe(); range_hw_all_off();
}

void smu_tick_1khz(void){ state_ms++; safety_hw_watchdog_heartbeat(); }

void smu_process(void){
 switch(g.state){
 case SMU_STATE_POWER_UP:
   safety_hw_disable_pa(); range_hw_all_off();
   if(!ad5686_init()){ fault(SMU_FAULT_DAC); break; }
   if(!ads131m03_init() || !ads131m03_start()){ fault(SMU_FAULT_ADC); break; }
   enter(SMU_STATE_SELF_TEST); break;
 case SMU_STATE_SELF_TEST:
   if(!safety_hw_power_good()){ if(state_ms>100) fault(SMU_FAULT_POWER); break; }
   if(!range_hw_verify_one_hot(SMU_RANGE_NONE)){ fault(SMU_FAULT_RANGE); break; }
   enter(SMU_STATE_OUTPUT_OFF); break;
 case SMU_STATE_OUTPUT_OFF: break;
 case SMU_STATE_OUTPUT_STARTING:
   if(!range_hw_verify_one_hot(SMU_RANGE_1P5A)){ if(state_ms>10) fault(SMU_FAULT_RANGE); break; }
   if(!safety_hw_request_pa_enable()){ fault(SMU_FAULT_POWER); break; }
   g.measurement_valid=false; enter(SMU_STATE_NORMAL); break;
 case SMU_STATE_NORMAL:
   if(safety_hw_compliance_active()){ g.servo_enabled=false; enter(SMU_STATE_COMPLIANCE); }
   break;
 case SMU_STATE_COMPLIANCE:
   g.servo_enabled=false;
   if(!safety_hw_compliance_active() && state_ms>=5) enter(SMU_STATE_NORMAL);
   break;
 case SMU_STATE_RANGE_CHANGE: /* transition policy intentionally deferred to hardware bring-up */ break;
 case SMU_STATE_CALIBRATION: break;
 case SMU_STATE_FAULT: safety_hw_disable_pa(); range_hw_all_off(); break;
 default: fault(SMU_FAULT_SELFTEST); break;
 }
}

smu_status_t smu_output_enable(void){
 if(g.state!=SMU_STATE_OUTPUT_OFF || g.faults) return SMU_ERR_STATE;
 g.output_requested=true; g.measurement_valid=false; g.servo_enabled=false;
 range_hw_all_off(); range_hw_command(SMU_RANGE_1P5A,true); g.range=SMU_RANGE_1P5A;
 enter(SMU_STATE_OUTPUT_STARTING); return SMU_OK;
}

void smu_output_disable(void){
 safety_hw_disable_pa(); range_hw_all_off(); g.output_requested=false; g.servo_enabled=false;
 g.measurement_valid=false; g.range=SMU_RANGE_NONE;
 if(g.state!=SMU_STATE_FAULT) enter(SMU_STATE_OUTPUT_OFF);
}

smu_status_t smu_set_mode(smu_force_mode_t mode){ if(g.state!=SMU_STATE_OUTPUT_OFF) return SMU_ERR_STATE; g.mode=mode; return SMU_OK; }
smu_status_t smu_request_range(smu_current_range_t r){ if(r==SMU_RANGE_NONE) return SMU_ERR_ARG; if(g.state!=SMU_STATE_NORMAL) return SMU_ERR_STATE; g.measurement_valid=false; g.servo_enabled=false; enter(SMU_STATE_RANGE_CHANGE); (void)r; return SMU_OK; }
const smu_context_t *smu_get_context(void){ return &g; }
