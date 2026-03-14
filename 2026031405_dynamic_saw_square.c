#include <stdlib.h>
#include <math.h>
typedef unsigned int FREQ;
static const FREQ FREQ_MAX = (FREQ)0 - (FREQ)1;
static const FREQ FREQ_MID = FREQ_MAX - (FREQ_MAX >> 1);
typedef double AMP;

static FREQ tuning[12] = { 0x002d9000,0x00300000,0x00334200,0x00360000,0x0038f400,0x003cc000,0x00401280,0x00445800,0x00480000,0x004bf000,0x00510000,0x00556e00 };

typedef struct osc_tag OSC;

struct osc_tag
{
	OSC* next;
	FREQ f;
	FREQ rank;
	AMP aaff_sum;
	AMP bf;
	AMP b;
};

static OSC zero = { NULL,1,FREQ_MAX,0,0,0 };

struct
{
	FREQ t;
	OSC* o;
}organ = { 0,&zero };

double S(void)
{
	AMP out = 0;
	OSC* p = organ.o;
	while (p != NULL)
	{
		FREQ saw = p->f * organ.t;
		if (p->rank & FREQ_MID)
		{
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
		else
		{
			if (saw < FREQ_MID)
			{
				FREQ r = FREQ_MID - saw;
				if (r < p->f)
				{
					out -= ((AMP)2 * (AMP)r / (AMP)p->f - (AMP)1) * (AMP)FREQ_MID * p->b;
				}
				else
				{
					out -= (AMP)FREQ_MID * p->b;
				}
			}
			else
			{
				FREQ r = (FREQ)0 - saw;
				if (r < p->f)
				{
					out += ((AMP)2 * (AMP)r / (AMP)p->f - (AMP)1) * (AMP)FREQ_MID * p->b;
				}
				else
				{
					out += (AMP)FREQ_MID * p->b;
				}
			}
		}
		p = p->next;
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

int osc_on(FREQ f, AMP aaff, int if_saw)
{
	FREQ rank;
	if (if_saw)
	{
		rank = FREQ_MAX;
	}
	else
	{
		rank = 1;
	}
	FREQ rank_test = 1;
	while (!(rank_test & f))
	{
		rank <<= 1;
		rank_test <<= 1;
	}
	OSC** mid = &organ.o;
	while (*mid != NULL)
	{
		mid = &(*mid)->next;
	}
	OSC** p = mid;
	OSC* q = organ.o;
	int j = 0;
	while (q != *mid)
	{
		*p = (OSC*)malloc(sizeof(OSC));
		if (*p == NULL)
			return -1;
		FREQ tmpf = f / euc(f, q->f) * q->f;
		AMP tmpd = (AMP)f / (AMP)euc(f, q->f) * (AMP)q->f / 2;
		if (tmpf < tmpd)
		{
			(*p)->f = FREQ_MAX;
			(*p)->rank = 0;
			(*p)->aaff_sum = 0;
			(*p)->bf = 0;
			(*p)->b = 0;
		}
		else
		{
			(*p)->f = tmpf;
			(*p)->aaff_sum = q->aaff_sum + aaff;
			(*p)->rank = rank & q->rank;
			if ((*p)->rank)
			{
				(*p)->bf = sqrt((*p)->aaff_sum) - sqrt(q->aaff_sum);
				OSC* r = *mid;
				int k = 0;
				while (r != *p)
				{
					if (!(~j & k++))
					{
						(*p)->bf -= r->bf;
					}
					r = r->next;
				}
			}
			else
			{
				(*p)->bf = 0;
			}
			(*p)->b = (*p)->bf / (*p)->f;
		}
		q = q->next;
		p = &(*p)->next;
		*p = NULL;
		++j;
	}
	return 0;
}

void osc_off(int i)
{
	OSC** p = &organ.o;
	int j = 0;
	while (*p != NULL)
	{
		if (j & (1 << i))
		{
			OSC* tmp = *p;
			*p = (*p)->next;
			free(tmp);
		}
		else
		{
			p = &(*p)->next;
		}
		++j;
	}
	return;
}

typedef struct note_tag NOTE;

struct note_tag
{
	NOTE* next;
	int n;
	int ch;
};

static NOTE* start_note = NULL;

int N_on(int note, int velocity, int channel)
{
	int if_saw=1;
	if ((channel & 1) != 0)
	{
			if_saw = 0;
	}
	NOTE** end = &start_note;
	while (*end != NULL)
	{
		end = &(*end)->next;
	}
	*end = (NOTE*)malloc(sizeof(NOTE));
	if (*end == NULL)
	{
		return -1;
	}
	(*end)->next = NULL;
	(*end)->n = note;
	(*end)->ch = channel;
	FREQ f = tuning[note % 12] << ((note - 24) / 12);
	AMP af = sqrt(velocity) * (AMP)f;
	if (osc_on(f, af * af, if_saw))
	{
		return -1;
	}
	return 0;
}

int N_off(int note, int velocity, int channel)
{
	NOTE** p = &start_note;
	int i = 0;
	while (*p != NULL)
	{
		if ((*p)->n == note && (*p)->ch == channel)
		{
			NOTE* tmp = *p;
			*p = (*p)->next;
			free(tmp);
			osc_off(i);
			return 0;
		}
		else
		{
			p = &(*p)->next;
		}
		++i;
	}
	return -1;
}

int init(void)
{
	return 0;
}