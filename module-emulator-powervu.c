#define MODULE_LOG_PREFIX "emu"

#include "globals.h"

#ifdef WITH_EMU

#include "cscrypt/des.h"
#include "ffdecsa/ffdecsa.h"
#include "module-emulator-osemu.h"
#include "module-emulator-streamserver.h"
#include "module-emulator-powervu.h"
#include "oscam-string.h"
#include "oscam-time.h"

static inline uint8_t GetBit(uint8_t byte, uint8_t bitnb)
{
	return ((byte & (1 << bitnb)) ? 1 : 0);
}

static inline uint8_t SetBit(uint8_t val, uint8_t bitnb, uint8_t biton)
{
	return (biton ? (val | (1 << bitnb)) : (val & ~(1 << bitnb)));
}

static uint8_t PowervuCrc8Calc(uint8_t *data, int len)
{
	int i;
	uint8_t crc = 0;
	uint8_t crcTable[256] = { 0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
							  0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
							  0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
							  0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
							  0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
							  0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
							  0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
							  0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
							  0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
							  0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
							  0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
							  0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
							  0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
							  0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
							  0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
							  0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
							  0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
							  0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
							  0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
							  0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
							  0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
							  0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
							  0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
							  0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
							  0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
							  0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
							  0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
							  0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
							  0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
							  0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
							  0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
							  0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3 };

	for(i = 0; i < len; i++)
	{
		crc = crcTable[data[i] ^ crc];
	}

	return crc;
}

static void PowervuPadData(uint8_t *data, int len, uint8_t *dataPadded)
{
	int i;
	uint8_t pad[] = { 0x01, 0x02, 0x22, 0x04, 0x20, 0x2A, 0x1F, 0x03,
					  0x04, 0x06, 0x02, 0x0C, 0x2B, 0x2B, 0x01, 0x7B };

	for(i = 0; i < len; i++)
	{
		dataPadded[i] = data[i];
	}

	dataPadded[len] = 0x01;

	for(i = len + 1; i < 0x2F; i++)
	{
		dataPadded[i] = 0x00;
	}

	dataPadded[0x2F] = len;

	for(i = 0; i < 0x10; i++)
	{
		dataPadded[0x30 + i] = pad[i];
	}
}

static void PowervuHashMode01CustomMD5(uint8_t *data, uint8_t *hash)
{
	int i, j, s;
	uint32_t a, b, c, d, f = 0, g;

	uint32_t T[] = { 0x783E16F6, 0xC267AC13, 0xA2B17F12, 0x6B8A31A4,
					 0xF910654D, 0xB702DBCB, 0x266CEF60, 0x5145E47C,
					 0xB92E00D6, 0xE80A4A64, 0x8A07FA77, 0xBA7D89A9,
					 0xEBED8022, 0x653AAF2B, 0xF118B03B, 0x6CC16544,
					 0x96EB6583, 0xF4E27E35, 0x1ABB119E, 0x068D3EF2,
					 0xDAEAA8A5, 0x3C312A3D, 0x59538388, 0xA100772F,
					 0xAB0165CE, 0x979959E7, 0x5DD8F53D, 0x189662BA,
					 0xFD021A9C, 0x6BC2D338, 0x1EFF667E, 0x40C66888,
					 0x6E9F07FF, 0x0CEF442F, 0x82D20190, 0x4E8CAEAC,
					 0x0F7CB305, 0x2E73FBE7, 0x1CE884A2, 0x7A60BD52,
					 0xC348B30D, 0x081CE3AA, 0xA12220E7, 0x38C7EC79,
					 0xCBD8DD3A, 0x62B4FBA5, 0xAD2A63DB, 0xE4D0852E,
					 0x53DE980F, 0x9C8DDA59, 0xA6B4CEDE, 0xB48A7692,
					 0x0E2C46A4, 0xEB9367CB, 0x165D72EE, 0x75532B45,
					 0xB9CA8E97, 0x08C8837B, 0x966F917B, 0x527515B4,
					 0xF27A5E5D, 0xB71E6267, 0x7603D7E6, 0x9837DD69 }; // CUSTOM T

	uint8_t r[] = { 0x06, 0x0A, 0x0F, 0x15, 0x05, 0x09, 0x0E, 0x14,
					0x04, 0x0B, 0x10, 0x17, 0x07, 0x0C, 0x11, 0x16 }; // STANDARD REORDERED

	uint8_t tIdxInit[] = {0, 1, 5, 0}; // STANDARD
	uint8_t tIdxIncr[] = {1, 5, 3, 7}; // STANDARD

	uint32_t h[] = { 0xEAD81D2E, 0xCE4DC6E9, 0xF9B5C301, 0x10325476 }; // CUSTOM h0, h1, h2, STANDARD h3
	uint32_t dataLongs[16];

	for (i = 0; i < 16; i++)
	{
		dataLongs[i] = (data[4 * i + 0] << 0) + (data[4 * i + 1] << 8) + (data[4 * i + 2] << 16) + (data[4 * i + 3] << 24);
	}

	a = h[0];
	b = h[1];
	c = h[2];
	d = h[3];

	for (i = 0; i < 4; i++)
	{
		g = tIdxInit[i];

		for (j = 0; j < 16; j++)
		{
			if (i == 0)
			{
				f = (b & c) | (~b & d);
			}
			else if (i == 1)
			{
				f = (b & d) | (~d & c);
			}
			else if (i == 2)
			{
				f = (b ^ c ^ d);
			}
			else if (i == 3)
			{
				f = (~d | b) ^ c;
			}

			f = dataLongs[g] + a + T[16 * i + j] + f;

			s = r[4 * i + (j & 3)];
			f = (f << s) | (f >> (32 - s));

			a = d;
			d = c;
			c = b;
			b += f;

			g = (g + tIdxIncr[i]) & 0xF;
		}
	}

	h[0] += a;
	h[1] += b;
	h[2] += c;
	h[3] += d;

	for (i = 0; i < 4; i++)
	{
		hash[4 * i + 0] = h[i] >> 0;
		hash[4 * i + 1] = h[i] >> 8;
		hash[4 * i + 2] = h[i] >> 16;
		hash[4 * i + 3] = h[i] >> 24;
	}
}

static void PowervuHashMode02(uint8_t *data, uint8_t *hash)
{
	int i;
	uint32_t a, b, c, d, e, f = 0, tmp;
	uint32_t h[] = { 0x81887F3A, 0x36CCA480, 0x99056FB1, 0x79705BAE };
	uint32_t dataLongs[80];

	for (i = 0; i < 16; i++)
	{
		dataLongs[i] = (data[4 * i + 0] << 24) + (data[4 * i + 1] << 16) + (data[4 * i + 2] << 8) + (data[4 * i + 3] << 0);
	}

	for (i = 0; i < 64; i++)
	{
		dataLongs[16 + i] = dataLongs[16 + i - 2];
		dataLongs[16 + i] ^= dataLongs[16 + i - 7];
		dataLongs[16 + i] ^= dataLongs[16 + i - 13];
		dataLongs[16 + i] ^= dataLongs[16 + i - 16];
	}

	a = dataLongs[0];
	b = dataLongs[1];
	c = dataLongs[2];
	d = dataLongs[3];
	e = dataLongs[4];

	for (i = 0; i < 80; i++)
	{
		if (i < 0x15) f = (b & c) | (~b & d);
		else if (i < 0x28) f = (b ^ c ^ d);
		else if (i < 0x3D) f = (b & c) | (c & d) | (b & d);
		else if (i < 0x50) f = (b ^ c ^ d);

		tmp = a;
		a = e + f + (a << 5) + (a >> 27) + h[i / 0x14] + dataLongs[i];
		e = d;
		d = c;
		c = (b << 30) + (b >> 2);
		b = tmp;
	}

	dataLongs[0] += a;
	dataLongs[1] += b;
	dataLongs[2] += c;
	dataLongs[3] += d;

	for (i = 0; i < 4; i++)
	{
		hash[4 * i + 0] = dataLongs[i] >> 24;
		hash[4 * i + 1] = dataLongs[i] >> 16;
		hash[4 * i + 2] = dataLongs[i] >> 8;
		hash[4 * i + 3] = dataLongs[i] >> 0;
	}
}

static void PowervuHashMode03(uint8_t *data, uint8_t *hash)
{
	int i, j, k, s, s2, tmp;
	uint32_t a, b, c, d, f = 0, g;
	uint32_t a2, b2, c2, d2, f2 = 0, g2;

	uint32_t T[] = { 0xC88F3F2E, 0x967506BA, 0xDA877A7B, 0x0DECCDFE };
	uint32_t T2[] = { 0x01F42668, 0x39C7CDA5, 0xD490E2FE, 0x9965235D };

	uint8_t r[] = { 0x0B, 0x0E, 0x0F, 0x0C, 0x05, 0x08, 0x07, 0x09,
					0x0B, 0x0D, 0x0E, 0x0F, 0x06, 0x07, 0x09, 0x08,
					0x07, 0x06, 0x08, 0x0D, 0x0B, 0x09, 0x07, 0x0F,
					0x07, 0x0C, 0x0F, 0x09, 0x0B, 0x07, 0x0D, 0x0C };

	uint8_t tIdxIncr[] = { 0x07, 0x04, 0x0D, 0x01, 0x0A, 0x06, 0x0F, 0x03,
						   0x0C, 0x00, 0x09, 0x05, 0x02, 0x0E, 0x0B, 0x08,
						   0x05, 0x0D, 0x02, 0x00, 0x04, 0x09, 0x03, 0x08,
						   0x01, 0x0A, 0x07, 0x0B, 0x06, 0x0F, 0x0C, 0x0E };

	uint32_t h[] = { 0xC8616857, 0x9D3F5B8E, 0x4D7B8F76, 0x97BC8D80 };

	uint32_t dataLongs[80];
	uint32_t result[4];

	for (i = 0; i < 16; i++)
	{
		dataLongs[i] = (data[4 * i + 0] << 24) + (data[4 * i + 1] << 16) + (data[4 * i + 2] << 8) + (data[4 * i + 3] << 0);
	}

	a = h[0];
	b = h[1];
	c = h[2];
	d = h[3];

	a2 = h[3];
	b2 = h[2];
	c2 = h[1];
	d2 = h[0];

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 16; j++)
		{
			tmp = j;

			for (k = 0; k < i; k++)
			{
				tmp = tIdxIncr[tmp];
			}

			g = 0x0F - tmp;
			g2 = tmp;

			if (i == 0) f = (b & d) | (~d & c);
			else if (i == 1) f = (~c | b) ^ d;
			else if (i == 2) f = (~b & d) | (b & c);
			else if (i == 3) f = (b ^ c ^ d);

			if (i == 0) f2 = (b2 ^ c2 ^ d2);
			else if (i == 1) f2 = (~b2 & d2) | (b2 & c2);
			else if (i == 2) f2 = (~c2 | b2) ^ d2;
			else if (i == 3) f2 = (b2 & d2) | (~d2 & c2);

			f = dataLongs[g] + a + T[i] + f;
			s = r[0x0F + (((i & 1) ^ 1) << 4) - j];
			f = (f << s) | (f >> (32 - s));

			f2 = dataLongs[g2] + a2 + T2[i] + f2;
			s2 = r[((i & 1) << 4) + j];
			f2 = (f2 << s2) | (f2 >> (32 - s2));

			a = d;
			d = (c << 10) | (c >> 22);
			c = b;
			b = f;

			a2 = d2;
			d2 = (c2 << 10) | (c2 >> 22);
			c2 = b2;
			b2 = f2;
		}
	}

	result[0] = h[3] + b + a2;
	result[1] = h[2] + c + b2;
	result[2] = h[1] + d + c2;
	result[3] = h[0] + a + d2;

	for (i = 0; i < 4; i++)
	{
		hash[4 * i + 0] = result[i] >> 0;
		hash[4 * i + 1] = result[i] >> 8;
		hash[4 * i + 2] = result[i] >> 16;
		hash[4 * i + 3] = result[i] >> 24;
	}
}

static const uint8_t table04[] = { 0x02, 0x03, 0x07, 0x0B, 0x0D, 0x08, 0x00, 0x01,
								   0x2B, 0x2D, 0x28, 0x20, 0x21, 0x0A, 0x0C, 0x0E,
								   0x22, 0x36, 0x23, 0x27, 0x29, 0x24, 0x25, 0x26,
								   0x2A, 0x3C, 0x3E, 0x3F, 0x0F, 0x2C, 0x2E, 0x2F,
								   0x12, 0x13, 0x17, 0x1B, 0x1C, 0x18, 0x10, 0x11,
								   0x19, 0x14, 0x15, 0x16, 0x1A, 0x09, 0x04, 0x05,
								   0x32, 0x33, 0x37, 0x3B, 0x06, 0x1C, 0x1E, 0x1F,
								   0x3D, 0x38, 0x30, 0x31, 0x39, 0x34, 0x35, 0x3A };

static const uint8_t table05[] = { 0x08, 0x09, 0x0A, 0x03, 0x04, 0x3F, 0x27, 0x28,
								   0x29, 0x2A, 0x05, 0x0B, 0x1B, 0x1C, 0x1C, 0x1E,
								   0x20, 0x0C, 0x0D, 0x22, 0x23, 0x24, 0x00, 0x01,
								   0x02, 0x06, 0x07, 0x25, 0x26, 0x0E, 0x0F, 0x21,
								   0x10, 0x11, 0x12, 0x2E, 0x2F, 0x13, 0x14, 0x15,
								   0x2B, 0x2C, 0x2D, 0x16, 0x17, 0x18, 0x19, 0x1A,
								   0x30, 0x31, 0x37, 0x3B, 0x3C, 0x3D, 0x3E, 0x1F,
								   0x38, 0x39, 0x32, 0x33, 0x34, 0x35, 0x36, 0x3A };

static const uint8_t table06[] = { 0x00, 0x01, 0x02, 0x06, 0x07, 0x08, 0x03, 0x2A,
								   0x2B, 0x2C, 0x2E, 0x2F, 0x04, 0x05, 0x09, 0x0D,
								   0x20, 0x21, 0x22, 0x26, 0x27, 0x3A, 0x3B, 0x3C,
								   0x3E, 0x3F, 0x10, 0x11, 0x12, 0x16, 0x17, 0x28,
								   0x18, 0x13, 0x14, 0x15, 0x19, 0x1C, 0x1A, 0x1B,
								   0x1C, 0x1E, 0x1F, 0x23, 0x24, 0x25, 0x29, 0x2D,
								   0x30, 0x31, 0x32, 0x36, 0x37, 0x38, 0x33, 0x34,
								   0x0A, 0x0B, 0x0C, 0x0E, 0x0F, 0x35, 0x39, 0x3D };

static const uint8_t table07[] = { 0x10, 0x11, 0x12, 0x17, 0x1C, 0x1E, 0x0E, 0x38,
								   0x39, 0x3A, 0x13, 0x14, 0x29, 0x2A, 0x16, 0x1F,
								   0x00, 0x01, 0x02, 0x3C, 0x3D, 0x3E, 0x3F, 0x07,
								   0x08, 0x09, 0x03, 0x04, 0x05, 0x06, 0x3B, 0x0A,
								   0x20, 0x21, 0x22, 0x19, 0x1A, 0x1B, 0x1C, 0x0B,
								   0x0C, 0x15, 0x23, 0x24, 0x25, 0x26, 0x18, 0x0F,
								   0x30, 0x31, 0x2B, 0x33, 0x34, 0x35, 0x36, 0x37,
								   0x27, 0x28, 0x2C, 0x2D, 0x2E, 0x2F, 0x32, 0x0D };

static const uint8_t table08[] = { 0x10, 0x11, 0x1E, 0x17, 0x18, 0x19, 0x12, 0x13,
								   0x14, 0x1C, 0x1C, 0x15, 0x0D, 0x05, 0x06, 0x0A,
								   0x00, 0x01, 0x0E, 0x07, 0x08, 0x09, 0x02, 0x2D,
								   0x25, 0x26, 0x2A, 0x2B, 0x2F, 0x03, 0x04, 0x0C,
								   0x20, 0x21, 0x2E, 0x27, 0x28, 0x29, 0x30, 0x31,
								   0x3E, 0x37, 0x38, 0x39, 0x22, 0x23, 0x24, 0x2C,
								   0x32, 0x33, 0x34, 0x3C, 0x3D, 0x35, 0x36, 0x3A,
								   0x3B, 0x0B, 0x0F, 0x16, 0x1A, 0x1B, 0x1F, 0x3F };

static const uint8_t table09[] = { 0x20, 0x21, 0x24, 0x22, 0x23, 0x2A, 0x2B, 0x33,
								   0x35, 0x38, 0x39, 0x36, 0x2D, 0x2C, 0x2E, 0x2F,
								   0x00, 0x01, 0x04, 0x02, 0x25, 0x28, 0x08, 0x09,
								   0x06, 0x07, 0x0A, 0x0B, 0x0D, 0x0C, 0x0E, 0x0F,
								   0x10, 0x11, 0x14, 0x12, 0x13, 0x15, 0x19, 0x16,
								   0x29, 0x26, 0x03, 0x17, 0x1A, 0x1C, 0x1C, 0x1E,
								   0x30, 0x31, 0x34, 0x32, 0x37, 0x3A, 0x3B, 0x3D,
								   0x3C, 0x3E, 0x3F, 0x1B, 0x05, 0x18, 0x27, 0x1F };

static const uint8_t table0A[] = { 0x00, 0x04, 0x05, 0x0B, 0x0C, 0x06, 0x09, 0x0A,
								   0x0E, 0x0D, 0x0F, 0x25, 0x15, 0x1B, 0x1C, 0x16,
								   0x10, 0x11, 0x01, 0x02, 0x03, 0x07, 0x08, 0x12,
								   0x13, 0x17, 0x18, 0x14, 0x23, 0x27, 0x28, 0x24,
								   0x30, 0x31, 0x32, 0x33, 0x37, 0x38, 0x34, 0x35,
								   0x3B, 0x3C, 0x20, 0x21, 0x22, 0x2B, 0x2C, 0x26,
								   0x36, 0x39, 0x3A, 0x3E, 0x3D, 0x19, 0x1A, 0x1E,
								   0x1C, 0x1F, 0x3F, 0x29, 0x2A, 0x2E, 0x2D, 0x2F };

static void PowervuHashModes04to0ATables(uint8_t *data, uint8_t *hash, const uint8_t *table)
{
	int i;

	for (i = 0; i < 16; i++)
	{
		hash[i] = table[i];
		hash[i] ^= data[table[i]];
		hash[i] ^= table[16 + i];
		hash[i] ^= data[table[16 + i]];
		hash[i] ^= table[32 + i];
		hash[i] ^= data[table[32 + i]];
		hash[i] ^= table[48 + i];
		hash[i] ^= data[table[48 + i]];
	}
}

static const uint8_t table0F[] = { 0xC7, 0x45, 0x15, 0x71, 0x61, 0x07, 0x05, 0x47 };
static const uint8_t table10[] = { 0x0F, 0x47, 0x2B, 0x6C, 0xAD, 0x0F, 0xB3, 0xEA };
static const uint8_t table11[] = { 0xB1, 0x46, 0xD1, 0x66, 0x5D, 0x28, 0x59, 0xD2 };
static const uint8_t table12[] = { 0x0B, 0x4B, 0xD7, 0x68, 0x5F, 0xAD, 0x4B, 0xBB };
static const uint8_t table13[] = { 0x4F, 0x4E, 0xE1, 0x6A, 0x21, 0xD3, 0xF7, 0xA6 };
static const uint8_t table14[] = { 0xDD, 0x39, 0xB9, 0x65, 0x03, 0x91, 0xF1, 0xAC };
static const uint8_t table15[] = { 0x3F, 0x50, 0xB5, 0x6F, 0x37, 0xC9, 0x13, 0x5D };
static const uint8_t table16[] = { 0xF9, 0x5C, 0xFD, 0x72, 0x19, 0x42, 0x23, 0x6B };
static const uint8_t table17[] = { 0xDF, 0x60, 0x93, 0x64, 0x33, 0x16, 0xB3, 0x8A };
static const uint8_t table18[] = { 0x09, 0x64, 0x5F, 0x6B, 0xFB, 0x21, 0x19, 0xE4 };

static void PowervuHashModes0Fto18Tables(uint8_t *data, uint8_t *hash, const uint8_t *table)
{
	int i;
	uint32_t t[4], tmp;

	memset(hash, 0x00, 16);

	t[0] = (table[1] << 8) + table[0];
	t[1] = (table[3] << 8) + table[2];
	t[2] = (table[5] << 8) + table[4];
	t[3] = (table[7] << 8) + table[6];

	for (i = 0; i < 60; i += 4)
	{
		t[0] = ((t[0] & 0xFFFF) * t[2]) + (t[0] >> 16);
		t[1] = ((t[1] & 0xFFFF) * t[3]) + (t[1] >> 16);
		tmp = t[0] + t[1];

		hash[(i + 0) & 0x0F] = hash[(i + 0) & 0x0F] ^ data[i + 0] ^ (tmp >> 24);
		hash[(i + 1) & 0x0F] = hash[(i + 1) & 0x0F] ^ data[i + 1] ^ (tmp >> 16);
		hash[(i + 2) & 0x0F] = hash[(i + 2) & 0x0F] ^ data[i + 2] ^ (tmp >> 8);
		hash[(i + 3) & 0x0F] = hash[(i + 3) & 0x0F] ^ data[i + 3] ^ (tmp >> 0);
	}
}

static const uint8_t table19[] = { 0x02, 0x03, 0x05, 0x10 };

static void PowervuHashModes19to27Tables3(uint8_t *data, uint8_t *hash, const uint8_t *table)
{
	int i;
	uint8_t val, it[4];
	uint16_t seed = 0xFFFF, tmp;

	memset(hash, 0x00, 16);

	for (i = 0; i < 4; i++)
	{
		it[i] = 0x10 - table[i];
	}

	for (i = 0; i < 16; i++)
	{
		val = ((seed >> it[0]) ^ (seed >> it[1]) ^ (seed >> it[2]) ^ (seed >> it[3])) & 0x01;

		if (val == 0x00)
		{
			seed = seed >> 1;
		}
		else
		{
			seed = (seed >> 1) | 0x8000;
		}
		tmp = seed + (data[i] << 8) + data[i + 32];

		val = ((seed >> it[0]) ^ (seed >> it[1]) ^ (seed >> it[2]) ^ (seed >> it[3])) & 0x01;

		if (val == 0x00)
		{
			seed = seed >> 1;
		}
		else
		{
			seed = (seed >> 1) | 0x8000;
		}
		tmp = tmp + seed + (data[i + 16] << 8) + data[i + 48];

		hash[i & 0x0F] ^= tmp >> 8;
		hash[(i + 1) & 0x0F] ^= tmp;
	}
}

static void PowervuCreateHash(uint8_t *data, int len, uint8_t *hash, int mode)
{
	uint8_t dataPadded[64];

	PowervuPadData(data, len, dataPadded);

	switch (mode)
	{
		case 1:
			PowervuHashMode01CustomMD5(dataPadded, hash);
			break;

		case 2:
			PowervuHashMode02(dataPadded, hash);
			break;

		case 3:
			PowervuHashMode03(dataPadded, hash);
			break;

		case 4:
			PowervuHashModes04to0ATables(dataPadded, hash, table04);
			break;

		case 5:
			PowervuHashModes04to0ATables(dataPadded, hash, table05);
			break;

		case 6:
			PowervuHashModes04to0ATables(dataPadded, hash, table06);
			break;

		case 7:
			PowervuHashModes04to0ATables(dataPadded, hash, table07);
			break;

		case 8:
			PowervuHashModes04to0ATables(dataPadded, hash, table08);
			break;

		case 9:
			PowervuHashModes04to0ATables(dataPadded, hash, table09);
			break;

		case 10:
			PowervuHashModes04to0ATables(dataPadded, hash, table0A);
			break;

		case 15:
			PowervuHashModes0Fto18Tables(dataPadded, hash, table0F);
			break;

		case 16:
			PowervuHashModes0Fto18Tables(dataPadded, hash, table10);
			break;

		case 17:
			PowervuHashModes0Fto18Tables(dataPadded, hash, table11);
			break;

		case 18:
			PowervuHashModes0Fto18Tables(dataPadded, hash, table12);
			break;

		case 19:
			PowervuHashModes0Fto18Tables(dataPadded, hash, table13);
			break;

		case 20:
			PowervuHashModes0Fto18Tables(dataPadded, hash, table14);
			break;

		case 21:
			PowervuHashModes0Fto18Tables(dataPadded, hash, table15);
			break;

		case 22:
			PowervuHashModes0Fto18Tables(dataPadded, hash, table16);
			break;

		case 23:
			PowervuHashModes0Fto18Tables(dataPadded, hash, table17);
			break;

		case 24:
			PowervuHashModes0Fto18Tables(dataPadded, hash, table18);
			break;

		case 25:
			PowervuHashModes19to27Tables3(dataPadded, hash, table19);
			break;

		default:
			cs_log("A new hash mode [%d] is in use.", mode);
			break;
	}
}

void PowervuCreateHashMode03(uint8_t *data, uint8_t *hash)
{
	int i, j, c;
	uint8_t buffer0[16], buffer1[8], buffer2[8], tmpBuff1[4], tmpBuff2[4];

	uint8_t table[] = { 0x68, 0xCE, 0xE7, 0x71, 0xCC, 0x3A, 0x0B, 0x6E,
						0x2A, 0x43, 0x17, 0x07, 0x5A, 0xD9, 0x14, 0x5B,
						0xB0, 0x8E, 0xA8, 0x7F, 0xD8, 0xA2, 0xCF, 0x73,
						0xC2, 0xB9, 0x5D, 0x46, 0xDD, 0x2C, 0xE2, 0x2D,
						0xFD, 0x50, 0xE9, 0x7C, 0x28, 0x72, 0x9B, 0xAA,
						0xEC, 0x24, 0x74, 0xAB, 0x00, 0x1C, 0x8B, 0x65,
						0x38, 0x13, 0x22, 0x82, 0xAC, 0x9A, 0x4D, 0x2B,
						0xEA, 0x04, 0x31, 0x84, 0x32, 0x3D, 0x36, 0x53,
						0x5F, 0x42, 0x96, 0xDE, 0x47, 0x08, 0x51, 0x4B,
						0x3E, 0xD1, 0x1E, 0x12, 0xD2, 0x1F, 0x7D, 0x26,
						0xCD, 0x57, 0x8C, 0xB6, 0xD3, 0xF8, 0x11, 0xAD,
						0x6A, 0x88, 0x95, 0x21, 0xE8, 0xBF, 0x6B, 0x27,
						0xBE, 0xA3, 0x33, 0xB8, 0x9E, 0xB3, 0x6C, 0xC3,
						0x06, 0xC7, 0x6F, 0x99, 0x97, 0xDA, 0x09, 0xAF,
						0xAE, 0xCB, 0x79, 0x37, 0x55, 0x85, 0x8D, 0x2F,
						0x8A, 0x70, 0xA1, 0x7A, 0x66, 0x29, 0x67, 0x0F,
						0xEB, 0x9C, 0xC8, 0xC4, 0xD6, 0x4C, 0xDF, 0x1A,
						0xC0, 0x01, 0x64, 0xBC, 0x4E, 0xE1, 0x54, 0xD7,
						0x4F, 0xB7, 0x5E, 0xCA, 0xF0, 0x91, 0xE4, 0x59,
						0x4A, 0xC6, 0x83, 0x8F, 0xBD, 0x61, 0xFF, 0x56,
						0x92, 0xF1, 0x5C, 0x77, 0xC9, 0x20, 0xF4, 0xE5,
						0x10, 0x69, 0x03, 0x1D, 0xD5, 0x45, 0xF6, 0x0E,
						0xEF, 0xA0, 0xE3, 0x58, 0xFC, 0xED, 0x80, 0x16,
						0xEE, 0xFA, 0x02, 0xF5, 0xB4, 0x0A, 0xE0, 0x0C,
						0xF7, 0xF9, 0xBA, 0x7E, 0x18, 0x78, 0x19, 0xB5,
						0x0D, 0x44, 0x34, 0xD4, 0xDC, 0x30, 0x6D, 0x3B,
						0x63, 0x41, 0x48, 0x40, 0xA7, 0xA5, 0xC5, 0x98,
						0x76, 0x3F, 0xC1, 0x25, 0x93, 0x49, 0xD0, 0x62,
						0x2E, 0x75, 0xDB, 0x94, 0xF3, 0x52, 0x05, 0x81,
						0xFB, 0xBB, 0xA6, 0x89, 0x39, 0xA4, 0xF2, 0xA9,
						0xFE, 0x60, 0x3C, 0x15, 0xB1, 0x35, 0x86, 0x9D,
						0x9F, 0x90, 0x1B, 0xE6, 0x7B, 0x23, 0x87, 0xB2 };

	for (i = 0; i < 4; i++)
	{
		buffer0[0 + i] = data[12 + i];
		buffer0[4 + i] = data[8 + i];
		buffer0[8 + i] = data[4 + i];
		buffer0[12 + i] = data[0 + i];
	}

	for (c = 0; c < 12; c++)
	{
		for (i = 0; i < 4; i++)
		{
			buffer1[0 + i] = buffer0[8 + i] ^ buffer0[12 + i];
			buffer1[4 + i] = buffer0[0 + i] ^ buffer0[4 + i];
		}

		for (i = 0; i < 8; i++)
		{
			buffer1[i] = table[buffer1[i] ^ data[16 + 16 * (c % 3) + i]];
		}

		for (j = 0; j < 8; j++)
		{
			buffer2[j] = 0;
			for (i = 0; i < 8; i++)
			{
				buffer2[j] ^= buffer1[i] * (j * i + 1);
			}
		}

		for (i = 0; i < 8; i++)
		{
			buffer2[i] = table[buffer2[i] ^ data[24 + 16 * (c % 3) + i]] ^ data[16 + 16 * (c % 3) + i];
		}

		for (i = 0; i < 4; i++)
		{
			buffer0[12 + i] ^= buffer2[0 + i];
			buffer0[8 + i] ^= buffer2[0 + i];
			buffer0[4 + i] ^= buffer2[4 + i];
			buffer0[0 + i] ^= buffer2[4 + i];
		}

		tmpBuff1[0] = buffer0[14];
		tmpBuff1[1] = buffer0[15];
		tmpBuff1[2] = buffer0[12] ^ buffer0[14];
		tmpBuff1[3] = buffer0[13] ^ buffer0[15];

		tmpBuff2[0] = buffer0[6];
		tmpBuff2[1] = buffer0[7];
		tmpBuff2[2] = buffer0[4] ^ buffer0[6];
		tmpBuff2[3] = buffer0[5] ^ buffer0[7];

		for (i = 0; i < 4; i++)
		{
			buffer0[12 + i] = tmpBuff1[i];
			buffer0[4 + i] = tmpBuff2[i];
		}
	}

	for (i = 0; i < 4; i++)
	{
		hash[0 + i] = buffer0[12 + i] ^ data[0 + i];
		hash[4 + i] = buffer0[8 + i] ^ data[4 + i];
		hash[8 + i] = buffer0[4 + i] ^ data[8 + i];
		hash[12 + i] = buffer0[0 + i] ^ data[12 + i];
	}
}

void PowervuCreateDataCwMode03(uint8_t *seed, int lenSeed, uint8_t *basecw, uint8_t val,
								uint8_t *ecmBody, uint8_t *data)
{
	uint8_t padding[] = { 0x4A, 0x56, 0x7F, 0x16, 0xFC, 0x1F, 0x5B, 0x95,
						  0x19, 0xEF, 0x75, 0x14, 0x0E, 0x9E, 0x17, 0x3C,
						  0xF5, 0xB7, 0xA0, 0x93, 0xA3, 0x0F, 0xFA, 0x38,
						  0x7A, 0x34, 0x6C, 0xDC, 0xFB, 0xB0, 0x24, 0x42,
						  0x74, 0x72, 0x1C, 0xDC, 0x1E, 0xA1, 0x6D, 0xAB,
						  0xC8, 0x44, 0x53, 0xEF, 0x56, 0x00, 0xE9, 0x97,
						  0x48, 0x77, 0xF8, 0x00, 0x8E, 0x0B, 0x78, 0xA2 };

	memcpy(data + 8, padding, 56);

	data[0] = ecmBody[0x0F];
	data[1] = ecmBody[0x09];
	data[2] = ecmBody[0x10];
	data[3] = ecmBody[0x11];
	data[4] = ecmBody[0x05];
	data[5] = ecmBody[0x07];
	data[6] = ecmBody[0x08];
	data[7] = ecmBody[0x0A];

	int idxData = 8, idxSeed = 0, idxBase = 0;
	while (idxBase < 7)
	{
		if ((idxBase == 0) || (idxBase == 2) || (idxBase == 5)) data[idxData++] = val;
		if (idxSeed < lenSeed) data[idxData++] = seed[idxSeed++];
		data[idxData++] = basecw[idxBase++];
	}
}

void PowervuCreateDataUnmaskMode03(uint8_t *ecmBody, uint8_t *data)
{
	uint8_t padding[] = { 0xB1, 0x7C, 0xD2, 0xA7, 0x5E, 0x45, 0x6C, 0x36,
						  0xF0, 0xB6, 0x81, 0xF3, 0x25, 0x06, 0x65, 0x06,
						  0x6B, 0xBF, 0x4C, 0xE7, 0xED, 0x6E, 0x85, 0x00,
						  0xCC, 0xF2, 0x61, 0x48, 0x62, 0x24, 0x0E, 0x3C,
						  0x05, 0x89, 0xA5, 0x39, 0x5A, 0x4E, 0x9B, 0xC8,
						  0x14, 0x78, 0xEA, 0xB6, 0xFB, 0xF8, 0x10, 0xE6,
						  0x61, 0xF5, 0x3A, 0xBC, 0x5B, 0x79, 0x09, 0x97 };

	memcpy(data + 8, padding, 56);

	data[0] = ecmBody[0x17];
	data[1] = ecmBody[0x26];
	data[2] = ecmBody[0x19];
	data[3] = ecmBody[0x21];
	data[4] = ecmBody[0x26];
	data[5] = ecmBody[0x31];
	data[6] = ecmBody[0x21];
	data[7] = ecmBody[0x27];
}

uint8_t PowervuGetModeCW(uint8_t *extraData)
{
	uint64_t data = ((uint32_t)extraData[0] << 24) + (extraData[1] << 16) + (extraData[2] << 8) + extraData[3];
	uint64_t t1 = (data * 0x76E9DEA7) >> 50;
	uint64_t t2 = (t1 * 0x51EB851F) >> 36;
	uint64_t t3 = t2 * 0x32;
	uint8_t r = t1 - t3;
	return r;
}

uint8_t PowervuGetModeUnmask(uint8_t *extraData)
{
	uint64_t data = ((uint32_t)extraData[0] << 24) + (extraData[1] << 16) + (extraData[2] << 8) + extraData[3];
	uint64_t t1 = (data * 0xB9CD6BE5) >> 45;
	uint64_t t2 = (t1 * 0x51EB851F) >> 36;
	uint64_t t3 = t2 * 0x32;
	uint8_t r = t1 - t3;
	return r;
}

static void PowervuCreateDataEcmEmm(uint8_t *emmEcm, uint8_t *pos, int lenHeader, int len, uint8_t *data)
{
	int i;

	for (i = 0; i < len; i++)
	{
		data[i] = emmEcm[lenHeader + pos[i]];
	}
}

static uint8_t PowervuCreateDataCw(uint8_t *seed, uint8_t lenSeed, uint8_t *baseCw, uint8_t val,
									uint8_t *seedEcmCw, uint8_t *data)
{
	int i;

	for (i = 0; i < lenSeed; i++)
	{
		data[i] = seed[i];
	}

	for (i = 0; i < 7; i++)
	{
		data[lenSeed + i] = baseCw[i];
	}

	data[lenSeed + 7] = val;

	for (i = 0; i < 16; i++)
	{
		data[lenSeed + 7 + 1 + i] = seedEcmCw[i];
	}

	return lenSeed + 7 + 1 + 0x10;
}

static uint8_t PowervuUnmaskEcm(uint8_t *ecm, uint8_t *seedEcmCw, uint8_t *modeCW)
{
	int i, l;
	uint8_t data[64], mask[16];
	uint8_t hashModeEcm, hashModeCw, modeUnmask = 0;
	uint32_t crc;

	uint8_t sourcePos[] = { 0x04, 0x05, 0x06, 0x07, 0x0A, 0x0B, 0x0C, 0x0D,
							0x0E, 0x0F, 0x10, 0x17, 0x1C, 0x1D, 0x1F, 0x23,
							0x24, 0x25, 0x26, 0x27, 0x29, 0x2C, 0x2D, 0x2E };

	uint8_t destPos[] = { 0x08, 0x09, 0x11, 0x18, 0x19, 0x1A, 0x1B, 0x1E,
						  0x20, 0x21, 0x22, 0x28, 0x2A, 0x2B, 0x2F, 0x30 };

	uint8_t seedCwPos[] = {0x07, 0x0A, 0x04, 0x0D, 0x05, 0x0E, 0x06, 0x0B, 0x10, 0x0C, 0x0F};

	// Create seed for CW decryption
	memset(seedEcmCw, 0, 16);

	int extraBytesLen = ecm[9];
	int startOffset = extraBytesLen + 10;

	for (i = 0; i < 11; i++)
	{
		seedEcmCw[i] = ecm[startOffset + seedCwPos[i]];
	}

	*modeCW = 0;
	if (extraBytesLen > 0)
	{
		*modeCW = PowervuGetModeCW(ecm + 10);
	}

	// Read hash mode CW
	hashModeCw = ecm[28 + extraBytesLen] ^ PowervuCrc8Calc(seedEcmCw, 16);

	// Create mask for ECM decryption
	PowervuCreateDataEcmEmm(ecm, sourcePos, startOffset, 24, data);

	hashModeEcm = ecm[8] ^ PowervuCrc8Calc(data, 24);

	if (extraBytesLen > 0)
	{
		modeUnmask = PowervuGetModeUnmask(ecm + 10);
	}

	if (modeUnmask == 0x03)
	{
		ecm[startOffset + 0x21] -= ecm[startOffset + 0x07];
		ecm[startOffset + 0x26] -= ecm[startOffset + 0x05];
		ecm[startOffset + 0x26] -= ecm[startOffset + 0x08];
		ecm[startOffset + 0x19] -= ecm[startOffset + 0x06];
		ecm[startOffset + 0x31] -= ecm[startOffset + 0x09];
		ecm[startOffset + 0x27] -= ecm[startOffset + 0x0C];
		ecm[startOffset + 0x21] -= ecm[startOffset + 0x0B];
		ecm[startOffset + 0x17] -= ecm[startOffset + 0x04];

		PowervuCreateDataUnmaskMode03(ecm + startOffset, data);
		PowervuCreateHashMode03(data, mask);

		// Unmask body
		ecm[startOffset + 0x06] ^= mask[0x02];
		ecm[startOffset + 0x0B] ^= mask[0x06];
		ecm[startOffset + 0x0C] ^= mask[0x07];
		ecm[startOffset + 0x0D] ^= mask[0x08];
		ecm[startOffset + 0x0E] ^= mask[0x09];
		ecm[startOffset + 0x0F] ^= mask[0x0A];
		ecm[startOffset + 0x11] ^= mask[0x0B];
		ecm[startOffset + 0x18] ^= mask[0x0C];
		ecm[startOffset + 0x2D] ^= mask[0x0A];
		ecm[startOffset + 0x07] ^= mask[0x03];
		ecm[startOffset + 0x1B] ^= mask[0x0D];
		ecm[startOffset + 0x30] ^= mask[0x0C];
		ecm[startOffset + 0x1C] ^= mask[0x0E];
		ecm[startOffset + 0x1E] ^= mask[0x00];
		ecm[startOffset + 0x04] ^= mask[0x00];
		ecm[startOffset + 0x05] ^= mask[0x01];
		ecm[startOffset + 0x1F] ^= mask[0x01];
		ecm[startOffset + 0x2C] ^= mask[0x09];
		ecm[startOffset + 0x20] ^= mask[0x02];
		ecm[startOffset + 0x1D] ^= mask[0x0F];
		ecm[startOffset + 0x23] ^= mask[0x04];
		ecm[startOffset + 0x09] ^= mask[0x05];
		ecm[startOffset + 0x22] ^= mask[0x03];
		ecm[startOffset + 0x24] ^= mask[0x05];
		ecm[startOffset + 0x08] ^= mask[0x04];
		ecm[startOffset + 0x28] ^= mask[0x06];
		ecm[startOffset + 0x29] ^= mask[0x07];
		ecm[startOffset + 0x2A] ^= mask[0x08];
		ecm[startOffset + 0x2E] ^= mask[0x0B];

		for (i = 0; i < ecm[9]; i++)
		{
			ecm[10 + i] = 0x00;
		}
	}
	else if (modeUnmask == 0x04)
	{
		// Do nothing
	}
	else
	{
		PowervuCreateHash(data, 24, mask, hashModeEcm);
		// Unmask body
		for (i = 0; i < 16; i++)
		{
			ecm[startOffset + destPos[i]] ^= mask[i & 0x0F];
		}
	}

	// Fix header
	ecm[3] &= 0x0F;
	ecm[3] |= 0x30;
	ecm[8]  = 0x00;
	ecm[28 + extraBytesLen] = 0x00;

	// Fix CRC (optional)
	l = (((ecm[1] << 8) + ecm[2]) & 0xFFF) + 3 - 4;

	crc = fletcher_crc32(ecm, l);

	ecm[l + 0] = crc >> 24;
	ecm[l + 1] = crc >> 16;
	ecm[l + 2] = crc >> 8;
	ecm[l + 3] = crc >> 0;

	for (i = 0; i < 11; i++)
	{
		seedEcmCw[i] = ecm[startOffset + seedCwPos[i]];
	}

	return hashModeCw;
}

static void PowervuCreateCw(uint8_t *seed, uint8_t lenSeed, uint8_t *baseCw, uint8_t val, uint8_t *seedEcmCw,
							uint8_t *cw, int modeDesCsa, int hashMode, int modeCW, uint8_t* ecmBody)
{
	uint8_t data[64], hash[16], lenData;
	int i;

	uint8_t tableFixParity[] = { 0x01, 0x01, 0x02, 0x02, 0x04, 0x04, 0x07, 0x07,
								 0x08, 0x08, 0x0B, 0x0B, 0x0D, 0x0D, 0x0E, 0x0E,
								 0x10, 0x10, 0x13, 0x13, 0x15, 0x15, 0x16, 0x16,
								 0x19, 0x19, 0x1A, 0x1A, 0x1C, 0x1C, 0x1F, 0x1F,
								 0x20, 0x20, 0x23, 0x23, 0x25, 0x25, 0x26, 0x26,
								 0x29, 0x29, 0x2A, 0x2A, 0x2C, 0x2C, 0x2F, 0x2F,
								 0x31, 0x31, 0x32, 0x32, 0x34, 0x34, 0x37, 0x37,
								 0x38, 0x38, 0x3B, 0x3B, 0x3D, 0x3D, 0x3E, 0x3E,
								 0x40, 0x40, 0x43, 0x43, 0x45, 0x45, 0x46, 0x46,
								 0x49, 0x49, 0x4A, 0x4A, 0x4C, 0x4C, 0x4F, 0x4F,
								 0x51, 0x51, 0x52, 0x52, 0x54, 0x54, 0x57, 0x57,
								 0x58, 0x58, 0x5B, 0x5B, 0x5D, 0x5D, 0x5E, 0x5E,
								 0x61, 0x61, 0x62, 0x62, 0x64, 0x64, 0x67, 0x67,
								 0x68, 0x68, 0x6B, 0x6B, 0x6D, 0x6D, 0x6E, 0x6E,
								 0x70, 0x70, 0x73, 0x73, 0x75, 0x75, 0x76, 0x76,
								 0x79, 0x79, 0x7A, 0x7A, 0x7C, 0x7C, 0x7F, 0x7F,
								 0x80, 0x80, 0x83, 0x83, 0x85, 0x85, 0x86, 0x86,
								 0x89, 0x89, 0x8A, 0x8A, 0x8C, 0x8C, 0x8F, 0x8F,
								 0x91, 0x91, 0x92, 0x92, 0x94, 0x94, 0x97, 0x97,
								 0x98, 0x98, 0x9B, 0x9B, 0x9D, 0x9D, 0x9E, 0x9E,
								 0xA1, 0xA1, 0xA2, 0xA2, 0xA4, 0xA4, 0xA7, 0xA7,
								 0xA8, 0xA8, 0xAB, 0xAB, 0xAD, 0xAD, 0xAE, 0xAE,
								 0xB0, 0xB0, 0xB3, 0xB3, 0xB5, 0xB5, 0xB6, 0xB6,
								 0xB9, 0xB9, 0xBA, 0xBA, 0xBC, 0xBC, 0xBF, 0xBF,
								 0xC1, 0xC1, 0xC2, 0xC2, 0xC4, 0xC4, 0xC7, 0xC7,
								 0xC8, 0xC8, 0xCB, 0xCB, 0xCD, 0xCD, 0xCE, 0xCE,
								 0xD0, 0xD0, 0xD3, 0xD3, 0xD5, 0xD5, 0xD6, 0xD6,
								 0xD9, 0xD9, 0xDA, 0xDA, 0xDC, 0xDC, 0xDF, 0xDF,
								 0xE0, 0xE0, 0xE3, 0xE3, 0xE5, 0xE5, 0xE6, 0xE6,
								 0xE9, 0xE9, 0xEA, 0xEA, 0xEC, 0xEC, 0xEF, 0xEF,
								 0xF1, 0xF1, 0xF2, 0xF2, 0xF4, 0xF4, 0xF7, 0xF7,
								 0xF8, 0xF8, 0xFB, 0xFB, 0xFD, 0xFD, 0xFE, 0xFE };

	if (modeCW == 0x03)
	{
		PowervuCreateDataCwMode03(seed, lenSeed, baseCw, val, ecmBody, data);
		PowervuCreateHashMode03(data, hash);

		cw[0] = hash[0x09];
		cw[1] = hash[0x01];
		cw[2] = hash[0x0F];
		cw[3] = hash[0x0E];
		cw[4] = hash[0x04];
		cw[5] = hash[0x02];
		cw[6] = hash[0x05];
		cw[7] = hash[0x0D];
	}
	else if (modeCW == 0x04)
	{
		// Do nothing
	}
	else
	{
		lenData = PowervuCreateDataCw(seed, lenSeed, baseCw, val, seedEcmCw, data);
		PowervuCreateHash(data, lenData, hash, hashMode);

		for (i = 0; i < 8; i++)
		{
			cw[i] = hash[i];
		}
	}

	if (modeDesCsa == 0) // DES - Fix Parity Bits
	{
		for (i = 0; i < 8; i++)
		{
			cw[i] = tableFixParity[cw[i]];
		}
	}
	else if (modeDesCsa == 1) // CSA - Fix Checksums
	{
		cw[3] = cw[0] + cw[1] + cw[2];
		cw[7] = cw[4] + cw[5] + cw[6];
	}
}

static inline int8_t PowervuGetEcmKey(uint8_t *key, uint16_t srvid, uint8_t keyIndex, uint32_t keyRef)
{
	return FindKey('P', srvid, 0xFFFF0000, keyIndex == 1 ? "01" : "00", key, 7, 0, keyRef, 0, NULL);
}

static inline int8_t PowervuGetEmmKey(uint8_t *key, char *uniqueAddress, uint32_t keyRef, uint32_t *groupId)
{
	return FindKey('P', 0, 0xFFFFFFFF, uniqueAddress, key, 7, 0, keyRef, 0, groupId);
}

static const uint8_t PowerVu_A0_S_1[16] = { 0x33, 0xA4, 0x44, 0x3C, 0xCA, 0x2E, 0x75, 0x7B,
											0xBC, 0xE6, 0xE5, 0x35, 0xA0, 0x55, 0xC9, 0xA2 };

static const uint8_t PowerVu_A0_S_2[16] = { 0x5A, 0xB0, 0x2C, 0xBC, 0xDA, 0x32, 0xE6, 0x92,
											0x40, 0x53, 0x6E, 0xF9, 0x69, 0x11, 0x1E, 0xFB };

static const uint8_t PowerVu_A0_S_3[16] = { 0x4E, 0x18, 0x9B, 0x19, 0x79, 0xFB, 0x01, 0xFA,
											0xE3, 0xE1, 0x28, 0x3D, 0x32, 0xE4, 0x92, 0xEA };

static const uint8_t PowerVu_A0_S_4[16] = { 0x05, 0x6F, 0x37, 0x66, 0x35, 0xE1, 0x58, 0xD0,
											0xB4, 0x6A, 0x97, 0xAE, 0xD8, 0x91, 0x27, 0x56 };

static const uint8_t PowerVu_A0_S_5[16] = { 0x7B, 0x26, 0xAD, 0x34, 0x3D, 0x77, 0x39, 0x51,
											0xE0, 0xE0, 0x48, 0x8C, 0x39, 0xF5, 0xE8, 0x47 };

static const uint8_t PowerVu_A0_S_6[16] = { 0x74, 0xFA, 0x4D, 0x79, 0x42, 0x39, 0xD1, 0xA4,
											0x99, 0xA3, 0x97, 0x07, 0xDF, 0x14, 0x3A, 0xC4 };

static const uint8_t PowerVu_A0_S_7[16] = { 0xC6, 0x1E, 0x3C, 0x24, 0x11, 0x08, 0x5D, 0x6A,
											0xEB, 0x97, 0xB9, 0x25, 0xA7, 0xFA, 0xE9, 0x1A };

static const uint8_t PowerVu_A0_S_8[16] = { 0x9A, 0xAD, 0x72, 0xD7, 0x7C, 0x68, 0x3B, 0x55,
											0x1D, 0x4A, 0xA2, 0xB0, 0x38, 0xB9, 0x56, 0xD0 };

static const uint8_t PowerVu_A0_S_9[32] = { 0x61, 0xDA, 0x5F, 0xB7, 0xEB, 0xC6, 0x3F, 0x6C,
											0x09, 0xF3, 0x64, 0x38, 0x33, 0x08, 0xAA, 0x15,
											0xCC, 0xEF, 0x22, 0x64, 0x01, 0x2C, 0x12, 0xDE,
											0xF4, 0x6E, 0x3C, 0xCD, 0x1A, 0x64, 0x63, 0x7C };

static const uint8_t PowerVu_00_S_1[16] = { 0x97, 0x13, 0xEB, 0x6B, 0x04, 0x5E, 0x60, 0x3A,
											0xD9, 0xCC, 0x91, 0xC2, 0x5A, 0xFD, 0xBA, 0x0C };

static const uint8_t PowerVu_00_S_2[16] = { 0x61, 0x3C, 0x03, 0xB0, 0xB5, 0x6F, 0xF8, 0x01,
											0xED, 0xE0, 0xE5, 0xF3, 0x78, 0x0F, 0x0A, 0x73 };

static const uint8_t PowerVu_00_S_3[16] = { 0xFD, 0xDF, 0xD2, 0x97, 0x06, 0x14, 0x91, 0xB5,
											0x36, 0xAD, 0xBC, 0xE1, 0xB3, 0x00, 0x66, 0x41 };

static const uint8_t PowerVu_00_S_4[16] = { 0x8B, 0xD9, 0x18, 0x0A, 0xED, 0xEE, 0x61, 0x34,
											0x1A, 0x79, 0x80, 0x8C, 0x1E, 0x7F, 0xC5, 0x9F };

static const uint8_t PowerVu_00_S_5[16] = { 0xB0, 0xA1, 0xF2, 0xB8, 0xEA, 0x72, 0xDD, 0xD3,
											0x30, 0x65, 0x2B, 0x1E, 0xE9, 0xE1, 0x45, 0x29 };

static const uint8_t PowerVu_00_S_6[16] = { 0x5D, 0xCA, 0x53, 0x75, 0xB2, 0x24, 0xCE, 0xAF,
											0x21, 0x54, 0x9E, 0xBE, 0x02, 0xA9, 0x4C, 0x5D };

static const uint8_t PowerVu_00_S_7[16] = { 0x42, 0x66, 0x72, 0x83, 0x1B, 0x2D, 0x22, 0xC9,
											0xF8, 0x4D, 0xBA, 0xCD, 0xBB, 0x20, 0xBD, 0x6B };

static const uint8_t PowerVu_00_S_8[16] = { 0xC4, 0x0C, 0x6B, 0xD3, 0x6D, 0x94, 0x7E, 0x53,
											0xCE, 0x96, 0xAC, 0x40, 0x2C, 0x7A, 0xD3, 0xA9 };

static const uint8_t PowerVu_00_S_9[32] = { 0x31, 0x82, 0x4F, 0x9B, 0xCB, 0x6F, 0x9D, 0xB7,
											0xAE, 0x68, 0x0B, 0xA0, 0x93, 0x15, 0x32, 0xE2,
											0xED, 0xE9, 0x47, 0x29, 0xC2, 0xA8, 0x92, 0xEF,
											0xBA, 0x27, 0x22, 0x57, 0x76, 0x54, 0xC0, 0x59 };

static uint8_t PowervuSbox(uint8_t *input, uint8_t mode)
{
	uint8_t s_index, bit, last_index, last_bit;
	uint8_t const *Sbox1, *Sbox2, *Sbox3, *Sbox4, *Sbox5, *Sbox6, *Sbox7, *Sbox8, *Sbox9;

	if (mode)
	{
		Sbox1 = PowerVu_A0_S_1;
		Sbox2 = PowerVu_A0_S_2;
		Sbox3 = PowerVu_A0_S_3;
		Sbox4 = PowerVu_A0_S_4;
		Sbox5 = PowerVu_A0_S_5;
		Sbox6 = PowerVu_A0_S_6;
		Sbox7 = PowerVu_A0_S_7;
		Sbox8 = PowerVu_A0_S_8;
		Sbox9 = PowerVu_A0_S_9;
	}
	else
	{
		Sbox1 = PowerVu_00_S_1;
		Sbox2 = PowerVu_00_S_2;
		Sbox3 = PowerVu_00_S_3;
		Sbox4 = PowerVu_00_S_4;
		Sbox5 = PowerVu_00_S_5;
		Sbox6 = PowerVu_00_S_6;
		Sbox7 = PowerVu_00_S_7;
		Sbox8 = PowerVu_00_S_8;
		Sbox9 = PowerVu_00_S_9;
	}

	bit = (GetBit(input[2], 0) << 2) | (GetBit(input[3], 4) << 1) | (GetBit(input[5], 3));
	s_index = (GetBit(input[0], 0) << 3) | (GetBit(input[2], 6) << 2) | (GetBit(input[2], 4) << 1) | (GetBit(input[5], 7));
	last_bit = GetBit(Sbox1[s_index], 7 - bit);

	bit = (GetBit(input[5], 0) << 2) | (GetBit(input[4], 0) << 1) | (GetBit(input[6], 2));
	s_index = (GetBit(input[2], 1) << 3) | (GetBit(input[2], 2) << 2) | (GetBit(input[5], 5) << 1) | (GetBit(input[5], 1));
	last_bit = last_bit | (GetBit(Sbox2[s_index], 7 - bit) << 1);

	bit = (GetBit(input[6], 0) << 2) | (GetBit(input[1], 7) << 1) | (GetBit(input[6], 7));
	s_index = (GetBit(input[1], 3) << 3) | (GetBit(input[3], 7) << 2) | (GetBit(input[1], 5) << 1) | (GetBit(input[5], 2));
	last_bit = last_bit | (GetBit(Sbox3[s_index], 7 - bit) << 2);

	bit = (GetBit(input[1], 0) << 2) | (GetBit(input[2], 7) << 1) | (GetBit(input[2], 5));
	s_index = (GetBit(input[6], 3) << 3) | (GetBit(input[6], 4) << 2) | (GetBit(input[6], 6) << 1) | (GetBit(input[3], 5));
	last_index = GetBit(Sbox4[s_index], 7 - bit);

	bit = (GetBit(input[3], 3) << 2) | (GetBit(input[4], 6) << 1) | (GetBit(input[3], 2));
	s_index = (GetBit(input[3], 1) << 3) | (GetBit(input[4], 5) << 2) | (GetBit(input[3], 0) << 1) | (GetBit(input[4], 7));
	last_index = last_index | (GetBit(Sbox5[s_index], 7 - bit) << 1);

	bit = (GetBit(input[5], 4) << 2) | (GetBit(input[4], 4) << 1) | (GetBit(input[1], 2));
	s_index = (GetBit(input[2], 3) << 3) | (GetBit(input[6], 5) << 2) | (GetBit(input[1], 4) << 1) | (GetBit(input[4], 1));
	last_index = last_index | (GetBit(Sbox6[s_index], 7 - bit) << 2);

	bit = (GetBit(input[0], 6) << 2) | (GetBit(input[0], 7) << 1) | (GetBit(input[0], 4));
	s_index = (GetBit(input[0], 5) << 3) | (GetBit(input[0], 3) << 2) | (GetBit(input[0], 1) << 1) | (GetBit(input[0], 2));
	last_index = last_index | (GetBit(Sbox7[s_index], 7 - bit) << 3);

	bit = (GetBit(input[4], 2) << 2) | (GetBit(input[4], 3) << 1) | (GetBit(input[1], 1));
	s_index = (GetBit(input[1], 6) << 3) | (GetBit(input[6], 1) << 2) | (GetBit(input[5], 6) << 1) | (GetBit(input[3], 6));
	last_index = last_index | (GetBit(Sbox8[s_index], 7 - bit) << 4);

	return (GetBit(Sbox9[last_index & 0x1F], 7 - last_bit) & 1) ? 1 : 0;
}

static void PowervuDecrypt(uint8_t *data, uint32_t length, uint8_t *key, uint8_t sbox)
{
	uint32_t i;
	int32_t j, k;
	uint8_t curByte, tmpBit;

	for (i = 0; i < length; i++)
	{
		curByte = data[i];

		for (j = 7; j >= 0; j--)
		{
			data[i] = SetBit(data[i], j, (GetBit(curByte, j) ^ PowervuSbox(key, sbox)) ^ GetBit(key[0], 7));
			tmpBit = GetBit(data[i], j) ^ (GetBit(key[6], 0));

			if (tmpBit)
			{
				key[3] ^= 0x10;
			}

			for (k = 6; k > 0; k--)
			{
				key[k] = (key[k] >> 1) | (key[k - 1] << 7);
			}

			key[0] = (key[0] >> 1);
			key[0] = SetBit(key[0], 7, tmpBit);
		}
	}
}

static void ExpandDesKey(unsigned char *key)
{
	uint8_t i, j, parity;
	uint8_t tmpKey[7];

	memcpy(tmpKey, key, 7);

	key[0] = (tmpKey[0] & 0xFE);
	key[1] = ((tmpKey[0] << 7) | ((tmpKey[1] >> 1) & 0xFE));
	key[2] = ((tmpKey[1] << 6) | ((tmpKey[2] >> 2) & 0xFE));
	key[3] = ((tmpKey[2] << 5) | ((tmpKey[3] >> 3) & 0xFE));
	key[4] = ((tmpKey[3] << 4) | ((tmpKey[4] >> 4) & 0xFE));
	key[5] = ((tmpKey[4] << 3) | ((tmpKey[5] >> 5) & 0xFE));
	key[6] = ((tmpKey[5] << 2) | ((tmpKey[6] >> 6) & 0xFE));
	key[7] = (tmpKey[6] << 1);

	for (i = 0; i < 8; i++)
	{
		parity = 1;
		for (j = 1; j < 8; j++)
		{
			if ((key[i] >> j) & 0x1)
			{
				parity = ~parity & 0x01;
			}
		}
		key[i] |= parity;
	}
}

static uint8_t PowervuGetConvcwIndex(uint8_t ecmTag)
{
	switch (ecmTag)
	{
		case PVU_CONVCW_VID_ECM:
			return PVU_CW_VID;

		case PVU_CONVCW_HSD_ECM:
			return PVU_CW_HSD;

		case PVU_CONVCW_A1_ECM:
			return PVU_CW_A1;

		case PVU_CONVCW_A2_ECM:
			return PVU_CW_A2;

		case PVU_CONVCW_A3_ECM:
			return PVU_CW_A3;

		case PVU_CONVCW_A4_ECM:
			return PVU_CW_A4;

		case PVU_CONVCW_UTL_ECM:
			return PVU_CW_UTL;

		case PVU_CONVCW_VBI_ECM:
			return PVU_CW_VBI;

		default:
			return PVU_CW_VBI;
	}
}

static uint16_t PowervuGetSeedIV(uint8_t seedType, uint8_t *ecm)
{
	switch (seedType)
	{
		case PVU_CW_VID:
			return ((ecm[0x10] & 0x1F) << 3) | 0;
		case PVU_CW_HSD:
			return ((ecm[0x12] & 0x1F) << 3) | 2;
		case PVU_CW_A1:
			return ((ecm[0x11] & 0x3F) << 3) | 1;
		case PVU_CW_A2:
			return ((ecm[0x13] & 0x3F) << 3) | 1;
		case PVU_CW_A3:
			return ((ecm[0x19] & 0x3F) << 3) | 1;
		case PVU_CW_A4:
			return ((ecm[0x1A] & 0x3F) << 3) | 1;;
		case PVU_CW_UTL:
			return ((ecm[0x14] & 0x0F) << 3) | 4;
		case PVU_CW_VBI:
			return (((ecm[0x15] & 0xF8) >> 3) << 3) | 5;
		default:
			return 0;
	}
}

static uint8_t PowervuExpandSeed(uint8_t seedType, uint8_t *seed)
{
	uint8_t seedLength = 0, i;

	switch (seedType)
	{
		case PVU_CW_VID:
		case PVU_CW_HSD:
			seedLength = 4;
			break;
		case PVU_CW_A1:
		case PVU_CW_A2:
		case PVU_CW_A3:
		case PVU_CW_A4:
			seedLength = 3;
			break;
		case PVU_CW_UTL:
		case PVU_CW_VBI:
			seedLength = 2;
			break;
		default:
			return seedLength;
	}

	for (i = seedLength; i < 7; i++)
	{
		seed[i] = seed[i % seedLength];
	}

	return seedLength;
}

static void PowervuCalculateSeed(uint8_t seedType, uint8_t *ecm, uint8_t *seedBase, uint8_t *key,
									uint8_t *seed, uint8_t sbox)
{
	uint16_t tmpSeed;

	tmpSeed = PowervuGetSeedIV(seedType, ecm + 23);

	seed[0] = (tmpSeed >> 2) & 0xFF;
	seed[1] = ((tmpSeed & 0x3) << 6) | (seedBase[0] >> 2);
	seed[2] = (    seedBase[0] << 6) | (seedBase[1] >> 2);
	seed[3] = (    seedBase[1] << 6) | (seedBase[2] >> 2);
	seed[4] = (    seedBase[2] << 6) | (seedBase[3] >> 2);
	seed[5] = (    seedBase[3] << 6);

	PowervuDecrypt(seed, 6, key, sbox);

	seed[0] = (seed[1] << 2) | (seed[2] >> 6);
	seed[1] = (seed[2] << 2) | (seed[3] >> 6);
	seed[2] = (seed[3] << 2) | (seed[4] >> 6);
	seed[3] = (seed[4] << 2) | (seed[5] >> 6);
}

static void PowervuCalculateCw(uint8_t seedType, uint8_t *seed, uint8_t csaUsed, uint8_t *convolvedCw,
								uint8_t *cw, uint8_t *baseCw, uint8_t *seedEcmCw, uint8_t hashModeCw,
								uint8_t needsUnmasking, uint8_t xorMode, int modeCW, uint8_t* ecmBody)
{
	int32_t k;
	uint8_t seedLength, val = 0;

	seedLength = PowervuExpandSeed(seedType, seed);

	if (needsUnmasking && (((modeCW >= 0x00) && (hashModeCw > 0) && (hashModeCw <= 0x27) &&
		(hashModeCw != 0x0B) && (hashModeCw != 0x0C) && (hashModeCw != 0x0D) && (hashModeCw != 0x0E)) ||
		(modeCW == 0x03) || (modeCW == 0x04)))
	{
		switch (seedType)
		{
			case PVU_CW_VID:
				val = 0;
				break;

			case PVU_CW_A1:
			case PVU_CW_A2:
			case PVU_CW_A3:
			case PVU_CW_A4:
				val = 1;
				break;

			case PVU_CW_HSD:
				val = 2;
				break;

			case PVU_CW_UTL:
				val = 4;
				break;

			case PVU_CW_VBI:
				val = 5;
				break;
		}

		PowervuCreateCw(seed, seedLength, baseCw, val, seedEcmCw, cw, csaUsed, hashModeCw, modeCW, ecmBody);

		if (csaUsed)
		{
			cw[0] = cw[0] ^ convolvedCw[0];
			cw[1] = cw[1] ^ convolvedCw[1];
			cw[2] = cw[2] ^ convolvedCw[2];
			cw[3] = cw[3] ^ convolvedCw[3];
			cw[4] = cw[4] ^ convolvedCw[4];
			cw[5] = cw[5] ^ convolvedCw[5];
			cw[6] = cw[6] ^ convolvedCw[6];
			cw[7] = cw[7] ^ convolvedCw[7];

			cw[3] = cw[0] + cw[1] + cw[2];
			cw[7] = cw[4] + cw[5] + cw[6];
		}
	}
	else
	{
		if (csaUsed)
		{
			for (k = 0; k < 7; k++)
			{
				seed[k] ^= baseCw[k];
			}

			cw[0] = seed[0] ^ convolvedCw[0];
			cw[1] = seed[1] ^ convolvedCw[1];
			cw[2] = seed[2] ^ convolvedCw[2];
			cw[3] = seed[3] ^ convolvedCw[3];
			cw[4] = seed[3] ^ convolvedCw[4];
			cw[5] = seed[4] ^ convolvedCw[5];
			cw[6] = seed[5] ^ convolvedCw[6];
			cw[7] = seed[6] ^ convolvedCw[7];
		}
		else
		{
			if (xorMode == 0)
			{
				for (k = 0; k < 7; k++)
				{
					cw[k] = seed[k] ^ baseCw[k];
				}
			}

			if (xorMode == 1)
			{
				for (k = 0; k < 3; k++)
				{
					cw[k] = seed[k] ^ baseCw[k];
				}

				for (k = 3; k < 7; k++)
				{
					cw[k] = baseCw[k];
				}
			}

			ExpandDesKey(cw);
		}
	}
}

int8_t PowervuECM(uint8_t *ecm, uint8_t *dw, uint16_t srvid, emu_stream_client_key_data *cdata, EXTENDED_CW* cw_ex)
{
	uint32_t i, j, k;
	uint32_t ecmCrc32, keyRef1, keyRef2;

	uint16_t ecmLen = GetEcmLen(ecm);
	uint16_t nanoLen, channelId, ecmSrvid;

	uint8_t keyIndex, sbox, decrypt_ok, calculateAllCws, hashModeCw = 0, needsUnmasking, xorMode;
	uint8_t nanoCmd, nanoChecksum, keyType, fixedKey, oddKey, bid, csaUsed, modeCW = 0, offsetBody;

	uint8_t ecmKey[7], tmpEcmKey[7], seedBase[4], baseCw[7], seed[8][8], cw[8][8], convolvedCw[8][8];
	uint8_t ecmPart1[14], ecmPart2[27], unmaskedEcm[ecmLen], seedEcmCw[16];
	uint8_t *dwp;

	char tmpBuffer1[64];

	emu_stream_cw_item *cw_item;
	int8_t update_global_key = 0, ret = 1;
	int8_t update_global_keys[EMU_STREAM_SERVER_MAX_CONNECTIONS];

	memset(update_global_keys, 0, sizeof(update_global_keys));

	if (ecmLen < 7)
	{
		return EMU_NOT_SUPPORTED;
	}

	needsUnmasking = (ecm[3] & 0xF0) == 0x50;

	//cs_log_dbg(D_ATR, "ecm1: %s", cs_hexdump(0, ecm, ecmLen, tmpBuffer1, sizeof(tmpBuffer1)));

	if (needsUnmasking)
	{
		hashModeCw = PowervuUnmaskEcm(ecm, seedEcmCw, &modeCW);
	}

	//cs_log_dbg(D_ATR, "needsUnmasking=%d", needsUnmasking);
	//cs_log_dbg(D_ATR, "ecm2: %s", cs_hexdump(0, ecm, ecmLen, tmpBuffer1, sizeof(tmpBuffer1)));

	memcpy(unmaskedEcm, ecm, ecmLen);

	ecmCrc32 = b2i(4, ecm + ecmLen - 4);

	if (fletcher_crc32(ecm, ecmLen - 4) != ecmCrc32)
	{
		return EMU_CHECKSUM_ERROR;
	}
	ecmLen -= 4;

	for (i = 0; i < 8; i++)
	{
		memset(convolvedCw[i], 0, 8);
	}

	for (i = 3; i + 3 < ecmLen; )
	{
		nanoLen = (((ecm[i] & 0x0F) << 8) | ecm[i + 1]);
		i += 2;

		if (nanoLen > 0)
		{
			nanoLen--;
		}
		nanoCmd = ecm[i++];

		if (i + nanoLen > ecmLen)
		{
			return EMU_NOT_SUPPORTED;
		}

		switch (nanoCmd)
		{
			case 0x27:
				if (nanoLen < 15)
				{
					break;
				}

				nanoChecksum = 0;
				for (j = 4; j < 15; j++)
				{
					nanoChecksum += ecm[i + j];
				}

				if (nanoChecksum != 0)
				{
					break;
				}

				keyType = PowervuGetConvcwIndex(ecm[i + 4]);
				memcpy(convolvedCw[keyType], &ecm[i + 6], 8);
				break;

			default:
				break;
		}

		i += nanoLen;
	}

	for (i = 3; i + 3 < ecmLen; )
	{
		nanoLen = (((ecm[i] & 0x0F) << 8) | ecm[i + 1]);
		i += 2;

		if (nanoLen > 0)
		{
			nanoLen--;
		}
		nanoCmd = ecm[i++];

		if (i + nanoLen > ecmLen)
		{
			return EMU_NOT_SUPPORTED;
		}

		switch (nanoCmd)
		{
			case 0x20:
				if (nanoLen < 54)
				{
					break;
				}

				offsetBody = i + 4 + ecm[i + 3];
				i += ecm[i + 3]; // Extra Data Length

				csaUsed = GetBit(ecm[i + 7], 7);
				fixedKey = !GetBit(ecm[i + 6], 5);
				oddKey = GetBit(ecm[i + 6], 4);
				xorMode = GetBit(ecm[i + 6], 0);
				bid = (GetBit(ecm[i + 7], 1) << 1) | GetBit(ecm[i + 7], 0);
				sbox = GetBit(ecm[i + 6], 3);

				keyIndex = (fixedKey << 3) | (bid << 2) | oddKey;
				channelId = b2i(2, ecm + i + 23);
				ecmSrvid = (channelId >> 4) | ((channelId & 0xF) << 12);

				decrypt_ok = 0;

				memcpy(ecmPart1, ecm + i + 8, 14);
				memcpy(ecmPart2, ecm + i + 27, 27);

				keyRef1 = 0;
				keyRef2 = 0;

				cs_log_dbg(D_ATR, "csaUsed=%d, xorMode=%d, ecmSrvid=%04X, hashModeCw=%d, modeCW=%d",
							csaUsed, xorMode, ecmSrvid, hashModeCw, modeCW);

				do
				{
					if (!PowervuGetEcmKey(ecmKey, ecmSrvid, keyIndex, keyRef1++))
					{
						if (!PowervuGetEcmKey(ecmKey, channelId, keyIndex, keyRef2++))
						{
							cs_log("Key not found: P %04X %02X", ecmSrvid, keyIndex);
							return EMU_KEY_NOT_FOUND;
						}
					}

					PowervuDecrypt(ecm + i + 8, 14, ecmKey, sbox);

					if ((ecm[i + 6] != ecm[i + 6 + 7]) || (ecm[i + 6 + 8] != ecm[i + 6 + 15]))
					{
						memcpy(ecm + i + 8, ecmPart1, 14);
						continue;
					}

					memcpy(tmpEcmKey, ecmKey, 7);

					PowervuDecrypt(ecm + i + 27, 27, ecmKey, sbox);

					if ((ecm[i + 23] != ecm[i + 23 + 29]) || (ecm[i + 23 + 1] != ecm[i + 23 + 30]))
					{
						memcpy(ecm + i + 8, ecmPart1, 14);
						memcpy(ecm + i + 27, ecmPart2, 27);
						continue;
					}

					decrypt_ok = 1;
				}
				while (!decrypt_ok);

				memcpy(seedBase, ecm + i + 6 + 2, 4);

				if (cdata == NULL)
				{
					SAFE_MUTEX_LOCK(&emu_fixed_key_srvid_mutex);
					for (j = 0; j < EMU_STREAM_SERVER_MAX_CONNECTIONS; j++)
					{
						if(!stream_server_has_ecm[j] && emu_stream_cur_srvid[j] == srvid)
						{
							update_global_key = 1;
							update_global_keys[j] = 1;
						}
					}
					SAFE_MUTEX_UNLOCK(&emu_fixed_key_srvid_mutex);
				}

				if (cdata != NULL || update_global_key || cw_ex != NULL)
				{
					// Calculate all seeds
					for (j = 0; j < 8; j++)
					{
						memcpy(ecmKey, tmpEcmKey, 7);
						PowervuCalculateSeed(j, ecm + i, seedBase, ecmKey, seed[j], sbox);
					}
				}
				else
				{
					// Calculate only video seed
					memcpy(ecmKey, tmpEcmKey, 7);
					PowervuCalculateSeed(PVU_CW_VID, ecm+i, seedBase, ecmKey, seed[PVU_CW_VID], sbox);
				}

				memcpy(baseCw, ecm + i + 6 + 8, 7);

				calculateAllCws = cdata != NULL || update_global_key || cw_ex != NULL;

				if (calculateAllCws)
				{
					// Calculate all cws
					for (j = 0; j < 8; j++)
					{
						PowervuCalculateCw(j, seed[j], csaUsed, convolvedCw[j], cw[j], baseCw, seedEcmCw,
											hashModeCw, needsUnmasking, xorMode, modeCW, unmaskedEcm + offsetBody);

						if (csaUsed)
						{
							for (k = 0; k < 8; k += 4)
							{
								cw[j][k + 3] = ((cw[j][k] + cw[j][k + 1] + cw[j][k + 2]) & 0xFF);
							}
						}

						cs_log_dbg(D_ATR, "calculated cw %d: %s", j,
										cs_hexdump(3, cw[j], 8, tmpBuffer1, sizeof(tmpBuffer1)));
					}

					//cs_log_dbg(D_ATR, "csaUsed=%d, cw: %s cdata=%x, cw_ex=%x",
					//			csaUsed, cs_hexdump(3, cw[0], 8, tmpBuffer1, sizeof(tmpBuffer1)),
					//			(unsigned int)cdata, (unsigned int)cw_ex);

					if (update_global_key)
					{
						for (j = 0; j < EMU_STREAM_SERVER_MAX_CONNECTIONS; j++)
						{
							if (update_global_keys[j])
							{
								cw_item = (emu_stream_cw_item *)malloc(sizeof(emu_stream_cw_item));
								if (cw_item != NULL)
								{
									cw_item->csa_used = csaUsed;
									cw_item->is_even = ecm[0] == 0x80 ? 1 : 0;
									cs_ftime(&cw_item->write_time);
									add_ms_to_timeb(&cw_item->write_time, cfg.emu_stream_ecm_delay);
									memcpy(cw_item->cw, cw, sizeof(cw));
									ll_append(ll_emu_stream_delayed_keys[j], cw_item);
								}
							}
						}
					}

					if (cdata != NULL)
					{
						for (j = 0; j < 8; j++)
						{
							if (csaUsed)
							{
								if (cdata->pvu_csa_ks[j] == NULL)
								{
									cdata->pvu_csa_ks[j] = get_key_struct();
								}

								if (ecm[0] == 0x80)
								{
									set_even_control_word(cdata->pvu_csa_ks[j], cw[j]);
								}
								else
								{
									set_odd_control_word(cdata->pvu_csa_ks[j], cw[j]);
								}

								cdata->pvu_csa_used = 1;
							}
							else
							{
								if (ecm[0] == 0x80)
								{
									des_set_key(cw[j], cdata->pvu_des_ks[j][0]);
								}
								else
								{
									des_set_key(cw[j], cdata->pvu_des_ks[j][1]);
								}

								cdata->pvu_csa_used = 0;
							}
						}
					}

					if (cw_ex != NULL)
					{
						cw_ex->mode = CW_MODE_MULTIPLE_CW;

						if (csaUsed)
						{
							cw_ex->algo = CW_ALGO_CSA;
							cw_ex->algo_mode = CW_ALGO_MODE_ECB;
						}
						else
						{
							cw_ex->algo = CW_ALGO_DES;
							cw_ex->algo_mode = CW_ALGO_MODE_ECB;
						}

						for (j = 0; j < 4; j++)
						{
							dwp = cw_ex->audio[j];

							memset(dwp, 0, 16);

							if (ecm[0] == 0x80)
							{
								memcpy(dwp, cw[PVU_CW_A1+j], 8);

								/*if (csaUsed)
								{
									for (k = 0; k < 8; k += 4)
									{
										dwp[k + 3] = ((dwp[k] + dwp[k + 1] + dwp[k + 2]) & 0xFF);
									}
								}*/
							}
							else
							{
								memcpy(&dwp[8], cw[PVU_CW_A1+j], 8);

								/*if (csaUsed)
								{
									for (k = 8; k < 16; k += 4)
									{
										dwp[k + 3] = ((dwp[k] + dwp[k + 1] + dwp[k + 2]) & 0xFF);
									}
								}*/
							}
						}

						dwp = cw_ex->data;

						memset(dwp, 0, 16);

						if (ecm[0] == 0x80)
						{
							memcpy(dwp, cw[PVU_CW_HSD], 8);

							/*if (csaUsed)
							{
								for (k = 0; k < 8; k += 4)
								{
									dwp[k + 3] = ((dwp[k] + dwp[k + 1] + dwp[k + 2]) & 0xFF);
								}
							}*/
						}
						else
						{
							memcpy(&dwp[8], cw[PVU_CW_HSD], 8);

							/*if (csaUsed)
							{
								for (k = 8; k < 16; k += 4)
								{
									dwp[k + 3] = ((dwp[k] + dwp[k + 1] + dwp[k + 2]) & 0xFF);
								}
							}*/
						}
					}
				}
				else
				{
					// Calculate only video cw
					PowervuCalculateCw(PVU_CW_VID, seed[PVU_CW_VID], csaUsed, convolvedCw[PVU_CW_VID],
										cw[PVU_CW_VID], baseCw, seedEcmCw, hashModeCw, needsUnmasking,
										xorMode, modeCW, unmaskedEcm + offsetBody);

					if (csaUsed)
					{
						for (k = 0; k < 8; k += 4)
						{
							cw[PVU_CW_VID][k + 3] = ((cw[PVU_CW_VID][k] + cw[PVU_CW_VID][k + 1] + cw[PVU_CW_VID][k + 2]) & 0xFF);
						}
					}

					cs_log_dbg(D_ATR, "calculated video only cw: %s",
									cs_hexdump(3, cw[PVU_CW_VID], 8, tmpBuffer1, sizeof(tmpBuffer1)));
				}

				memset(dw, 0, 16);

				if (ecm[0] == 0x80)
				{
					memcpy(dw, cw[PVU_CW_VID], 8);

					/*if (csaUsed)
					{
						for (k = 0; k < 8; k += 4)
						{
							dw[k + 3] = ((dw[k] + dw[k + 1] + dw[k + 2]) & 0xFF);
						}
					}*/
				}
				else
				{
					memcpy(&dw[8], cw[PVU_CW_VID], 8);

					/*if (csaUsed)
					{
						for (k = 8; k < 16; k += 4)
						{
							dw[k + 3] = ((dw[k] + dw[k + 1] + dw[k + 2]) & 0xFF);
						}
					}*/
				}

				return EMU_OK;

			default:
				break;
		}

		i += nanoLen;
	}

	return ret;
}

// PowerVu EMM EMU
static void PowervuUnmaskEmm(uint8_t *emm)
{
	uint8_t hashModeEmm, data[30], mask[16];
	uint32_t crc, i, l;

	uint8_t sourcePos[] = { 0x03, 0x0C, 0x0D, 0x11, 0x15, 0x18, 0x1D, 0x1F, 0x25, 0x2A,
							0x32, 0x35, 0x3A, 0x3B, 0x3E, 0x42, 0x47, 0x48, 0x53, 0x58,
							0x5C, 0x61, 0x66, 0x69, 0x71, 0x72, 0x78, 0x7B, 0x81, 0x84 };

	uint8_t destPos[] = { 0x02, 0x08, 0x0B, 0x0E, 0x13, 0x16, 0x1E, 0x23, 0x28, 0x2B,
						  0x2F, 0x33, 0x38, 0x3C, 0x40, 0x44, 0x4A, 0x4D, 0x54, 0x57,
						  0x5A, 0x63, 0x68, 0x6A, 0x70, 0x75, 0x76, 0x7D, 0x82, 0x85 };

	// Create Mask for ECM decryption
	PowervuCreateDataEcmEmm(emm, sourcePos, 19, 30, data);

	hashModeEmm = emm[8] ^ PowervuCrc8Calc(data, 30);

	PowervuCreateHash(data, 30, mask, hashModeEmm);

	// Fix Header
	emm[3] &= 0x0F;
	emm[3] |= 0x10;
	emm[8]  = 0x00;

	// Unmask Body
	for (i = 0; i < 30; i++)
	{
		emm[19 + destPos[i]] ^= mask[i & 0x0F];
	}

	// Fix CRC (optional)
	l = (((emm[1] << 8) + emm[2]) & 0xFFF) + 3 - 4;
	crc = fletcher_crc32(emm, l);

	emm[l + 0] = crc >> 24;
	emm[l + 1] = crc >> 16;
	emm[l + 2] = crc >> 8;
	emm[l + 3] = crc >> 0;
}

static int8_t PowervuUpdateEcmKeysByGroup(uint32_t groupId, uint8_t keyIndex, uint8_t *Key, uint32_t uniqueAddress)
{
	int8_t ret = 0;
	uint8_t oldKey[7];
	uint32_t foundProvider = 0, keyRef = 0;
	char indexStr[3], uaInfo[13];

	snprintf(indexStr, 3, "%02X", keyIndex);
	snprintf(uaInfo, 13, "UA: %08X", uniqueAddress);

	SAFE_MUTEX_LOCK(&emu_key_data_mutex);

	while (FindKey('P', groupId << 16 & 0xFFFF0000, 0x0000FFFF, indexStr, oldKey, 7, 0, keyRef, 0, &foundProvider))
	{
		keyRef++;

		if (memcmp(oldKey, Key, 7) == 0) // New ECM key already in the db
		{
			continue;
		}

		if (SetKey('P', foundProvider, indexStr, Key, 7, 1, uaInfo, NULL))
		{
			ret = 1;
		}
	}
	SAFE_MUTEX_UNLOCK(&emu_key_data_mutex);

	return ret;
}

int8_t PowervuEMM(uint8_t *emm, uint32_t *keysAdded)
{
	uint8_t emmInfo, emmType, decryptOk = 0;
	uint8_t emmKey[7], tmpEmmKey[7], tmp[26];
	uint16_t emmLen = GetEcmLen(emm);
	uint32_t i, uniqueAddress, groupId, keyRef = 0;
	//uint32_t emmCrc32;
	char keyName[EMU_MAX_CHAR_KEYNAME], keyValue[16];

	if (emmLen < 50)
	{
		return EMU_NOT_SUPPORTED;
	}

	// Check if unmasking is needed
	if ((emm[3] & 0xF0) == 0x50)
	{
		PowervuUnmaskEmm(emm);
	}

	// looks like checksum does not work for all EMMs
	//emmCrc32 = b2i(4, emm+emmLen-4);
	//
	//if(fletcher_crc32(emm, emmLen-4) != emmCrc32)
	//{
	//	return EMU_CHECKSUM_ERROR;
	//}
	emmLen -= 4;

	uniqueAddress = b2i(4, emm+12);
	snprintf(keyName, EMU_MAX_CHAR_KEYNAME, "%.8X", uniqueAddress);

	do
	{
		if (!PowervuGetEmmKey(emmKey, keyName, keyRef++, &groupId))
		{
			cs_log_dbg(D_TRACE, "EMM key for UA %s is missing", keyName);
			return EMU_KEY_NOT_FOUND;
		}

		for (i = 19; i + 27 <= emmLen; i += 27)
		{
			emmInfo = emm[i];

			if (!GetBit(emmInfo, 7))
			{
				continue;
			}

			//keyNb = emm[i] & 0x0F;

			memcpy(tmp, emm + i + 1, 26);
			memcpy(tmpEmmKey, emmKey, 7);
			PowervuDecrypt(emm + i + 1, 26, tmpEmmKey, 0);

			if ((emm[13] != emm[i + 24]) || (emm[14] != emm[i + 25]) || (emm[15] != emm[i + 26]))
			{
				memcpy(emm + i + 1, tmp, 26);
				memcpy(tmpEmmKey, emmKey, 7);
				PowervuDecrypt(emm + i + 1, 26, tmpEmmKey, 1);

				if ((emm[13] != emm[i + 24]) || (emm[14] != emm[i + 25]) || (emm[15] != emm[i + 26]))
				{
					memcpy(emm + i + 1, tmp, 26);
					memcpy(tmpEmmKey, emmKey, 7);
					continue;
				}
			}

			decryptOk = 1;

			emmType = emm[i + 2] & 0x7F;

			if (emmType > 1)
			{
				continue;
			}

			if (emm[i + 3] == 0 && emm[i + 4] == 0)
			{
				cs_hexdump(0, &emm[i + 3], 7, keyValue, sizeof(keyValue));
				cs_log("Key found in EMM: P %04X**** %02X %s -> REJECTED (looks invalid) UA: %08X",
						groupId, emmType, keyValue, uniqueAddress);
				continue;
			}

			PowervuUpdateEcmKeysByGroup(groupId, emmType, &emm[i + 3], uniqueAddress);

			(*keysAdded)++;
			cs_hexdump(0, &emm[i + 3], 7, keyValue, sizeof(keyValue));
			cs_log("Key found in EMM: P %04X**** %02X %s ; UA: %08X", groupId, emmType, keyValue, uniqueAddress);
		}

	} while (!decryptOk);

	return EMU_OK;
}

int8_t PowervuGetHexserials(uint16_t srvid, uint8_t hexserials[][4], uint32_t maxCount, uint32_t *count)
{
	//srvid == 0xFFFF -> get all

	int8_t alreadyAdded;
	uint8_t tmp[4];
	uint32_t i, j, k, groupid, length;
	KeyDataContainer *KeyDB;

	KeyDB = GetKeyContainer('P');
	if (KeyDB == NULL)
		{ return 0; }

	(*count) = 0;

	for (i = 0; i < KeyDB->keyCount && (*count) < maxCount; i++)
	{
		if (KeyDB->EmuKeys[i].provider <= 0x0000FFFF) // skip au keys
			{ continue; }

		if (srvid != 0xFFFF && (KeyDB->EmuKeys[i].provider & 0x0000FFFF) != srvid)
			{ continue; }

		groupid = KeyDB->EmuKeys[i].provider >> 16;

		for (j = 0; j < KeyDB->keyCount && (*count) < maxCount; j++)
		{
			if (KeyDB->EmuKeys[j].provider != groupid) // search au key with groupip
				{ continue; }

			length = strlen(KeyDB->EmuKeys[j].keyName);

			if (length < 3)
				{ continue; }

			if (length > 8)
				{ length = 8; }

			memset(tmp, 0, 4);
			CharToBin(tmp + (4 - (length / 2)), KeyDB->EmuKeys[j].keyName, length);

			for (k = 0, alreadyAdded = 0; k < *count; k++)
			{
				if (!memcmp(hexserials[k], tmp, 4))
				{
					alreadyAdded = 1;
					break;
				}
			}

			if (!alreadyAdded)
			{
				memcpy(hexserials[*count], tmp, 4);
				(*count)++;
			}
		}
	}

	return 1;
}

#endif // WITH_EMU
