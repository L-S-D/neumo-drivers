#ifndef _TDA18271C2DD_H_
#define _TDA18271C2DD_H_

#include <linux/types.h>
#include <linux/i2c.h>

struct neumo_dvb_frontend *tda18271c2dd_attach(struct neumo_dvb_frontend *fe,
					 struct i2c_adapter *i2c, u8 adr);

#endif
