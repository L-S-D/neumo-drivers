#ifndef _DRXK_H_
#define _DRXK_H_

#include <linux/types.h>
#include <linux/i2c.h>

extern struct neumo_dvb_frontend *drxk_attach(struct i2c_adapter *i2c, 
					u8 adr,
					struct neumo_dvb_frontend **fe_t);
#endif
