#include "aa_bufferlist.h"


buffer_node_t * buffer_new_node(void *data, size_t size)
{
	buffer_node_t *bn;
	bn = (buffer_node_t *) malloc(sizeof(buffer_node_t));;
	if (bn == NULL) {
		return NULL;
	}
	bn->start = data;
	bn->pos = data;
	bn->size = size;
	return bn;
}

buffer_list_t *buffer_new(uint32_t bufsize)
{
	buffer_list_t *bl;
	buffer_node_t *bn;
	llist_t *ll;

	if (bufsize == 0) {
		bufsize = DEFAULT_BUFSIZE;
	}
	bl = (buffer_list_t *) malloc(sizeof(buffer_list_t));
	if (bl == NULL) {
		return NULL;
	}
	
	ll = llist_new(bufsize);
	if (ll == NULL) {
		free(bl);
	}

	bl->base = ll;
	bl->max = bufsize;
	bl->bufsize = 0;
	bl->nbytes = 0;

	return bl;
}


int buffer_delete(buffer_list_t* bl)
{
	if (bl) {
		if (bl->bufsize != 0) {
			return -1;
		}
		if (bl->base) {
			llist_delete(bl->base);
		}
		free(bl);
		return 0;
	}
	return -1;
}

size_t buffer_nbytes(buffer_list_t *bl)
{
	return bl->nbytes;
}

int buffer_pop(buffer_list_t *bl) 
{
	int ret;
	buffer_node_t *bn;

	ret = llist_fetch_head(bl->base, (void **)&bn);
	if (ret != 0) {
		return ret;
	}

	bl->nbytes -= bn->size;
	free(bn->start);
	free(bn);
	bl->bufsize--;

	return 0;
}

int buffer_move_head(buffer_list_t *bl, size_t size)
{
	int ret;
	buffer_node_t *bn;

	ret = llist_fetch_head(bl->base, (void **)&bn);
	if (ret != 0) {
		return ret;
	}

	if (bn->size > size) {
		bn->pos += size;
		bn->size -= size;
		bl->nbytes -= size;

		return 0;
	}

	return -1;
}

ssize_t buffer_read(buffer_list_t *bl, void *data, size_t size)
{
	ssize_t nbytes;
	buffer_node_t *bn;
	int ret;
	ssize_t res = 0;

	while (bl->nbytes > 0 && size > 0) {
		ret = llist_get_head(bl->base, (void **)&bn);
		if (ret != 0) {
			res = -1;
			break;
		}
		if (bn->size <= size) {
			/* delete the node */
			ret = llist_fetch_head(bl->base, (void **)&bn);
			if (ret != 0) {
				res = -1;
				break;
			}
			memcpy(data + res, bn->pos, bn->size);
			res += bn->size;
			size -= bn->size;
			bl->nbytes -= bn->size;
			free(bn->start);
			free(bn);
			bl->bufsize--;
		} else {
			/* modify data pos */
			memcpy(data + res, bn->pos, size);
			res += size;
			bn->pos += size;
			bn->size -= size;
			bl->nbytes -= size;
			size = 0;
		}
	}

	return res;
}

ssize_t buffer_write(buffer_list_t *bl, const void *buf, size_t size)
{
	buffer_node_t *bn;
	void *data;
	int ret;

	data = malloc(size);
	if (data == NULL) {
		return -1;
	}
	memcpy(data, buf, size);
	bn = buffer_new_node(data, size);
	if (bn == NULL) {
		free(data);
		return -1;
	}
	ret = llist_append(bl->base, bn);
	if (ret != 0) {
		free(data);
		free(bn);
		return -1;
	}

	bl->bufsize++;
	bl->nbytes += size;
	return size;
}

void * buffer_get_next(buffer_list_t *bl, void *ptr)
{
	llist_node *ln;
	ln = llist_get_next_nb(bl->base, ptr);
	return ln; 
}

void * buffer_get_head(buffer_list_t *bl)
{
	llist_node *ln;
	int ret;
	ret = llist_get_head_nb(bl->base, &ln);
	if (ret != 0) {
		return NULL;
	}
	return ln;
}

void * buffer_get_data(void *buf)
{
	return ((llist_node *)buf)->ptr;
}

#define AA_BUFFERLIST_TEST
#ifdef AA_BUFFERLIST_TEST

int main()
{
	char *str1 = "hello";
	char *str2 = "world";
	buffer_list_t *bl;
	int ret;
	char *buf;
	int len1 = strlen(str1);
	int len2 = strlen(str2);

	buf = (char *) malloc(len1 + len2 + 1);
	if (!buf) 
		return -1;

	bl = buffer_new(100);
	if (bl) {
		/* test 1 */
		ret = buffer_write(bl, str1, len1);
		printf("write %d bytes\n", ret);
		ret = buffer_write(bl, str2, len2);
		printf("write %d bytes\n", ret);
		ret = buffer_read(bl, buf, len1 + len2);
		buf[len1 + len2] = 0;
		printf("read %d bytes, data is '%s'\n", ret, buf);
		printf("buffer nbytes is %d\n", buffer_nbytes(bl));
		/* test 2 */	
		ret = buffer_write(bl, str1, len1);
		printf("write %d bytes\n", ret);
		ret = buffer_write(bl, str2, len2);
		printf("write %d bytes\n", ret);
		while ((ret = buffer_read(bl, buf, 3)) > 0) {
			buf[3] = 0;
			printf("read %d bytes, data is '%s'\n", ret, buf);
		}
	}
	free(buf);
	buffer_delete(bl);

	return 0;
}

#endif
