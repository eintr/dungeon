#include "aa_string.h"


string_t *string_fromc(const char *str)
{
	return memvec_new(str, strlen(str));
}

char *string_toc(string_t *s)
{
	char *tmp;

	tmp = malloc(s->size+1);
	if (tmp==NULL) {
		return NULL;
	}
	memcpy(tmp, s->ptr, s->size);
	tmp[s->size] = '\0';
	return tmp;
}

/*
int string_frommemvec(string_t *result, const memvec_t *src)
{
	result->ptr = malloc(src->size);
	if (result->ptr==NULL) {
		return -1;
	}
	memcpy(result->ptr, src->ptr, src->size);
	result->size = src->size;
	return 0;
}
*/

int string_cmp(string_t *s1, string_t *s2)
{
	return memvec_cmp_content(s1, s2);
}

int string_cpy(string_t *dest, string_t *src)
{
	if (dest->ptr) {
		free(dest->ptr);
	}
	dest->ptr = strdup(src->ptr);
	dest->size = src->size;

	return 0;
}

int string_delete(string_t *s)
{
	return memvec_delete(s);
}

#ifdef AA_STRING_TEST

int main() 
{

	char *data1 = (char *) malloc(5);
	memcpy(data1, "data", 5);
	char *data2 = (char *) malloc(5);
	memcpy(data2, "what", 5);
	string_t *s1, *s2;
	s1 = string_fromc(data1);
	s2 = string_fromc(data2);
	char *str;
	if (s1) {
		str = string_toc(s1);
		printf("s1 data is '%s', data size is %d\n", str, s1->size);
		free(str);
		str = string_toc(s2);
		printf("s2 data is '%s', data size is %d\n", str, s2->size);
		free(str);
		
		string_cpy(s2, s1);
		if (s2) {
			printf("cmp result is %d\n", string_cmp(s1, s2));
			printf("modify s2 ...\n");
			s2->ptr[3] = 'e';
			printf("cmp result is %d\n", string_cmp(s1, s2));

			string_delete(s2);
		}
		string_delete(s1);
	}
	return 0;
}

#endif
