#ifndef _FAT_STRUCT_H
#define _FAT_STRUCT_H

#define _str(x) #x
#define str(x) _str(x)

/*
 * data structures on disk
 */

// partition headers in MBR
typedef struct __attribute__ ((packed))
{
	uint8_t  boot_flag;
	uint8_t  chs_begin[3];
	uint8_t  type;
	uint8_t  chs_end[3];
	uint32_t lba_begin;
	uint32_t n_sectors;
} _fat_partition;

// MBR itself
typedef struct __attribute__ ((packed))
{
	uint8_t mbr[446];
	_fat_partition partition[4];
	uint16_t magic;
} _fat_bootblock;

/*
 * FAT superblock
 */
typedef struct __attribute__ ((packed))
{
	// common fields:
	uint8_t  jump[3];												// 0-2
	char     oem_id[8];												// 3-10
	uint16_t bytes_per_sector;		// usually 512					// 11-12
	uint8_t  sectors_per_cluster;									// 13
	uint16_t num_boot_sectors;		// usually 1					// 14-15
	uint8_t  num_fats;				// usually 2					// 16
	uint16_t num_root_dir_ents;										// 17-18
	uint16_t total_sectors;			// 0 if num_sectors > 65535		// 19-20
	uint8_t  media_id;				// usually 0xF0					// 21
	uint16_t sectors_per_fat;										// 22-23
	uint16_t sectors_per_track;										// 24-25
	uint16_t heads;													// 26-27

	uint32_t hidden_sectors;										// 28-31
	uint32_t total_sectors_32;										// 32-35

	union {
		uint8_t  mbr[474];

		struct __attribute__ ((packed))
		{
			uint8_t drive_number;
			uint8_t mount_state;
		} fat16;

		struct __attribute__ ((packed))
		{
			uint32_t sectors_per_fat_32;
			uint16_t flags;
			uint8_t  version[2];
			uint32_t root_dir_cluster;
			uint16_t fsinfo_sector;
			uint16_t backup_boot_sector;
			uint16_t reserved[6];
			uint8_t  drive_number;
			uint8_t  mount_state;
		} fat32;
	};
	uint16_t magic;                 // always ntohs(0x55AA)			// 511-512
} _fat_volid;

/*
 * Directory entries
 *
 * 32 bytes long
 */
typedef struct __attribute__ ((packed))
{
	uint8_t  name[11];
	uint8_t  attr;
	uint8_t  irrelevant0[8];
	uint16_t ch;
	uint8_t  irrelevant1[4];
	uint16_t cl;
	uint32_t size;
} _fat_direntry;

typedef struct __attribute__ ((packed))
{
	union {
		uint8_t flags;
		struct {
			uint8_t  sequence    :4;
			uint8_t  irrelevant0 :2;
			uint8_t  final       :1;
			uint8_t  irrelevant1 :1;
		};
	};

	uint16_t name0[5];

	uint8_t  attr; // always 0xF
	uint8_t  type; // always 0
	uint8_t  checksum;
	uint16_t name1[6];
	uint16_t cs;   // always 0
	uint16_t name2[2];
} _fat_lfnentry;

typedef struct __attribute__ ((packed))
{
	char* path;

	uint32_t root_cluster;

	uint32_t direntry_cluster;
	uint8_t  direntry_index;

	/*
	 * where are we now?
	 */
	uint32_t current_cluster;
	uint32_t byte_in_cluster;
	uint32_t cluster_index;

	// overall position = cluster_index * sectors_per_cluster * bytes_per_sector + byte_in_cluster
	// when (byte_in_cluster >= sectors_per_cluster * bytes_per_sector + byte_in_cluster)
	// it's time to fetch next cluster

	union {
		uint32_t size;
		uint32_t pathname_traversed_bytes;
	};
} FIL;

typedef enum
{
	IOACTION_NULL,
	IOACTION_MOUNT,
	IOACTION_OPEN,
	IOACTION_READ_ONE,
	IOACTION_WRITE_ONE,
	IOACTION_SEEK,
} _fat_ioaction;

class Fat;
class _fat_ioreceiver;
struct __fat_ioresult;

struct __attribute__ ((packed))
_fat_ioresult
{
	uint32_t lba;
	uint8_t  action:6;
	uint8_t  ready :1;
	uint8_t  fini  :1;

	uint8_t* buffer;
	uint32_t buflen;

	_fat_ioreceiver* owner;

	_fat_ioresult* next;
};

struct _fat_file_ioresult;
struct __attribute__ ((packed))
_fat_traverse_ioresult : _fat_ioresult
{
	uint32_t size;
	uint32_t cluster;

	_fat_file_ioresult* child;
};

struct __attribute__ ((packed))
_fat_file_ioresult : _fat_ioresult
{
	char*    path;
	uint8_t  path_traverse;

	uint32_t bytes_remaining;

	FIL      file;

	_fat_traverse_ioresult traverse;
};

enum _fat_mount_stage_t {
	FAT_MOUNT_STAGE_SUPERBLOCK,
	FAT_MOUNT_STAGE_ROOT_DIR
};

struct __attribute__ ((packed))
_fat_mount_ioresult : _fat_ioresult
{
	char label[12];
	enum _fat_mount_stage_t stage;
	uint32_t lba_start;
	uint32_t root_dir_end;

	_fat_ioreceiver* owner;
};


#endif /* _FAT_STRUCT_H */
