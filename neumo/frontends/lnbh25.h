/*
 * lnbh25.h
 */

#ifndef _LNBH25_H
#define _LNBH25_H

#include <media/neumo-dvb-frontend.h>

extern struct neumo_dvb_frontend *lnbh25_attach(struct neumo_dvb_frontend *fe,
					  struct i2c_adapter *i2c,
					  u8 i2c_addr);

#endif
