#ifndef _TDA18212DD_H_
#define _TDA18212DD_H_
struct neumo_dvb_frontend *tda18212dd_attach(struct neumo_dvb_frontend *fe,
				       struct i2c_adapter *i2c, u8 adr);
#endif
