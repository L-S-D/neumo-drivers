/*
    TBS ECP3 FPGA based cards PCIe driver

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "tbsecp3.h"
#define dprintk(fmt, arg...)																					\
	printk(KERN_DEBUG pr_fmt("%s:%d " fmt), __func__, __LINE__, ##arg)

/* Module parameters for I2C speed configuration */
static int i2c_speed = 0;  /* 0 = use card default, otherwise override for all cards */
module_param(i2c_speed, int, 0644);
MODULE_PARM_DESC(i2c_speed, "I2C speed for all cards (0=use defaults, 3=1302kHz, 4=977kHz, 9=434kHz, 39=100kHz)");

/* Per-board-type I2C speed overrides (indexed by board_id) */
#define MAX_BOARD_TYPES 64  /* must be larger than highest TBSECP3_BOARD_* define */
static int i2c_speed_board[MAX_BOARD_TYPES];
static int i2c_speed_board_count = 0;
module_param_array(i2c_speed_board, int, &i2c_speed_board_count, 0644);
MODULE_PARM_DESC(i2c_speed_board, "Per-board-type I2C speed (indexed by board_id)");

/* Per-card MAC address overrides (format: "mac_addr:speed,mac_addr:speed,...") */
#define MAX_MAC_OVERRIDES 16
static char *i2c_speed_mac[MAX_MAC_OVERRIDES];
static int i2c_speed_mac_count = 0;
module_param_array(i2c_speed_mac, charp, &i2c_speed_mac_count, 0644);
MODULE_PARM_DESC(i2c_speed_mac, "Per-card I2C speed by MAC address (format: \"0x1234567890ab:9\")");

struct mac_speed_override {
	u64 mac;
	int speed;
};

static struct mac_speed_override mac_overrides[MAX_MAC_OVERRIDES];
static int mac_override_count = 0;

/* Parse MAC address overrides from module parameter strings */
static void parse_mac_overrides(void)
{
	int i;
	u64 mac;
	int speed;
	char *endp;

	mac_override_count = 0;

	for (i = 0; i < i2c_speed_mac_count && i < MAX_MAC_OVERRIDES; i++) {
		if (!i2c_speed_mac[i])
			continue;

		/* Parse "0x1234567890ab:9" format */
		mac = simple_strtoull(i2c_speed_mac[i], &endp, 0);
		if (*endp != ':') {
			pr_err("tbsecp3: Invalid MAC override format: %s (expected 'mac:speed')\n",
			       i2c_speed_mac[i]);
			continue;
		}

		speed = simple_strtol(endp + 1, NULL, 10);
		if (speed <= 0) {
			pr_err("tbsecp3: Invalid speed in MAC override: %s\n", i2c_speed_mac[i]);
			continue;
		}

		mac_overrides[mac_override_count].mac = mac;
		mac_overrides[mac_override_count].speed = speed;
		pr_info("tbsecp3: MAC override[%d]: 0x%012llx -> speed=%d\n",
		        mac_override_count, mac, speed);
		mac_override_count++;
	}
}

/* Determine I2C speed for a given card - checks overrides in priority order:
 * 1. Per-card MAC address override (highest priority)
 * 2. Per-board-type override
 * 3. Global i2c_speed parameter
 * 4. Card default from board info (lowest priority)
 * The card MAC address is only known after the adapters have been attached,
 * so the MAC override is applied by tbsecp3_i2c_update_speed().
 */
static u32 get_i2c_speed_for_card(struct tbsecp3_dev *dev, const char** source)
{
	int i;
	u16 board_id = dev->info->board_id;
	u64 mac = dev->card_mac_address;
	u32 speed;

	/* Priority 1: Check per-card MAC override */
	for (i = 0; mac != 0 && i < mac_override_count; i++) {
		if (mac_overrides[i].mac == mac) {
			*source = "MAC override";
			return mac_overrides[i].speed;
		}
	}

	/* Priority 2: Check per-board-type override */
	if (board_id < MAX_BOARD_TYPES && board_id < i2c_speed_board_count &&
			i2c_speed_board[board_id] > 0) {
		*source = "board override";
		return i2c_speed_board[board_id];
	}

	/* Priority 3: Check global override */
	if (i2c_speed > 0) {
		*source = "global override";
		return i2c_speed;
	}

	/* Priority 4: Use card default */
	*source = "card default";
	speed = dev->info->i2c_speed;
	/* default to 400kbps
		 i2c_speed in kHz is = 125000/4/baud/2/4
		 baud = 9:  434kHz
		 baud = 39: 100.16 kHz
		 baud = 3:  1302.08 kHz
	*/
	if (!speed)
		speed = 9;
	return speed;
}

union tbsecp3_i2c_ctrl {
	struct {
		u32 ctrl;
		u32 data;
	} raw;
	struct {
		u8 size:4;
		u8 saddr:2;
		u8 stop:1;
		u8 start:1;
		u8 read:1;
		u8 addr:7;
		u8 buf[6];
	} bits;
};

static int i2c_xfer(struct i2c_adapter *adapter, struct i2c_msg *msg, int num)
{
	struct tbsecp3_i2c *bus = i2c_get_adapdata(adapter);
	struct tbsecp3_dev *dev = bus->dev;
	union tbsecp3_i2c_ctrl i2c_ctrl;
	int i, j, retval;
	u16 len, remaining, xfer_max;
	u8 *b;

	mutex_lock(&bus->lock);
	//clear the i2c status
	tbs_read(bus->base, 0x00); //TBSECP3_I2C_STAT=0x00

	for (i = 0; i < num; i++) {

		b = msg[i].buf;
		remaining = msg[i].len;

		i2c_ctrl.raw.ctrl = 0;
		i2c_ctrl.bits.start = 1;
		i2c_ctrl.bits.addr = msg[i].addr;

		if (msg[i].flags & I2C_M_RD) {
			i2c_ctrl.bits.read = 1;
			xfer_max = 4;
		} else {
			xfer_max = 6;
		}

		do {
			if (remaining <= xfer_max)
				i2c_ctrl.bits.stop = 1;

			len = remaining > xfer_max ? xfer_max : remaining;
			i2c_ctrl.bits.size = len;

			if (!(msg[i].flags & I2C_M_RD)) {
				for (j = 0; j < len; j++)
					i2c_ctrl.bits.buf[j] = *b++;
				tbs_write(bus->base, TBSECP3_I2C_DATA, i2c_ctrl.raw.data);
			}
			bus->done = 0;
			tbs_write(bus->base, TBSECP3_I2C_CTRL, i2c_ctrl.raw.ctrl);
			retval = wait_event_timeout(bus->wq, bus->done == 1, HZ);
			if (retval == 0) {
				tbs_read(bus->base, TBSECP3_I2C_STAT); // restore iic to its original state
				dev_err(&dev->pci_dev->dev, "i2c xfer timeout\n");
				retval = -EIO;
				goto i2c_xfer_exit;
			}

			j = tbs_read(bus->base, TBSECP3_I2C_CTRL);
			if (j & 0x04) {
				dev_err(&dev->pci_dev->dev, "i2c nack (%x)\n", j);
				retval = -EIO;
				goto i2c_xfer_exit;
			}

			if (msg[i].flags & I2C_M_RD) {
				i2c_ctrl.raw.data = tbs_read(bus->base, TBSECP3_I2C_DATA);
				memcpy(b, &i2c_ctrl.raw.data, len);
				b += len;
			}

			i2c_ctrl.bits.start = 0;
			remaining -= len;
		} while (remaining);

	}
	retval = num;
i2c_xfer_exit:
	mutex_unlock(&bus->lock);
	return retval;
}

static u32 i2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_SMBUS_EMUL;
}

struct i2c_algorithm tbsecp3_i2c_algo_template = {
	.master_xfer   = i2c_xfer,
	.functionality = i2c_functionality,
};

static int tbsecp3_i2c_register(struct tbsecp3_i2c *bus)
{
	struct tbsecp3_dev *dev = bus->dev;
	struct i2c_adapter *adap;

	init_waitqueue_head(&bus->wq);
	mutex_init(&bus->lock);

	adap = &bus->i2c_adap;
	strcpy(adap->name, "tbsecp3");
	adap->algo = &tbsecp3_i2c_algo_template;
	adap->algo_data = (void*) bus;
	adap->dev.parent = &dev->pci_dev->dev;
	adap->owner = THIS_MODULE;

	strcpy(bus->i2c_client.name, "tbsecp3cli");
	bus->i2c_client.adapter = adap;

	i2c_set_adapdata(&bus->i2c_adap, bus);
	return i2c_add_adapter(&bus->i2c_adap);
}

static void tbsecp3_i2c_unregister(struct tbsecp3_i2c *bus)
{
	i2c_del_adapter(&bus->i2c_adap);
}

/* ----------------------------------------------------------------------- */


void tbsecp3_i2c_reg_init(struct tbsecp3_dev *dev)
{
	int i;
	const char* source;
	u32 baud;

	/* Parse MAC overrides once on first call */
	static bool mac_parsed = false;
	if (!mac_parsed) {
		parse_mac_overrides();
		mac_parsed = true;
	}

	baud = get_i2c_speed_for_card(dev, &source);
	dev->i2c_baud = baud;
	dev_info(&dev->pci_dev->dev, "%s (MAC 0x%012llx) i2c_speed=%u (~%u kHz, %s)\n",
					 dev->info->short_name, dev->card_mac_address, baud,
					 125000 / 4 / baud / 2 / 4, source);

	for (i = 0; i < 4; i++) {
		tbs_write(dev->i2c_bus[i].base, TBSECP3_I2C_BAUD, baud);
		tbs_read(dev->i2c_bus[i].base, TBSECP3_I2C_STAT);
		tbs_write(TBSECP3_INT_BASE, TBSECP3_I2C_IE(i), 1);
	}
}

/*
	Re-evaluate the i2c speed after the card MAC address is known (it is read over i2c
	while attaching the adapters) and apply it if it differs, e.g., due to a MAC override.
	The bus locks are held so that the speed does not change during a transfer.
 */
void tbsecp3_i2c_update_speed(struct tbsecp3_dev *dev)
{
	int i;
	const char* source;
	u32 baud = get_i2c_speed_for_card(dev, &source);

	if (baud == dev->i2c_baud)
		return;
	dev_info(&dev->pci_dev->dev, "%s (MAC 0x%012llx) changing i2c_speed %u -> %u (~%u kHz, %s)\n",
					 dev->info->short_name, dev->card_mac_address, dev->i2c_baud, baud,
					 125000 / 4 / baud / 2 / 4, source);
	for (i = 0; i < 4; i++) {
		mutex_lock(&dev->i2c_bus[i].lock);
		tbs_write(dev->i2c_bus[i].base, TBSECP3_I2C_BAUD, baud);
		mutex_unlock(&dev->i2c_bus[i].lock);
	}
	dev->i2c_baud = baud;
}

int tbsecp3_i2c_init(struct tbsecp3_dev *dev)
{
	int i, ret = 0;

	/* I2C Defaults / setup */
	for (i = 0; i < 4; i++) {
		dev->i2c_bus[i].base = TBSECP3_I2C_BASE(i);
		dev->i2c_bus[i].dev = dev;
		ret = tbsecp3_i2c_register(&dev->i2c_bus[i]);
		if (ret)
			break;
	}
	if (ret) {
		do {
			tbsecp3_i2c_unregister(&dev->i2c_bus[i]);
		} while (i-- > 0);
	} else {
		tbsecp3_i2c_reg_init(dev);
	}
	return ret;
}

void tbsecp3_i2c_exit(struct tbsecp3_dev *dev)
{
	int i;
	for (i = 0; i < 4; i++)
		tbsecp3_i2c_unregister(&dev->i2c_bus[i]);
}

//check for incorrect include files
#include <media/neumo-check.h>
