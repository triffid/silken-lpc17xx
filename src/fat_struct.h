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
 * FAT12 superblock
 */
typedef struct __attribute__ ((packed))
{
	uint8_t  jump[3];
	char     oem_id[8];
	uint16_t bytes_per_sector;		// usually 512
	uint8_t  sectors_per_cluster;
	uint16_t num_boot_sectors;		// usually 1
	uint8_t  num_fats;				// usually 2
	uint16_t num_root_dir_ents;
	uint16_t total_sectors;			// 0 if num_sectors > 65535
	uint8_t  media_id;				// usually 0xF0
	uint16_t sectors_per_fat;
	uint16_t sectors_per_track;
	uint16_t heads;
	uint16_t hidden_sectors;		// root dir entry? usually 2
	uint8_t  mbr[480];
	uint16_t magic;					// always 0xAA55
} _fat12_volid;

/*
 * FAT16 superblock
 */
typedef struct __attribute__ ((packed))
{
	uint8_t  jump[3];												// 0-3
	char     oem_id[8];												// 4-12
	uint16_t bytes_per_sector;		// usually 512					// 13-14
	uint8_t  sectors_per_cluster;									// 15
	uint16_t num_boot_sectors;		// usually 1					// 16-17
	uint8_t  num_fats;				// usually 2					// 18
	uint16_t num_root_dir_ents;										// 19-20
	uint16_t total_sectors;			// 0 if num_sectors > 65535		// 21-22
	uint8_t  media_id;				// usually 0xF0					// 23
	uint16_t sectors_per_fat;										// 24-25
	uint16_t sectors_per_track;										// 26-27
	uint16_t heads;													// 28-29
	uint16_t hidden_sectors;		// root dir entry? usually 2	// 30-31

	// fat12 legacy superblock ends here

	uint32_t total_sectors_32;		// 0 if num_sectors < 65536		// 32-25
	uint8_t  logical_drive_num;										// 36
	uint8_t  reserved0;												// 37
	uint8_t  extended_sig;			// 0x28 or 0x29 indicates validity of next 3 fields // 38
	uint32_t serial_number;											// 39-42
	char     vol_label[11];											// 43-53
	char     fstype[8];				// "FAT32" or "FAT16" or "FAT" or zeros

	uint8_t  mbr[448];
	uint16_t magic;					// always 0xAA55
} _fat16_volid;

/*
 * FAT32 superblock
 * first sector of filesystem
 */
typedef struct __attribute__ ((packed))
{
	uint8_t  jump[3];
	uint8_t  oem_id[8];				// eg "MSWIN4.0"
	uint16_t bytes_per_sector;		// usually 512
	uint8_t  sectors_per_cluster;	// 1,2,4,8,...,128
	uint16_t n_reserved_sectors;	// usually 32
	uint8_t  n_fats;				// always 2
	uint8_t  irrelevant1[19];
	uint32_t sectors_per_fat;		// depends on disk size
	uint8_t  irrelevant2[4];
	uint32_t root_dir_cluster;		// usually 2
	uint32_t total_sectors;			// disk size / bytes_per_sector
	uint8_t  irrelevant3[458];
	uint16_t magic;
} _fat32_volid;

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
			uint8_t  irrelevant0 :1;
			uint8_t  final       :1;
			uint8_t  irrelevant1 :2;
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

struct __attribute__ ((packed))
_fat_mount_ioresult : _fat_ioresult
{
	char label[12];
	uint32_t lba_start;
	_fat_ioreceiver* owner;
};


#endif /* _FAT_STRUCT_H */
