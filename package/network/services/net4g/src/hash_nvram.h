#ifndef _hash_nvram_h_
#define _hash_nvram_h_

struct nvram_tuple {
	char *name;
	char *value;
	struct nvram_tuple *next;
};

struct nvram_handle {
	unsigned int length;
	unsigned int offset;
	struct nvram_tuple *nvram_hash[65];
	struct nvram_tuple *nvram_dead;
};

typedef struct nvram_handle nvram_handle_t;
typedef struct nvram_tuple  nvram_tuple_t;



/* Set the value of an NVRAM variable */
int hash_nvram_set(nvram_handle_t *h, const char *name, const char *value);

/* Get the value of an NVRAM variable. */
char * hash_nvram_get(nvram_handle_t *h, const char *name);

/* Unset the value of an NVRAM variable. */
int hash_nvram_unset(nvram_handle_t *h, const char *name);

/* Get all NVRAM variables. */
nvram_tuple_t * hash_nvram_getall(nvram_handle_t *h);

/* Regenerate NVRAM. */
int hash_nvram_commit(nvram_handle_t *h);

/* Open NVRAM and obtain a handle. */
nvram_handle_t *hash_nvram_open(const char *file, int rdonly);

/* Close NVRAM and free memory. */
int hash_nvram_close(nvram_handle_t *h);

/* Get the value of an NVRAM variable in a safe way, use "" instead of NULL. */
#define hash_nvram_safe_get(h, name) (hash_nvram_get(h, name) ? : "")


/* Helper macros */
#define NVRAM_ARRAYSIZE(a)	sizeof(a)/sizeof(a[0])
#define	NVRAM_ROUNDUP(x, y)	((((x)+((y)-1))/(y))*(y))

/* NVRAM constants */
#define NVRAM_SPACE			0x8000


#endif /* _hash_nvram_h_ */
