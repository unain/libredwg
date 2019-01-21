/* PROG1.C                                                    */
/* Simple Hashing LZ77 Sliding Dictionary Compression Program */
/* By Rich Geldreich, Jr. October, 1993                       */
/* Originally compiled with QuickC v2.5 in the small model.   */
#include "lz77.h"
/* dictionary plus MAXMATCH extra chars for string comparisions */
unsigned char
dict[DICTSIZE + MAXMATCH];

/* hashtable & link list table */
unsigned int
hash[HASHSIZE],
nextlink[DICTSIZE];

/* misc. global variables */
unsigned int
matchlength,
matchpos,
bitbuf,
bitsin,
masks[17] = { 0,1,3,7,15,31,63,127,255,511,1023,2047,4095,8191,16383,32767,65535 };
unsigned int litCount = 0;
unsigned int comp_bytes = 0;
unsigned int comp_offset = 0;
unsigned int last_index = 0;

FILE *infile, *outfile, *dwgfile;
char *inmemory, *endmemory;
unsigned insize;
/* initializes the search structures needed for compression */
void InitEncode(void)
{
	register unsigned int i;

	//hashSize =  1<<20 = 1048576
	for (i = 0; i < HASHSIZE; i++) hash[i] = NIL;
	// 2^23 = 8388608
	nextlink[DICTSIZE] = NIL;

}

/* loads dictionary with characters from the input stream */
unsigned int LoadDict(unsigned int dictpos)
{
	register unsigned int i, j;

	if (inmemory > endmemory) return 0;
	if (inmemory + SECTORLEN < endmemory) {
		memcpy(&dict[dictpos], inmemory, SECTORLEN);
		i = SECTORLEN;
	}
	else {
		memcpy(&dict[dictpos], inmemory, endmemory - inmemory);
		i = endmemory - inmemory;
	}
	inmemory += i;
	/*if ((i = fread(&dict[dictpos], sizeof(char), SECTORLEN, infile)) == EOF)
	{
	printf("\nerror reading from input file");
	exit(EXIT_FAILURE);
	}*/

	/* since the dictionary is a ring buffer, copy the characters at
	the very start of the dictionary to the end */
	if (dictpos == 0)
	{
		//2^18 + 2 -1 = 262145
		for (j = 0; j < MAXMATCH; j++) dict[j + DICTSIZE] = dict[j];
	}

	return i;
}

/* deletes data from the dictionary search structures */
/* this is only done when the number of bytes to be
compressed exceeds the dictionary's size */
void DeleteData(unsigned int dictpos)
{

	register unsigned int i, j;

	j = dictpos;        /* put dictpos in register for more speed */

						/* delete all references to the sector being deleted */

	for (i = 0; i < DICTSIZE; i++)
		if ((nextlink[i] & SECTORAND) == j) nextlink[i] = NIL;

	for (i = 0; i < HASHSIZE; i++)
		if ((hash[i] & SECTORAND) == j) hash[i] = NIL;

}

/* hash data just entered into dictionary */
/* XOR hashing is used here, but practically any hash function will work */
void HashData(unsigned int dictpos, unsigned int bytestodo)
{
	register unsigned int i, j, k;

	if (bytestodo <= THRESHOLD)   /* not enough bytes in sector for match? */
		for (i = 0; i < bytestodo; i++) nextlink[dictpos + i] = NIL;
	else
	{
		/* matches can't cross sector boundries */
		for (i = bytestodo - THRESHOLD; i < bytestodo; i++)
			nextlink[dictpos + i] = NIL;
		// SHIFTBITS = 7;
		j = (((unsigned int)dict[dictpos]) << SHIFTBITS) ^ dict[dictpos + 1];

		k = dictpos + bytestodo - THRESHOLD;  /* calculate end of sector */

		for (i = dictpos; i < k; i++)
		{
			nextlink[i] = hash[j = (((j << SHIFTBITS) & (HASHSIZE - 1)) ^ dict[i + THRESHOLD])];
			hash[j] = i;
		}
	}
}

/* finds match for string at position dictpos */
/* this search code finds the longest AND closest
match for the string at dictpos */
void FindMatch(unsigned int dictpos, unsigned int startlen)
{
	register unsigned int i, j, k;
	unsigned char l;

	i = dictpos; matchlength = startlen; k = MAXCOMPARES;
	l = dict[dictpos + matchlength];

	do
	{
		if ((i = nextlink[i]) == NIL) return;   /* get next string in list */
		if (dictpos < i) return;
		if (dictpos - i < 0x3FFF && dictpos - i > 255) return;
		if (dict[i + matchlength] == l)        /* possible larger match? */
		{
			for (j = 0; j < MAXMATCH; j++)          /* compare strings */
				if (dict[dictpos + j] != dict[i + j]) break;

			if (j > matchlength)  /* found larger match? */
			{
				matchlength = j;
				matchpos = i;
				if (matchlength == MAXMATCH) return;  /* exit if largest possible match */
				l = dict[dictpos + matchlength];
			}
		}
	} while (--k);  /* keep on trying until we run out of chances */

}
void compute_opt(unsigned int i) {
	printf("com_bytes  %d  com_off %d  lit_len  %d\n", comp_bytes, comp_offset, litCount);
	//fprintf(outfile, "com_bytes  %d  com_off %d  lit_len  %d\n", comp_bytes, comp_offset, litCount);
	//0x40 - 0xFF
	if (comp_bytes < 15 && comp_offset <= 255) {
		unsigned int opt1 = (comp_bytes + 1) << 4 | comp_offset << 2 & 0x0C;
		unsigned int opt2 = comp_offset >> 2;
		if (litCount <= 3) {
			opt1 |= litCount;
		}
		if (opt1 == 0x58 && opt2 == 0x05) {
			opt1 == 0x58;
		}
		fputc(opt1, dwgfile);
		fputc(opt2, dwgfile);
		if (litCount > 3) {
			unsigned int opt3 = litCount - 3;
			if (opt3 > 0x0F) {
				fputc(0, dwgfile);
				opt3 -= 0x0F;
				while (opt3 > 0xFF)
				{
					fputc(0, dwgfile);
					opt3 -= 0xFF;
				}
				fputc(opt3, dwgfile);
			}
			else
				fputc(opt3, dwgfile);
		}
		for (int k = i - litCount; k < i; k++)
			fputc(dict[k], dwgfile);
	}// 0x21 ~ 0x3F
	else if (comp_bytes < 34 && comp_offset <= 255) {
		unsigned int opt1 = 0x1E + comp_bytes;
		unsigned int byte1 = comp_offset << 2;

		if (litCount <= 3) {
			byte1 |= litCount;
		}
		unsigned int byte2 = comp_offset >> 6;
		fputc(opt1, dwgfile);
		fputc(byte1, dwgfile);
		fputc(byte2, dwgfile);
		if (litCount > 3) {
			unsigned int opt3 = litCount - 3;
			if (opt3 > 0x0F) {
				fputc(0, dwgfile);
				opt3 -= 0x0F;
				while (opt3 > 0xFF)
				{
					fputc(0, dwgfile);
					opt3 -= 0xFF;
				}
				fputc(opt3, dwgfile);
			}
			else
				fputc(opt3, dwgfile);

		}
		for (int k = i - litCount; k < i; k++)
			fputc(dict[k], dwgfile);
	}
	//0x20
	else if (comp_offset <= 255) {
		fputc(0x20, dwgfile);
		unsigned int opt1 = comp_bytes - 0x21;
		while (opt1 > 0xFF)
		{
			fputc(0x00, dwgfile);
			opt1 -= 0xFF;
		}
		fputc(opt1, dwgfile);
		unsigned int byte1 = comp_offset << 2;

		if (litCount <= 3) {
			byte1 |= litCount;
		}
		unsigned int byte2 = comp_offset >> 6;
		fputc(byte1, dwgfile);
		fputc(byte2, dwgfile);
		if (litCount > 3) {
			unsigned int opt3 = litCount - 3;
			if (opt3 > 0x0F) {
				fputc(0, dwgfile);
				opt3 -= 0x0F;
				while (opt3 > 0xFF)
				{
					fputc(0, dwgfile);
					opt3 -= 0xFF;
				}
				fputc(opt3, dwgfile);
			}
			else
				fputc(opt3, dwgfile);

		}
		for (int k = i - litCount; k < i; k++)
			fputc(dict[k], dwgfile);

	}
	else {
		printf("here111");
	}

}
/* finds dictionary matches for characters in current sector */
void DictSearch(unsigned int dictpos, unsigned int bytestodo)
{

	register unsigned int i, j;

#if (GREEDY == 0)

	unsigned int matchlen1, matchpos1;

	/* non-greedy search loop (slow) */

	i = dictpos; j = bytestodo;

	while (j) /* loop while there are still characters left to be compressed */
	{
		FindMatch(i, THRESHOLD);

		if (matchlength > THRESHOLD)
		{
			matchlen1 = matchlength;
			matchpos1 = matchpos;

			for (; ; )
			{
				FindMatch(i + 1, matchlen1);

				if (matchlength > matchlen1)
				{
					matchlen1 = matchlength;
					matchpos1 = matchpos;
					litCount++;
					i++;
					j--;
				}
				else
				{
					if (matchlen1 > j)
					{
						matchlen1 = j;
						if (matchlen1 <= THRESHOLD) {
							litCount++;
							i++; j--; break;
						}
					}
					//calculate opt1,opt2,opt3
					if (comp_bytes != 0) {
						compute_opt(i);
					}
					else {
						unsigned int opt3 = litCount - 3;
						if (opt3 > 0x0F) {
							fputc(0, dwgfile);
							opt3 -= 0x0F;
							while (opt3 > 0xFF)
							{
								fputc(0, dwgfile);
								opt3 -= 0xFF;
							}
							fputc(opt3, dwgfile);
						}
						else
							fputc(opt3, dwgfile);
						for (int k = i - litCount; k < i; k++)
							fputc(dict[k], dwgfile);
					}
					litCount = 0;
					comp_bytes = matchlen1;
					comp_offset = i - matchpos1 - 1;
					i += matchlen1;
					j -= matchlen1;
					break;
				}
			}
		}
		else
		{
			litCount++;
			i++;
			j--;
		}
	}
	last_index = i;
#else

	/* greedy search loop (fast) */

	i = dictpos; j = bytestodo;

	while (j) /* loop while there are still characters left to be compressed */
	{
		FindMatch(i, THRESHOLD);

		if (matchlength > j) matchlength = j;     /* clamp matchlength */

		if (matchlength > THRESHOLD)  /* valid match? */
		{
			SendMatch(matchlength, (i - matchpos) & (DICTSIZE - 1));
			i += matchlength;
			j -= matchlength;
		}
		else
		{
			SendChar(dict[i++]);
			j--;
		}
	}

#endif

}

/* main encoder */
void Encode(char *memory, unsigned int size, char* filename)
{
	litCount = comp_bytes = comp_offset = last_index = 0;
	outfile = fopen("log.txt", "w");
	dwgfile = fopen(filename, "wb");
	unsigned int dictpos, deleteflag, sectorlen;
	unsigned long bytescompressed;
	inmemory = memory;
	endmemory = memory + size;
	InitEncode();

	dictpos = deleteflag = 0;

	bytescompressed = 0;

	while (1)
	{
		/* delete old data from dictionary */
		if (deleteflag) DeleteData(dictpos);

		/* grab more data to compress */
		if ((sectorlen = LoadDict(dictpos)) == 0) break;

		/* hash the data */
		HashData(dictpos, sectorlen);

		/* find dictionary matches */
		DictSearch(dictpos, sectorlen);

		bytescompressed += sectorlen;

		//printf("\r%ld", bytescompressed);

		dictpos += SECTORLEN;

		/* wrap back to beginning of dictionary when its full */
		if (dictpos == DICTSIZE)
		{
			inmemory -= litCount;
			litCount = 0;
			dictpos = 0;
			deleteflag = 1;   /* ok to delete now */
		}
	}


	//process the end
	compute_opt(last_index);
	fputc(0x11, dwgfile);
	fclose(dwgfile);
	return;
}

