#include <math.h>
typedef unsigned int FREQ;
static const FREQ FREQ_MAX = (FREQ)0 - (FREQ)1;
typedef double AMP;

static FREQ tuning[12] = { 0x002d9000,0x0030a7a6,0x00334200,0x00360000,0x0039aa40,0x003cc000,0x0040df88,0x00445800,0x0048fb79,0x004ce300,0x00510000,0x00567f60 };

#define NOTE_NUM 8

typedef struct osc_tag
{
	FREQ f;
	AMP aaff_sum;
	AMP bf;
	AMP b;
}OSC;

struct
{
	FREQ t;
	OSC o[1 << NOTE_NUM];
}organ = { 0 };

double S(void)
{
	AMP out = 0;
	for (int i = 0; i < (1 << NOTE_NUM); ++i)
	{
		OSC* p = &organ.o[i];
		if (p->b)
		{
			FREQ saw = p->f * organ.t;
			FREQ r = (FREQ)0 - saw;
			if (r > p->f)
			{
				out += ((AMP)(saw + p->f) - (AMP)((FREQ)0 - saw)) * p->b;
			}
			else
			{
				out += (AMP)((FREQ)0 - p->f) * ((AMP)2 * (AMP)r / (AMP)p->f - (AMP)1) * p->b;
			}
		}
	}
	++organ.t;
	return (double)out;
}

//ユークリッドの互除法を用いてmとnの最大公約数を求める関数
FREQ euc(FREQ m, FREQ n)
{
	if (n == 0)
	{
		return m;
	}
	FREQ r = m % n;
	if (r)
	{
		return euc(n, r);
	}
	else
	{
		return n;
	}
}

int rot(int i, int s)
{
	return ((i << s) | (i >> (NOTE_NUM - s))) & ((1 << NOTE_NUM) - 1);
}

void osc(FREQ f, AMP aaff, int i)
{
	static const int mid = 1 << (NOTE_NUM - 1);
	for (int j = mid; j < 1 << NOTE_NUM; ++j)
	{
		OSC* p = &organ.o[rot(j, i)];
		OSC* q = &organ.o[rot(j - mid, i)];
		p->aaff_sum = q->aaff_sum;
		FREQ gcd = euc(f, q->f);
		FREQ tmpf = f / gcd * q->f;
		AMP tmpd = (AMP)f / (AMP)gcd * (AMP)q->f / 2;
		//周波数の桁あふれの確認
		if (tmpf < tmpd)
		{
			p->f = FREQ_MAX;
			p->bf = 0;
			p->b = 0;
		}
		else
		{
			p->f = tmpf;
			if (aaff)
			{
				p->aaff_sum += aaff;
				p->bf = sqrt(p->aaff_sum) - sqrt(q->aaff_sum);
				for (int k = mid; k < j; ++k)
				{
					if (!(~j & k))
						p->bf -= organ.o[rot(k, i)].bf;
				}
				p->b = p->bf / p->f;
			}
			else
			{
				p->bf = 0;
				p->b = 0;
			}
		}
	}
}


int n[NOTE_NUM][2];

int N_on(int note, int velocity, int channel)
{
	for (int i = 0; i < NOTE_NUM; ++i)
	{
		if (n[i][0] < 0)
		{
			n[i][0] = note;
			n[i][1] = channel;
			FREQ f = tuning[note % 12] << ((note - 24) / 12);
			AMP af = sqrt(velocity) * (AMP)f;
			osc(f, af * af, i);
			break;
		}
	}
	return 0;
}

int N_off(int note, int velocity, int channel)
{
	for (int i = 0; i < NOTE_NUM; ++i)
	{
		if (n[i][0] == note && n[i][1] == channel)
		{
			n[i][0] = -1;
			osc(1, 0, i);
			break;
		}
	}
	return 0;
}

int init(void)
{
	for (int i = 0; i < (1 << NOTE_NUM); ++i)
	{
		organ.o[i].f = 1;
		organ.o[i].bf = 0;
		organ.o[i].aaff_sum = 0;
		organ.o[i].b = 0;
	}
	for (int i = 0; i < NOTE_NUM; ++i)
	{
		n[i][0] = -1;
	}
	return 0;
}