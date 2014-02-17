#include "ds_memvec.h"

memvec_t *memvec_new(const void *src, size_t size) 
{
	memvec_t *mv;
	if (size <= 0) {
		return NULL;
	}
	mv = (memvec_t *)malloc(sizeof(memvec_t));
	if (mv == NULL) {
		return NULL;
	}
	mv->ptr = (uint8_t *) src;
	mv->size = size;
	return mv;
}

int memvec_delete(memvec_t *mv)
{
	if (mv) {
		if (mv->ptr) {
			free(mv->ptr);
		}
		mv->ptr = NULL;
		free(mv);
		mv = NULL;
		return 0;
	}
	return -1;
}

int memvec_cmp(memvec_t *mv1, memvec_t *mv2)
{
	if ((mv1->size == mv2->size) && (mv1->ptr == mv2->ptr))
		return 0;
	return -1;
}

int memvec_cmp_content(memvec_t *mv1, memvec_t *mv2) 
{
	uint8_t *p1, *p2;

	if (mv1->size == mv2->size) {
		p1 = mv1->ptr;
		p2 = mv2->ptr;
		return memcmp(p1, p2, mv1->size);
	}

	return (mv1->size > mv2->size) ? 1 : -1;
}

memvec_t *memvec_dup(memvec_t *src) 
{
	memvec_t *mv;
	mv = (memvec_t *) malloc(sizeof(memvec_t));
	if (mv == NULL) {
		return NULL;
	}
	mv->ptr = (void *) malloc(src->size);
	if (mv->ptr == NULL) {
		free(mv);
		return NULL;
	}
	memcpy(mv->ptr, src->ptr, src->size);
	mv->size = src->size;
	return mv;
}

char *memvec_serialize(memvec_t *mv)
{
	char *s;
	
	s = (char *) malloc(mv->size + 1);
	if (s == NULL) {
		return NULL;
	}
	memcpy(s, mv->ptr, mv->size);
	s[mv->size] = '\0';
	return s;
}

#ifdef AA_MEMVEC_TEST

int main() 
{

	char *data = (char *) malloc(5);
	memcpy(data, "data", 5);
	memvec_t *mv1, *mv2;
	mv1 = memvec_new(data, strlen(data));
	char *str;
	if (mv1) {
		str = memvec_serialize(mv1);
		printf("mv1 data is '%s', data size is %d\n", str, mv1->size);
		free(str);
		
		mv2 = memvec_dup(mv1);
		if (mv2) {
			printf("cmp result is %d\n", memvec_cmp(mv1, mv2));
			printf("content cmp result is %d\n", memvec_cmp_content(mv1, mv2));
			printf("modify mv2 ...\n");
			mv2->ptr[3] = 'e';
			printf("cmp result is %d\n", memvec_cmp(mv1, mv2));
			printf("content cmp result is %d\n", memvec_cmp_content(mv1, mv2));

			memvec_delete(mv2);
		}
		memvec_delete(mv1);
	}
	return 0;
}

#endif
