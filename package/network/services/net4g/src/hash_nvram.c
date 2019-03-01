#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/limits.h>

#include "hash_nvram.h"

/*
 * -- Helper functions --
 */

/* String hash */
static uint32_t hash(const char *s)
{
	uint32_t hash = 0;

	while (*s)
		hash = 31 * hash + *s++;

	return hash;
}

/* Free all tuples. */
static void _nvram_free(nvram_handle_t *h)
{
	uint32_t i;
	nvram_tuple_t *t, *next;

	/* Free hash table */
	for (i = 0; i < NVRAM_ARRAYSIZE(h->nvram_hash); i++) {
		for (t = h->nvram_hash[i]; t; t = next) {
			next = t->next;
			free(t);
			if(t->value) free(t->value);
		}
		h->nvram_hash[i] = NULL;
	}

	/* Free dead table */
	for (t = h->nvram_dead; t; t = next) {
		next = t->next;
		free(t);
	}

	h->nvram_dead = NULL;
}

/* (Re)allocate NVRAM tuples. */
static nvram_tuple_t * _nvram_realloc( nvram_handle_t *h, nvram_tuple_t *t,
	const char *name, const char *value )
{
	if ((strlen(value) + 1) > NVRAM_SPACE)
		return NULL;

	if (!t) {
		if (!(t = malloc(sizeof(nvram_tuple_t) + strlen(name) + 1))) {
			fprintf(stderr,"nvram malloc tuple_t error!\n");
			return NULL;
		}

		/* Copy name */
		t->name = (char *) &t[1];
		strcpy(t->name, name);

		t->value = NULL;
	}

	/* Copy value */
	if (!t->value || strcmp(t->value, value))
	{
		if(!(t->value = (char *) realloc(t->value, strlen(value)+1))) {
			fprintf(stderr,"nvram realloc value error!\n");
			return NULL;
		}

		strcpy(t->value, value);
		t->value[strlen(value)] = '\0';
	}

	return t;
}

#if 0
/* (Re)initialize the hash table. */
static int _nvram_rehash(nvram_handle_t *h)
{
	/* (Re)initialize hash table */
	_nvram_free(h);

	/* Parse and set "name=value\0 ... \0\0" */
	char *name = (char *) &header[1];

	for (; *name; name = value + strlen(value) + 1) {
		if (!(eq = strchr(name, '=')))
			break;
		*eq = '\0';
		value = eq + 1;
		hash_nvram_set(h, name, value);
		*eq = '=';
	}

	return 0;
}
#endif


/*
 * -- Public functions --
 */


/* Get the value of an NVRAM variable. */
char * hash_nvram_get(nvram_handle_t *h, const char *name)
{
	uint32_t i;
	nvram_tuple_t *t;
	char *value;

	if (!name)
		return NULL;

	/* Hash the name */
	i = hash(name) % NVRAM_ARRAYSIZE(h->nvram_hash);

	/* Find the associated tuple in the hash table */
	for (t = h->nvram_hash[i]; t && strcmp(t->name, name); t = t->next);

	value = t ? t->value : NULL;

	return value;
}

/* Set the value of an NVRAM variable. */
int hash_nvram_set(nvram_handle_t *h, const char *name, const char *value)
{
	uint32_t i;
	nvram_tuple_t *t, *u, **prev;

	/* Hash the name */
	i = hash(name) % NVRAM_ARRAYSIZE(h->nvram_hash);

	/* Find the associated tuple in the hash table */
	for (prev = &h->nvram_hash[i], t = *prev;
		 t && strcmp(t->name, name); prev = &t->next, t = *prev);

	/* (Re)allocate tuple */
	if (!(u = _nvram_realloc(h, t, name, value)))
		return -12; /* -ENOMEM */

	/* Value reallocated */
	if (t && t == u)
		return 0;

	/* Move old tuple to the dead table */
	if (t) {
		*prev = t->next;
		t->next = h->nvram_dead;
		h->nvram_dead = t;
	}

	/* Add new tuple to the hash table */
	u->next = h->nvram_hash[i];
	h->nvram_hash[i] = u;

	return 0;
}

/* Unset the value of an NVRAM variable. */
int hash_nvram_unset(nvram_handle_t *h, const char *name)
{
	uint32_t i;
	nvram_tuple_t *t, **prev;

	if (!name)
		return 0;

	/* Hash the name */
	i = hash(name) % NVRAM_ARRAYSIZE(h->nvram_hash);

	/* Find the associated tuple in the hash table */
	for (prev = &h->nvram_hash[i], t = *prev;
		 t && strcmp(t->name, name); prev = &t->next, t = *prev);

	/* Move it to the dead table */
	if (t) {
		*prev = t->next;
		t->next = h->nvram_dead;
		h->nvram_dead = t;
	}

	return 0;
}

/* Get all NVRAM variables. */
nvram_tuple_t * hash_nvram_getall(nvram_handle_t *h)
{
	int i;
	nvram_tuple_t *t, *l, *x;

	l = NULL;

	for (i = 0; i < NVRAM_ARRAYSIZE(h->nvram_hash); i++) {
		for (t = h->nvram_hash[i]; t; t = t->next) {
			if( (x = (nvram_tuple_t *) malloc(sizeof(nvram_tuple_t))) != NULL )
			{
				x->name  = t->name;
				x->value = t->value;
				x->next  = l;
				l = x;
			}
			else
			{
				break;
			}
		}
	}

	return l;
}

int hash_nvram_close(nvram_handle_t *h)
{
	_nvram_free(h);
	return 0;
}

/* Regenerate NVRAM. */
#if 0
int nvram_commit(nvram_handle_t *h)
{
	nvram_header_t *header = nvram_header(h);
	char *init, *config, *refresh, *ncdl;
	char *ptr, *end;
	int i;
	nvram_tuple_t *t;
	nvram_header_t tmp;
	uint8_t crc;

	/* Regenerate header */
	header->magic = NVRAM_MAGIC;
	header->crc_ver_init = (NVRAM_VERSION << 8);
	if (!(init = hash_nvram_get(h, "sdram_init")) ||
		!(config = hash_nvram_get(h, "sdram_config")) ||
		!(refresh = hash_nvram_get(h, "sdram_refresh")) ||
		!(ncdl = hash_nvram_get(h, "sdram_ncdl"))) {
		header->crc_ver_init |= SDRAM_INIT << 16;
		header->config_refresh = SDRAM_CONFIG;
		header->config_refresh |= SDRAM_REFRESH << 16;
		header->config_ncdl = 0;
	} else {
		header->crc_ver_init |= (strtoul(init, NULL, 0) & 0xffff) << 16;
		header->config_refresh = strtoul(config, NULL, 0) & 0xffff;
		header->config_refresh |= (strtoul(refresh, NULL, 0) & 0xffff) << 16;
		header->config_ncdl = strtoul(ncdl, NULL, 0);
	}

	/* Clear data area */
	ptr = (char *) header + sizeof(nvram_header_t);
	memset(ptr, 0xFF, NVRAM_SPACE - sizeof(nvram_header_t));
	memset(&tmp, 0, sizeof(nvram_header_t));

	/* Leave space for a double NUL at the end */
	end = (char *) header + NVRAM_SPACE - 2;

	/* Write out all tuples */
	for (i = 0; i < NVRAM_ARRAYSIZE(h->nvram_hash); i++) {
		for (t = h->nvram_hash[i]; t; t = t->next) {
			if ((ptr + strlen(t->name) + 1 + strlen(t->value) + 1) > end)
				break;
			ptr += sprintf(ptr, "%s=%s", t->name, t->value) + 1;
		}
	}

	/* End with a double NULL and pad to 4 bytes */
	*ptr = '\0';
	ptr++;

	if( (int)ptr % 4 )
		memset(ptr, 0, 4 - ((int)ptr % 4));

	ptr++;

	/* Set new length */
	header->len = NVRAM_ROUNDUP(ptr - (char *) header, 4);

	/* Little-endian CRC8 over the last 11 bytes of the header */
	tmp.crc_ver_init   = header->crc_ver_init;
	tmp.config_refresh = header->config_refresh;
	tmp.config_ncdl    = header->config_ncdl;
	crc = hndcrc8((unsigned char *) &tmp + NVRAM_CRC_START_POSITION,
		sizeof(nvram_header_t) - NVRAM_CRC_START_POSITION, 0xff);

	/* Continue CRC8 over data bytes */
	crc = hndcrc8((unsigned char *) &header[0] + sizeof(nvram_header_t),
		header->len - sizeof(nvram_header_t), crc);

	/* Set new CRC8 */
	header->crc_ver_init |= crc;

	/* Write out */
	msync(h->mmap, h->length, MS_SYNC);
	fsync(h->fd);

	/* Reinitialize hash table */
	return _nvram_rehash(h);
}
#endif

