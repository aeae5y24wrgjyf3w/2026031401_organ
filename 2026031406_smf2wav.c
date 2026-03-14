#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>
#include <string.h>

#include "./2026031402_io.h"

#define SAMPLE_RATE 44100
#define BIT_DEPTH 16
#define MY_FILENAME1 "tmp.dat"

typedef int16_t SAMPLE;
#define SAMPLE_MAX INT16_MAX

typedef struct track_tag TRACK;

struct track_tag
{
	TRACK* next;
	uint8_t* b;
	uint8_t* b0;
	uint8_t* bl;
	uint8_t status;
};

typedef struct
{
	TRACK start;
	uint16_t tick;
	double cycle;
} SCORE;

int read_vl(TRACK* p_trk, uint32_t* value)
{
	*value = 0;
	for (int i = 0; i < 4; ++i)
	{
		uint8_t buf;
		*value <<= 7;
		if (p_trk->b != p_trk->bl)
		{
			buf = *(p_trk->b++);
		}
		else
			return -1;
		*value |= (buf & 0x7f);
		if (!(buf & 0x80))
		{
			break;
		}
	}
	return 0;
}

int write_vl(TRACK* p_trk, uint32_t* value)
{
	uint32_t tmp = *value;
	for (int i = 0; i < 4; ++i)
	{
		if (p_trk->b != p_trk->b0)
		{
			*(--p_trk->b) = (tmp & 0x7f);
		}
		else
			return -1;
		if (i > 0)
		{
			*p_trk->b |= 0x80;
		}
		tmp >>= 7;
		if (tmp == 0)
		{
			break;
		}
	}
	return 0;
}

int read_event(TRACK* p_trk, SCORE* score)
{
	uint8_t tmp_status = 0;
	if (p_trk->b != p_trk->bl)
	{
		tmp_status = *(p_trk->b++);
	}
	else//no more data in this track
	{
		return -1;
	}
	if (!(tmp_status & 0x80))//running status (same event as before event)
	{
		if (p_trk->b != p_trk->b0)
		{
			*(--p_trk->b) = tmp_status;
		}
		else//no more data in this track
		{
			return -1;
		}
	}
	else
	{
		p_trk->status = tmp_status;
	}
	if (p_trk->status & 0x80)
	{
		if ((p_trk->status & 0xf0) == 0xf0)//meta event or system exclusive event
		{
			uint8_t meta_type = 0;
			if (p_trk->status & 0x08)//meta event
			{
				if (p_trk->b != p_trk->bl)
				{
					meta_type = *(p_trk->b++);
				}
				else//no more data in this track
				{
					return -1;
				}
			}
			uint32_t event_len = 0;
			if (read_vl(p_trk, &event_len))
			{
				return -1;
			}
			//printf("event_length:%08x ", event_len);
			if (meta_type == 0x51)//tempo
			{
				uint32_t tempo = 0;
				while (event_len)
				{
					--event_len;
					tempo <<= 8;
					if (p_trk->b != p_trk->bl)
					{
						tempo |= *(p_trk->b++);
					}
					else//no more data in this track
					{
						return -1;
					}
				}
				score->cycle = SAMPLE_RATE * (double)tempo / (double)score->tick / 1000000.0;
			}
			while (event_len)
			{
				--event_len;
				if (p_trk->b != p_trk->bl)
				{
					uint8_t buf = *(p_trk->b++);
				}
				else//no more data in this track
				{
					return -1;
				}
			}
			return 0;
		}
		else//midi message
		{
			uint8_t v0;
			if (p_trk->b != p_trk->bl)
			{
				v0 = *(p_trk->b++);
			}
			else//no more data in this track
			{
				return -1;
			}
			if ((p_trk->status & 0xf0) == 0xc0 || (p_trk->status & 0xf0) == 0xd0)//program change or channel pressure
			{
				return 0;
			}
			else//note on, note off, polyphonic key pressure, control change or pitch bend change
			{
				uint8_t v1 = 0;
				if (p_trk->b != p_trk->bl)
				{
					v1 = *(p_trk->b++);
				}
				else//no more data in this track
				{
					return -1;
				}
				if ((p_trk->status & 0xe0) == 0x80)//note on or note off
				{
					uint8_t channel = p_trk->status & 0xf;
					uint8_t note = v0;
					uint8_t velocity = v1;
					if ((p_trk->status & 0xf0) == 0x80 || velocity == 0)//note off
					{
						
						if (N_off(note, velocity, channel))
						{
							return -1;
						}
					}
					else//note on
					{
						if (N_on(note, velocity, channel))
						{
							return -1;
						}
					}
				}
			}
		}
		return 0;
	}
	else
	{
		printf("bad event status");
		return -1;
	}
}

size_t read_4(uint32_t* u, FILE* fp)
{
	size_t ret;
	*u = 0;
	for (int i = 0; i < 4; ++i)
	{
		*u <<= 8;
		uint8_t buf;
		ret = fread(&buf, 1, 1, fp);
		*u |= buf;
	}
	return ret;
}

size_t read_2(uint16_t* u, FILE* fp)
{
	size_t ret;
	for (int i = 0; i < 2; ++i)
	{
		*u <<= 8;
		uint8_t buf;
		ret = fread(&buf, 1, 1, fp);
		*u |= buf;
	}
	return ret;
}

int read_smf(FILE* fpm, SCORE* score)
{
	////header chunk////
	printf("HEADER ");
	//chunk type
	uint32_t chunk_type;
	if (!read_4(&chunk_type, fpm))
	{
		printf("no more data");
		return -1;
	}
	if (chunk_type == 0x4d546864)
	{
		printf("MThd ");
	}
	else
	{
		printf("not MThd\n");
		return -1;
	}
	//header length
	uint32_t header_len;
	if (!read_4(&header_len, fpm))
	{
		printf("no more data");
		return -1;
	}
	printf("header_length:%08x ", header_len);
	//format
	uint16_t format;
	if (!read_2(&format, fpm))
	{
		printf("no more data");
		return -1;
	}
	printf("format:%d ", format);
	//number of trucks
	uint16_t num_track;
	if (!read_2(&num_track, fpm))
	{
		printf("no more data");
		return -1;
	}
	printf("number of tracks:%08x ", num_track);
	//time unit
	score->tick;
	if (!read_2(&score->tick, fpm))
	{
		printf("no more data");
		return -1;
	}
	printf("time unit:%08x\n", score->tick);

	////track chunks////
	TRACK* cur_trk = &score->start;
	for (int n = 0; n < num_track; ++n)
	{
		printf("TRACK%08x ", n);
		//chunk type
		if (!read_4(&chunk_type, fpm))
		{
			printf("no more data");
			return -1;
		}
		if (chunk_type == 0x4d54726b)
		{
			printf("MTrk ");
		}
		else
		{
			break;
			printf("not MTrk ");
			return -1;
		}
		//data length
		uint32_t track_len;
		if (!read_4(&track_len, fpm))
		{
			printf("no more data");
			return -1;
		}
		printf("track_data_length:%08x\n", track_len);
		cur_trk->next = (TRACK*)malloc(sizeof(TRACK));
		if (cur_trk->next == NULL)
		{
			printf("NULL");
			return -1;
		}
		cur_trk = cur_trk->next;
		cur_trk->b0 = (uint8_t*)malloc(track_len);
		if (cur_trk->b0 == NULL)
		{
			printf("NULL");
			return -1;
		}
		if (fread(cur_trk->b0, 1, track_len, fpm) < track_len)
		{
			printf("no more data");
			return -1;
		}
		cur_trk->b = cur_trk->b0;
		cur_trk->bl = cur_trk->b0 + track_len;
		cur_trk->next = NULL;
		cur_trk->status = 0;
	}
	return 0;
}

int main(int argc, char* argv[])
{
	
	if (argc < 2)
	{
		printf("no input MIDI filename\n");
		return -1;
	}
	FILE* fp1 = fopen(MY_FILENAME1, "wb");
	if (fp1 == NULL)
	{
		printf("failed to open %s\n", MY_FILENAME1);
		return -1;
	}printf("%s opened\n", MY_FILENAME1);
	double max = 0;
	int sample = 0;
	init();

	for(int i=1;i<argc;++i)
	{
		char filename_m[FILENAME_MAX];
		strcpy(filename_m, argv[i]);
		FILE* fpm = fopen(filename_m, "rb");
		if (fpm == NULL)
		{
			printf("failed to open %s", filename_m);
			break;
		}printf("%s opened\n", filename_m);
		SCORE score0;
		score0.start.next = NULL;

		if (read_smf(fpm, &score0))
		{
			break;
		}
		fclose(fpm); printf("%s closed\n", filename_m);
		score0.cycle = SAMPLE_RATE * 500000.0 / (double)score0.tick / 1000000.0;
		while (score0.start.next != NULL)
		{
			TRACK* prev = &score0.start;
			while (prev->next != NULL)
			{
				TRACK* cur = prev->next;
				if (cur->b == cur->bl)
				{
					free(cur->b0);
					prev->next = cur->next;
					free(cur);
				}
				else
				{
					////delta_time////
					uint32_t delta_t = 0;
					if (read_vl(cur, &delta_t))
					{
						printf("no data sorry.");
						break;
					}
					if (delta_t > 0)
					{
						--delta_t;
						if (write_vl(cur, &delta_t))
						{
							printf("no data sorry.");
							break;
						}
						prev = cur;
					}
					else
					{
						////event////
						if (read_event(cur, &score0))
						{
							printf("no data sorry.");
							break;
						}
					}
				}
			}
			////PLAY////
			static double cycle = 0;
			cycle += score0.cycle;
			while (--cycle > 0)
			{
				double tmp = S();
				fwrite(&tmp, sizeof(double), 1, fp1);
				if (tmp > max)
				{
					max = tmp;
				}
				else if (-tmp > max)
				{
					max = -tmp;
				}
				++sample;
			}
		}
	}
	fclose(fp1); printf("%s closed\n", MY_FILENAME1);

	//max = 1;
	double ratio = (double)SAMPLE_MAX / max;

	FILE* fp2 = fopen(MY_FILENAME1, "rb");
	if (fp2 == NULL)
	{
		printf("failed to open %s", MY_FILENAME1);
		return -1;
	}printf("%s opened\n", MY_FILENAME1);
	char filename_w[FILENAME_MAX];
	strcpy(filename_w, "a.wav");
	FILE* fp = fopen(filename_w, "wb");
	if (fp == NULL)
	{
		printf("failed to open %s", filename_w);
		return -1;
	}printf("%s opened\n", filename_w);
	int8_t riff_chunk_ID[4] = { 'R','I','F','F' };
	int32_t riff_chunk_size = 36 + sample * BIT_DEPTH / 8;
	int8_t riff_form_type[4] = { 'W','A','V','E' };
	int8_t fmt_chunk_ID[4] = { 'f', 'm', 't', ' ' };
	int32_t fmt_chunk_size = 16;
	int16_t fmt_wave_format_type = 1;
	int16_t fmt_channel = 1;
	int32_t fmt_samples_per_sec = SAMPLE_RATE;
	int32_t fmt_bytes_per_sec = SAMPLE_RATE * BIT_DEPTH / 8;
	int16_t fmt_block_size = BIT_DEPTH / 8;
	int16_t fmt_bits_per_sample = BIT_DEPTH;
	int8_t data_chunk_ID[4] = { 'd','a','t','a' };
	int32_t data_chunk_size = sample * BIT_DEPTH / 8;

	fwrite(riff_chunk_ID, 1, 4, fp);
	fwrite(&riff_chunk_size, 4, 1, fp);
	fwrite(riff_form_type, 1, 4, fp);
	fwrite(fmt_chunk_ID, 1, 4, fp);
	fwrite(&fmt_chunk_size, 4, 1, fp);
	fwrite(&fmt_wave_format_type, 2, 1, fp);
	fwrite(&fmt_channel, 2, 1, fp);
	fwrite(&fmt_samples_per_sec, 4, 1, fp);
	fwrite(&fmt_bytes_per_sec, 4, 1, fp);
	fwrite(&fmt_block_size, 2, 1, fp);
	fwrite(&fmt_bits_per_sample, 2, 1, fp);
	fwrite(data_chunk_ID, 1, 4, fp);
	fwrite(&data_chunk_size, 4, 1, fp);
	while (sample)
	{
		double buff0;
		fread(&buff0, sizeof(double), 1, fp2);

		SAMPLE buff1 = (SAMPLE)(buff0 * ratio);
		fwrite(&buff1, sizeof(SAMPLE), 1, fp);
		--sample;
	}
	fclose(fp); printf("%s closed\n", filename_w);
	fclose(fp2); printf("%s closed\n", MY_FILENAME1);
	remove(MY_FILENAME1);
	return 0;
}