
/*
 * neumo-dvb_frontend_private.h
 *
 * The Digital TV Frontend kABI defines a driver-internal interface for
 * registering low-level, hardware specific driver to a hardware independent
 * frontend layer.
 *
 * Copyright (C) 2001 convergence integrated media GmbH
 * Copyright (C) 2004 convergence GmbH
 * Copyright (C) 2024-2026 Deep Thought <deeptho@gmail.com> changes for neumodvb
 *
 * Written by Ralph Metzler
 * Overhauled by Holger Waechtler
 * Kernel I2C stuff by Michael Hunold <hunold@convergence.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *

 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#pragma once

struct neumo_dtv_frontend_properties;
struct neumo_dvb_frontend_private;

extern int dvb_frontend_debug;
extern int dvb_shutdown_timeout;
extern int dvb_force_auto_inversion;
extern int dvb_override_tune_delay;
extern int dvb_powerdown_on_sleep;
extern int dvb_mfe_wait_time;

extern struct mutex frontend_mutex;

struct neumo_dtv_fe_algo_state {
	atomic_t task_should_stop;
	atomic_t cur_index; //used to measure progress; should be updated from time to time
	atomic_t max_index; //when cur_inde==max_index task is done
	wait_queue_head_t wait_queue;
};

struct neumo_dvb_frontend_private {
	/* thread/frontend values */
	struct dvb_device* dvbdev;
	struct dvb_frontend_parameters dvb_parameters_out;
	struct neumo_dvb_fe_events events;
	struct semaphore sem;
	//struct list_head list_head;
	wait_queue_head_t wait_queue;
	struct task_struct *thread;
	unsigned long release_jiffies;
	unsigned int wakeup;
	enum fe_status status;
	unsigned long tune_mode_flags;
	unsigned int delay;
	unsigned int reinitialise;
	int tone;
	int voltage;

	unsigned int state;
	unsigned int bending;
	int lnb_drift;
	unsigned int inversion;
	unsigned int auto_step;
	unsigned int auto_sub_step;
	unsigned int started_auto_step;
	unsigned int min_delay;
	unsigned int max_drift;
	unsigned int step_size;
	int quality;
	unsigned int check_wrapped;
	enum neumo_dvbfe_search algo_status;
	struct dtv_fe_spectrum spectrum;
	struct dtv_fe_constellation constellation;
	int heartbeat_interval;
#if defined(CONFIG_MEDIA_CONTROLLER_DVB)
	struct media_pipeline pipe;
#endif
	struct neumo_dtv_fe_algo_state algo_state;
	/*
	 * Copies of the property cache, too large for the kernel stack (about
	 * 1.8 KB): one the frontend thread keeps across set_frontend(), one for
	 * the properties an ioctl reports. Both used with the semaphore held.
	 */
	struct neumo_dtv_frontend_properties cache_save;
	struct neumo_dtv_frontend_properties getp;
};
