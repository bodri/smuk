#include "smu_cal_seq.h"
#include "safety_hw.h"
#include "range_hw.h"

void smu_cal_seq_init(smu_cal_seq_t *s) {
	*s = (smu_cal_seq_t){0};
	s->discard_required = 16;
	s->acquire_required = 64;
}

bool smu_cal_seq_start(smu_cal_seq_t *s, smu_cal_target_t t, calbus_sel_t b) {
	if (s->state != CAL_SEQ_IDLE || b > CALBUS_N3V)
		return false;
	s->target = t;
	s->bus = b;
	s->state = CAL_SEQ_SAFE;
	s->state_ms = 0;
	s->measurement_valid = false;
	s->servo_allowed = false;
	s->fault = false;
	s->result_ready = false;
	return true;
}

void smu_cal_seq_adc_frame(smu_cal_seq_t *s,
                           int32_t target,
                           int32_t calbus) {
    if (s->state == CAL_SEQ_DISCARD) {
        if (s->discarded < s->discard_required) {
            s->discarded++;
        }
    } else if ((s->state == CAL_SEQ_ACQUIRE) &&
             (s->acquired < s->acquire_required)) {
        s->target_sum += target;
        s->calbus_sum += calbus;
        s->acquired++;
    }
}

void smu_cal_seq_tick_1ms(smu_cal_seq_t *s) {
	s->state_ms++;
	switch (s->state) {
	case CAL_SEQ_IDLE:
		return;
	case CAL_SEQ_SAFE:
		safety_hw_disable_pa();
		range_hw_all_off();
		cal_hw_voltage_relay(false);
		cal_hw_current_inject_relay(false);
		if (!cal_hw_pa_interlock_ok()) {
			s->fault = true;
			s->state = CAL_SEQ_FAULT;
			break;
		}
		s->state = CAL_SEQ_SELECT;
		s->state_ms = 0;
		break;
	case CAL_SEQ_SELECT:
		cal_hw_select_bus(s->bus);
		s->state = CAL_SEQ_SETTLE;
		s->state_ms = 0;
		break;
	case CAL_SEQ_SETTLE:
		if (s->state_ms >= 2) {
			s->state = CAL_SEQ_RELAY;
			s->state_ms = 0;
		}
		break;
	case CAL_SEQ_RELAY:
		if (s->target == CAL_TARGET_VOLTAGE)
			cal_hw_voltage_relay(true);
		else
			cal_hw_current_inject_relay(true);
		s->discarded = 0;
		s->state = CAL_SEQ_DISCARD;
		s->state_ms = 0;
		break;
	case CAL_SEQ_DISCARD:
		if (s->discarded >= s->discard_required) {
			s->acquired = 0;
			s->target_sum = 0;
			s->calbus_sum = 0;
			s->state = CAL_SEQ_ACQUIRE;
			s->state_ms = 0;
		}
		break;
	case CAL_SEQ_ACQUIRE:
	    if (s->acquired >= s->acquire_required)
	    {
	        if (s->acquire_required == 0u)
	        {
	            s->fault = true;
	            s->state = CAL_SEQ_FAULT;
	            break;
	        }

	        s->target_average =
	            (float)s->target_sum /
	            (float)s->acquire_required;

	        s->calbus_average =
	            (float)s->calbus_sum /
	            (float)s->acquire_required;

	        if (s->calbus_sum != 0)
	        {
	        	s->ratio =
	        	    (float)s->target_sum /
	        	    (float)s->calbus_sum;
	        }
	        else
	        {
	            /*
	             * Zero CALBUS is legitimate for our offset
	             * calibration point.
	             */
	            s->ratio = 0.0f;
	        }

	        s->result_ready = true;

	        s->state = CAL_SEQ_OPEN;
	        s->state_ms = 0;
	    }
	    break;
	case CAL_SEQ_OPEN:
		cal_hw_voltage_relay(false);
		cal_hw_current_inject_relay(false);
		s->state = CAL_SEQ_ZERO;
		s->state_ms = 0;
		break;
	case CAL_SEQ_ZERO:
		cal_hw_select_bus(CALBUS_0V);
		s->state = CAL_SEQ_DONE;
		break;
	case CAL_SEQ_DONE:
		s->measurement_valid = false;
		s->servo_allowed = false;
		s->state = CAL_SEQ_IDLE;
		break;
	case CAL_SEQ_FAULT:
		safety_hw_disable_pa();
		range_hw_all_off();
		cal_hw_voltage_relay(false);
		cal_hw_current_inject_relay(false);
		cal_hw_select_bus(CALBUS_0V);
		s->measurement_valid = false;
		s->servo_allowed = false;
		break;
	}
}
