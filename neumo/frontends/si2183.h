/*
 * Silicon Labs Si2183(2) DVB-T/T2/C/C2/S/S2 demodulator driver
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 */

#ifndef SI2183_H
#define SI2183_H
#include <linux/version.h>
#include <linux/mutex.h>
#include <media/neumo-dvb-frontend.h>

#define SI2183_ARGLEN      30
struct si2183_cmd {
	u8 args[SI2183_ARGLEN];
	unsigned wlen;
	unsigned rlen;
};

/*
 * I2C address
 * 0x64
 */
struct si2183_config {
	/*
	 * frontend
	 * returned by driver
	 */
	struct neumo_dvb_frontend **fe;

	/*
	 * tuner I2C adapter
	 * returned by driver
	 */
	struct i2c_adapter **i2c_adapter;

	/* TS mode */
#define SI2183_TS_PARALLEL	0x06
#define SI2183_TS_SERIAL	0x03
	u8 ts_mode;

	/* TS clock inverted */
	bool ts_clock_inv;

	int  start_clk_mode;  //0 terrestrial mode 1: satellite mode

	/* TS clock gapped */
	bool ts_clock_gapped;
	/*agc*/
	u8 agc_mode;

	/*rf switch*/
	void (*RF_switch)(struct i2c_adapter * i2c,u8 rf_in,u8 flag);
	/*rf no.*/
	u8 rf_in;

	/*
	 * the demodulator whose LNB port drives the LNB of this one, NULL: its
	 * own (TBS 6508: the odd tuners have none wired, their even partner's
	 * port drives the cable they share). The 22 kHz tone, DiSEqC and burst
	 * of the satellite frontend go out there, and that demodulator stays
	 * powered up while the satellite frontend of this one is in use.
	 */
	struct i2c_client *sec_client;

	void (*TS_switch)(struct i2c_adapter * i2c,u8 flag);
	void (*LED_switch)(struct i2c_adapter * i2c,u8 flag);
	//update the FW.
	void (*write_properties) (struct i2c_adapter *i2c,u8 reg, u32 buf);
	void (*read_properties) (struct i2c_adapter *i2c,u8 reg, u32 *buf);
	// EEPROM access
	void (*write_eeprom) (struct i2c_adapter *i2c,u8 reg, u8 buf);
	void (*read_eeprom) (struct i2c_adapter *i2c,u8 reg, u8 *buf);
};

struct si_base {
	struct mutex i2c_mutex;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
	struct i2c_mux_core *muxc;
#endif
	struct list_head     silist;

	u8                   adr;
	struct i2c_adapter  *i2c;
	u32                  count;

	struct i2c_adapter  *tuner_adapter;

#ifndef SI2183_USE_I2C_MUX
	struct i2c_client *i2c_gate_client;
#endif
};

struct spectrum_scan_state {
	bool spectrum_present;
	bool scan_in_progress;

	s32* freq;
	s32* spectrum;
	int spectrum_len;
	int spectrum_max_len;
};

struct si2183_scan_status_t {
	s32 buzint;
	s32 reqint;
	s32 buz;
	s32 req;
	s32 scan_status;
	s32 rf_freq;
	s32 symb_rate;
	s32 modulation;
};

struct blindscan_state {
	struct si2183_scan_status_t scan_status;
	bool start_resume;
	bool skip_resume;
	s32 action;
	s32 bandwidth;
	s32 seek_freq;
};

struct constellation_scan_state {
	bool constellation_present;
	bool in_progress;

	struct dtv_fe_constellation_sample* samples;
	int num_samples;
	int samples_len;
	int constel_select;
};



/* state struct */
struct si2183_dev {
	struct neumo_dvb_frontend fe;
	//enum fe_delivery_system delivery_system;
	enum fe_status fe_status;
	u8 stat_resp;
	u16 snr;
	bool fw_loaded;
	u8 ts_mode;
	bool ts_clock_inv;
	bool ts_clock_gapped;
	int start_clk_mode;
	u8 agc_mode;
	struct si_base *base;
	s32 tuner_tuned_freq; //frequency asked of tuner, without any correction
	s32 demod_tuned_freq; //frequency asked of tuner, plus baseband correction
	void (*RF_switch)(struct i2c_adapter * i2c,u8 rf_in,u8 flag);
	int nr; //adapter aka adapter number
	u8 rf_in;
	u8 active_fe; /* the frontends in use (1 << fe->id), SI2183_SEC_USER */
	struct mutex power_lock; /* active_fe, power up and down */
	struct i2c_client *sec_client; /* see struct si2183_config */
	struct spectrum_scan_state scan_state;
	struct constellation_scan_state constellation_scan_state;
	struct blindscan_state blindscan_state;

	void (*TS_switch)(struct i2c_adapter * i2c,u8 flag);
	void (*LED_switch)(struct i2c_adapter * i2c,u8 flag);

	void (*write_properties) (struct i2c_adapter *i2c,u8 reg, u32 buf);
	void (*read_properties) (struct i2c_adapter *i2c,u8 reg, u32 *buf);

	void (*write_eeprom) (struct i2c_adapter *i2c,u8 reg, u8 buf);
	void (*read_eeprom) (struct i2c_adapter *i2c,u8 reg, u8 *buf);

};

int si2183_cmd_execute(struct i2c_client *client, struct si2183_cmd *cmd);
int si2183_cmd_execute_unlocked(struct i2c_client *client,
																struct si2183_cmd *cmd);

int si2183_set_frontend(struct neumo_dvb_frontend *fe);
int si2183_spectrum_start(struct neumo_dvb_frontend *fe,
																 struct dtv_fe_spectrum* s,
													unsigned int *delay, enum fe_status *status);
int si2183_spectrum_get(struct neumo_dvb_frontend *fe, struct dtv_fe_spectrum* user);
int si2183_scan_sat_(struct neumo_dvb_frontend *fe, bool init,
										 unsigned int *delay,  enum fe_status *status, bool dont_retune);
int si2183_scan_sat(struct neumo_dvb_frontend *fe, bool init,
										unsigned int *delay,  enum fe_status *status);
int si2183_constellation_start(struct neumo_dvb_frontend *fe,
															 struct dtv_fe_constellation* user, int max_num_samples);
int si2183_constellation_get(struct neumo_dvb_frontend *fe, struct dtv_fe_constellation* user);
int si2183_stop_task(struct neumo_dvb_frontend *fe);
int si2183_read_status(struct neumo_dvb_frontend *fe, enum fe_status *status);
enum fe_delivery_system si2183_delsys(int val);
enum fe_modulation si2183_modulation(int val);
enum fe_rolloff si2183_rolloff(int val);
enum fe_code_rate si2183_code_rate(int val);
s32 si2183_signal_power_dbm(struct neumo_dvb_frontend *fe);
int si2183_scan_status(struct i2c_client *client, u8 intack, struct si2183_scan_status_t*s);
int si2183_isi_scan(struct neumo_dvb_frontend* fe, int num_isi);
int si2183_misc_data(struct i2c_client *client , bool* issyi, bool* npd);
int blind_tune(struct neumo_dvb_frontend *fe);
int si2183_set_sat_agc(struct i2c_client *client);


extern void si2183_cmd_str(struct si2183_cmd* cmd, const char* str, int wlen, int rlen);

#endif
