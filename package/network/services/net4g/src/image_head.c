#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "image.h"
#include "ota.h"

extern uint32_t crc32(uint32_t crc, unsigned char *buf, uint32_t len);

typedef struct table_entry {
	int	val;		/* as defined in image.h	*/
	char	*sname;		/* short (input) name		*/
	char	*lname;		/* long (output) name		*/
} table_entry_t;

table_entry_t arch_name[] = {
    {	IH_CPU_INVALID,		NULL,		"Invalid CPU",	},
    {	IH_CPU_ALPHA,		"alpha",	"Alpha",	},
    {	IH_CPU_ARM,		"arm",		"ARM",		},
    {	IH_CPU_I386,		"x86",		"Intel x86",	},
    {	IH_CPU_IA64,		"ia64",		"IA64",		},
    {	IH_CPU_M68K,		"m68k",		"MC68000",	},
    {	IH_CPU_MICROBLAZE,	"microblaze",	"MicroBlaze",	},
    {	IH_CPU_MIPS,		"mips",		"MIPS",		},
    {	IH_CPU_MIPS64,		"mips64",	"MIPS 64 Bit",	},
    {	IH_CPU_PPC,		"ppc",		"PowerPC",	},
    {	IH_CPU_S390,		"s390",		"IBM S390",	},
    {	IH_CPU_SH,		"sh",		"SuperH",	},
    {	IH_CPU_SPARC,		"sparc",	"SPARC",	},
    {	IH_CPU_SPARC64,		"sparc64",	"SPARC 64 Bit",	},
    {	-1,			"",		"",		},
};

table_entry_t os_name[] = {
    {	IH_OS_INVALID,	NULL,		"Invalid OS",		},
    {	IH_OS_4_4BSD,	"4_4bsd",	"4_4BSD",		},
    {	IH_OS_ARTOS,	"artos",	"ARTOS",		},
    {	IH_OS_DELL,	"dell",		"Dell",			},
    {	IH_OS_ESIX,	"esix",		"Esix",			},
    {	IH_OS_FREEBSD,	"freebsd",	"FreeBSD",		},
    {	IH_OS_IRIX,	"irix",		"Irix",			},
    {	IH_OS_LINUX,	"linux",	"Linux",		},
    {	IH_OS_LYNXOS,	"lynxos",	"LynxOS",		},
    {	IH_OS_NCR,	"ncr",		"NCR",			},
    {	IH_OS_NETBSD,	"netbsd",	"NetBSD",		},
    {	IH_OS_OPENBSD,	"openbsd",	"OpenBSD",		},
    {	IH_OS_PSOS,	"psos",		"pSOS",			},
    {	IH_OS_QNX,	"qnx",		"QNX",			},
    {	IH_OS_RTEMS,	"rtems",	"RTEMS",		},
    {	IH_OS_SCO,	"sco",		"SCO",			},
    {	IH_OS_SOLARIS,	"solaris",	"Solaris",		},
    {	IH_OS_SVR4,	"svr4",		"SVR4",			},
    {	IH_OS_U_BOOT,	"u-boot",	"U-Boot",		},
    {	IH_OS_VXWORKS,	"vxworks",	"VxWorks",		},
    {	-1,		"",		"",			},
};

table_entry_t type_name[] = {
    {	IH_TYPE_INVALID,    NULL,	  "Invalid Image",	},
    {	IH_TYPE_FILESYSTEM, "filesystem", "Filesystem Image",	},
    {	IH_TYPE_FIRMWARE,   "firmware",	  "Firmware",		},
    {	IH_TYPE_KERNEL,	    "kernel",	  "Kernel Image",	},
    {	IH_TYPE_MULTI,	    "multi",	  "Multi-File Image",	},
    {	IH_TYPE_RAMDISK,    "ramdisk",	  "RAMDisk Image",	},
    {	IH_TYPE_SCRIPT,     "script",	  "Script",		},
    {	IH_TYPE_STANDALONE, "standalone", "Standalone Program", },
    {	-1,		    "",		  "",			},
};

table_entry_t comp_name[] = {
    {	IH_COMP_NONE,	"none",		"uncompressed",		},
    {	IH_COMP_BZIP2,	"bzip2",	"bzip2 compressed",	},
    {	IH_COMP_GZIP,	"gzip",		"gzip compressed",	},
    {	IH_COMP_LZMA,	"lzma",		"lzma compressed",	},
    {	-1,		"",		"",			},
};

static	void	print_header (image_header_t *);
static	void	print_type (image_header_t *);
static	char	*put_table_entry (table_entry_t *, char *, int);
static	char	*put_arch (int);
static	char	*put_type (int);
static	char	*put_os   (int);
static	char	*put_comp (int);


static char *put_table_entry (table_entry_t *table, char *msg, int type)
{
	for (; table->val>=0; ++table) {
		if (table->val == type)
			return (table->lname);
	}
	return (msg);
}

static char *put_arch (int arch)
{
	return (put_table_entry(arch_name, "Unknown Architecture", arch));
}

static char *put_os (int os)
{
	return (put_table_entry(os_name, "Unknown OS", os));
}

static char *put_type (int type)
{
	return (put_table_entry(type_name, "Unknown Image", type));
}

static char *put_comp (int comp)
{
	return (put_table_entry(comp_name, "Unknown Compression", comp));
}

static void print_type (image_header_t *hdr)
{
	LOG ("%s %s %s (%s)\n",
			put_arch (hdr->ih_arch),
			put_os   (hdr->ih_os  ),
			put_type (hdr->ih_type),
			put_comp (hdr->ih_comp)
	);
}

static void print_header (image_header_t *hdr)
{
	time_t timestamp;
	uint32_t size;

	timestamp = (time_t)ntohl(hdr->ih_time);
	size = ntohl(hdr->ih_size);

	LOG ("Image Name:   %.*s\n", IH_NMLEN, hdr->ih_name);
	LOG ("Created:      %s", ctime(&timestamp));
	LOG ("Image Type:   "); print_type(hdr);
	LOG ("Data Size:    %d(0x%08x) Bytes = %.2f kB = %.2f MB\n",
		size,size, (double)size / 1.024e3, (double)size / 1.048576e6 );
	LOG ("Load Address: 0x%08X\n", ntohl(hdr->ih_load));
	LOG ("Entry Point:  0x%08X\n", ntohl(hdr->ih_ep));
	LOG ("Kernel Size:  0x%08X\n", ntohl(hdr->ih_ksz));
	
	LOG ("Head Magic:   0x%08X\n", ntohl(hdr->ih_magic));
	LOG ("Head Crc:     0x%08X\n", ntohl(hdr->ih_hcrc));
	LOG ("Data Crc:     0x%08X\n", ntohl(hdr->ih_dcrc));
}

int check_image(char *image_file)
{
	image_header_t headinfo;
	unsigned int checksum;
	int fd,ret;
	
	fd = open(image_file, O_RDONLY);
	if (fd <= 0) {
		LOG("Open image file error\n");
		return -1;
	}
	ret = read(fd,&headinfo,sizeof(image_header_t));
	if (ret != sizeof(image_header_t)) {
		LOG("Read image file error\n");
		close(fd);
		return -1;
	}
	
	print_header(&headinfo);
	
	unsigned int image_hcrc = ntohl(headinfo.ih_hcrc);
	//must be init ih_hcrc ,to calc right crc
	headinfo.ih_hcrc = 0;
	checksum = crc32(0,(unsigned char *)&headinfo,sizeof(image_header_t));	
	
	if (checksum != image_hcrc) {
		LOG("Bad image head_data crc: 0x%08X\n",checksum);
		close(fd);
		return -1;
	}
	LOG("[OK]Cacl ori head crc: 0x%08X\n",checksum);
	
	struct stat fd_stat;
	if (fstat(fd, &fd_stat) < 0) {
		LOG("Stat Image file error\n");
		close(fd);
		return -1;
	}
	LOG("The whole image file size=%ld\n",fd_stat.st_size);
	
	int kernel_size = ntohl(headinfo.ih_size);
	if (fd_stat.st_size - sizeof(image_header_t) != kernel_size) {
		//LOG("Image content size error\n");
		//return -1;
		if(kernel_size < 3000000) //kernel size < 3M, wrt Image
		{
			LOG("This is a WRT Image!size=%ld(0x%x)\n",kernel_size,kernel_size);
		} else {
			LOG("This is a SDK uImage!size=%ld(0x%x)\n",kernel_size,kernel_size);
			LOG("Image Content check ERROR!\n");
			return -1;
		}
	} else {
		LOG("This is a SDK uImage!size=%ld(0x%x)\n",kernel_size,kernel_size);
	}
	
	unsigned char *mptr = NULL;
	mptr = (unsigned char *)mmap(0, fd_stat.st_size,PROT_READ, MAP_SHARED, fd, 0);
	
	if ((caddr_t)mptr == (caddr_t)-1) {
		LOG("mmap error\n");
		close(fd);
		return -1;
	}
	
	/* move pointer to data starting erea*/
	mptr += sizeof(image_header_t);
	
	checksum = crc32(0,mptr,kernel_size);
	if (checksum != ntohl(headinfo.ih_dcrc)) {
		LOG("Bad kernel data crc: 0x%08X\n",checksum);
		munmap((void *)mptr,fd_stat.st_size);
		close(fd);
		return -1;
	}
	LOG("[OK]Cacl Kernel data crc:   0x%08X\n",checksum);
	
	munmap((void *)mptr,fd_stat.st_size);
	close(fd);
	
	return 0;
}
