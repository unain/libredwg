#ifndef LZ77_H
#define LZ77_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* set this to 1 for a greedy encoder */
#define GREEDY    0

/* ratio vs. speed constant */
/* the larger this constant, the better the compression */
#define MAXCOMPARES 75

/* unused entry flag */
#define NIL       0xFFFF

/* bits per symbol- normally 8 for general purpose compression */
#define CHARBITS  8

/* minimum match length & maximum match length */
#define THRESHOLD 2
#define MATCHBITS 18
#define MAXMATCH  ((1 << MATCHBITS) + THRESHOLD - 1)

/* sliding dictionary size and hash table's size */
/* some combinations of HASHBITS and THRESHOLD values will not work
correctly because of the way this program hashes strings */
#define DICTBITS  23
#define HASHBITS  20
#define DICTSIZE  (1 << DICTBITS)
#define HASHSIZE  (1 << HASHBITS)

/* # bits to shift after each XOR hash */
/* this constant must be high enough so that only THRESHOLD + 1
characters are in the hash accumulator at one time */
#define SHIFTBITS ((HASHBITS + THRESHOLD) / (THRESHOLD + 1))

/* sector size constants */
#define SECTORBIT 10
#define SECTORLEN (1 << SECTORBIT)
#define SECTORAND ((0xFFFF << SECTORBIT) & 0xFFFF)


void InitEncode(void);
unsigned int LoadDict(unsigned int dictpos);
void DeleteData(unsigned int dictpos);
void HashData(unsigned int dictpos, unsigned int bytestodo);
void FindMatch(unsigned int dictpos, unsigned int startlen);
void compute_opt(unsigned int i);
void DictSearch(unsigned int dictpos, unsigned int bytestodo);
extern "C" __declspec(dllexport)
void Encode(char *memory, unsigned int size, char* filename);
#endif // !LZ77_H
#pragma once
