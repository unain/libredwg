#include"lz77.h"
//#include "libredwg.h"
#include "decode.h"
static int RunDecom(char *filename, char* decodefile, unsigned int decomsize) {
	struct stat attrib;
	FILE *fp;
	fp = fopen(filename, "rb");
	size_t size;
	Bit_Chain bit_chain;
	if (stat(filename, &attrib))
	{
		return -1;
	}
	fp = fopen(filename, "rb");
	if (!fp)
	{
		printf("Could not open file: %s\n", filename);;
		return -1;
	}

	/* Load file to memory
	*/
	bit_chain.bit = 0;
	bit_chain.byte = 0;
	bit_chain.size = attrib.st_size;
	bit_chain.chain = (unsigned char *)malloc(bit_chain.size);
	if (!bit_chain.chain)
	{
		printf("Not enough memory.\n");
		fclose(fp);
		return -1;
	}
	size = 0;
	size = fread(bit_chain.chain, sizeof(char), bit_chain.size, fp);
	if (size != bit_chain.size)
	{
		printf("Could not read the entire file (%lu out of %lu);: %s\n",
			(long unsigned int) size, bit_chain.size, filename);
		fclose(fp);
		free(bit_chain.chain);
		return -1;
	}
	fclose(fp);
	char *decomp = (char *)malloc(0x7400 * sizeof(char));
	unsigned long  startIndex = bit_chain.byte;
	decompress_R2004_section(&bit_chain, decomp, size);
	FILE* file2 = fopen(decodefile, "wb");
	fwrite(decomp, 1, 0x7400, file2);
	fclose(file2);

	return 0;
}
int main() {
	int flag = 0;
	int success = 1;
	Dwg_Data dwg;
	dwg.parsetype = PARSE_ALL;
	dwg.num_objects = 0;
	//success = dwg_read_file("../dwg/normal2004.dwg", &dwg);
	success = dwg_read_file("F:\\PersonalProject\\github\\c\\libredwg\\build-aux\\vs2015\\Debug\\1.dwg", &dwg);
	if (success == 0) {
		for (int i = 0; i < dwg.header.num_descriptions; i++) {
			if (dwg.compress_data[i].compSize != 0) {

				printf("%d  %d\n", i, dwg.compress_data[i].compSize);
				char buf[50];

				sprintf(buf, "decode_%d_.bin", i);
				FILE* file = fopen(buf, "wb");
				fwrite(dwg.compress_data[i].decompress, 1, dwg.compress_data[i].decompSize, file);
				fclose(file);
				sprintf(buf, "encode_%d_.bin", i);
				file = fopen(buf, "wb");
				fwrite(dwg.compress_data[i].compress, 1, dwg.compress_data[i].compSize, file);
				fclose(file);

				sprintf(buf, "lz_encode_%d_.bin", i);
				//1.compress decompressed
				Encode(dwg.compress_data[i].decompress, dwg.compress_data[i].decompSize, buf);


				//2.decompress compressed
				char buf2[50];
				sprintf(buf2, "lz_decode_%d_.bin", i);
				RunDecom(buf, buf2, dwg.compress_data[i].decompSize);
				//3.comprare decompressed
			}
		}
	}
	printf("success %d  \n", success);
	return 0;
}