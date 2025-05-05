// 
// 과제3의 채점 프로그램은 기본적으로 아래와 같이 동작함
// 본인이 직접 main()을 구현하여 테스트해 보기 바람
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "hybridmapping.h"

FILE *flashmemoryfp;

/****************  prototypes ****************/
void ftl_open();
void ftl_write(int lsn, char *sectorbuf);
void ftl_read(int lsn, char *sectorbuf);
void ftl_print();

//
// 이 함수는 file system의 역할을 수행한다고 생각하면 되고,
// file system이 flash memory로부터 512B씩 데이터를 저장하거나 데이터를 읽어 오기 위해서는
// 각자 구현한 FTL의 ftl_write()와 ftl_read()를 호출하면 됨
//
int main(int argc, char *argv[])
{
	char *blockbuf;
	char sectorbuf[SECTOR_SIZE];
	int i;

	flashmemoryfp = fopen("flashmemory", "w+b");
	if(flashmemoryfp == NULL)
	{
		exit(1);
	}
	
	blockbuf = (char *)malloc(BLOCK_SIZE);
	memset(blockbuf, 0xFF, BLOCK_SIZE);

	for(i = 0; i < BLOCKS_PER_DEVICE; i++)
	{
		fwrite(blockbuf, BLOCK_SIZE, 1, flashmemoryfp);
	}

	free(blockbuf);

	ftl_open();

	// LBN 0 채우기 (LSN 0~3)
	for(i = 0; i < 4; i++) {
		memset(sectorbuf, 'A'+i, SECTOR_SIZE);
		ftl_write(i, sectorbuf);
	}

	// LBN 2 채우기 (LSN 8~11)
	for(i = 8; i < 12; i++) {
		memset(sectorbuf, 'M'+i-8, SECTOR_SIZE);
		ftl_write(i, sectorbuf);
	}

	// LBN 4 채우기 (LSN 16~19)
	for(i = 16; i < 20; i++) {
		memset(sectorbuf, 'W'+i-16, SECTOR_SIZE);
		ftl_write(i, sectorbuf);
	}

	ftl_print();

	// LBN 0에 대해 블록 교체 발생시키기
	for(i = 0; i < 4; i++) {
		memset(sectorbuf, '0'+i, SECTOR_SIZE);
		ftl_write(i, sectorbuf);  // 같은 LSN에 덮어쓰기
	}

	// LBN 2도 블록 교체 발생시키기
	for(i = 8; i < 12; i++) {
		memset(sectorbuf, '5'+i-8, SECTOR_SIZE);
		ftl_write(i, sectorbuf);
	}

	ftl_print();

	fclose(flashmemoryfp);
	return 0;
}