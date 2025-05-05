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
	int lsn, i;

        flashmemoryfp = fopen("flashmemory", "w+b");
	if(flashmemoryfp == NULL)
	{
		printf("file open error\n");
		exit(1);
	}
	   
    //
    // flash memory의 모든 바이트를 '0xff'로 초기화한다.
    // 
        blockbuf = (char *)malloc(BLOCK_SIZE);
	memset(blockbuf, 0xFF, BLOCK_SIZE);

	for(i = 0; i < BLOCKS_PER_DEVICE; i++)
	{
		fwrite(blockbuf, BLOCK_SIZE, 1, flashmemoryfp);
	}

	free(blockbuf);

	ftl_open();    // ftl_read(), ftl_write() 호출하기 전에 이 함수를 반드시 호출해야 함

	//
	// 여러 개의 ftl_write() 및 ftl_read() 호출을 통한 테스트 코드 작성
	//


	fclose(flashmemoryfp);

	return 0;
}