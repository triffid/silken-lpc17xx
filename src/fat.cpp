#include "fat.h"

#include "platform_memory.h"
#include "platform_utils.h"

#include <cstdlib>
#include <cstdio>
#include <cstddef>
#include <cstring>

#include <unistd.h>

void dump_buffer(uint8_t* buffer)
{
	for (int i = 0; i < 32; i++)
	{
		printf("0x%04x| ", i * 16);

		for (int j = 0; j < 16; j++)
			printf("%02X ", buffer[(i * 16) + j]);
		printf(" | ");
		for (int j = 0; j < 16; j++)
			if (buffer[(i * 16) + j] >= 32 && buffer[(i * 16) + j] < 127)
				printf("%c", buffer[(i * 16) + j]);
			else
				printf(".");

			printf("\n");
	}
}

const char* action_name(_fat_ioaction i)
{
	switch(i)
	{
		case IOACTION_NULL:
			return str(IOACTION_NULL);
		case IOACTION_MOUNT:
			return str(IOACTION_MOUNT);
		case IOACTION_OPEN:
			return str(IOACTION_OPEN);
		case IOACTION_READ_ONE:
			return str(IOACTION_READ_ONE);
		default:
			return "?";
	}
}

Fat::Fat()
{
	sd = NULL;
	fat_buf = dentry_buf = NULL;
	work_queue = NULL;

	fat_lba = dentry_lba = 0xFFFFFFFF;

	fat_begin_lba       = 0;
	cluster_begin_lba   = 0;
	sectors_per_cluster = 0;
	root_dir_sector     = 0;
	fat_type            = 0;
}

void Fat::f_mount(_fat_mount_ioresult* w, SD* sd)
{
	if (fat_buf)
		free(fat_buf);
	if (dentry_buf && dentry_buf != fat_buf)
		free(dentry_buf);

	this->sd = sd;

// 	fat_buf = (uint8_t*) malloc(512);
	fat_buf = (uint8_t*) AHB0.alloc(512);
	fat_lba = -1;

// 	dentry_buf = (uint8_t*) malloc(512);
// 	dentry_buf = fat_buf;
	dentry_buf = (uint8_t*) AHB0.alloc(512);
	dentry_lba = -1;

	w->action   = IOACTION_MOUNT;
	w->lba      = 0;
	w->buffer   = dentry_buf;
	w->buflen   = 512;
	w->owner    = NULL;
	w->ready    = 1;
	w->fini     = 0;
	w->next     = NULL;
	w->stage    = FAT_MOUNT_STAGE_SUPERBLOCK;

	if (work_queue)
	{
		printf("Error! Already mounted and I/O in progress! umount first!\n");
// 		f_umount();
		exit(1);
	}

	work_queue = w;

	this->sd->begin_read(w->lba, w->buffer, this);
}

int Fat::f_mounted()
{
	if (fat_begin_lba == 0)
		return 0;
	if (cluster_begin_lba == 0)
		return 0;
	if (sectors_per_cluster == 0)
		return 0;
	if (root_dir_sector == 0)
		return 0;
	if (fat_type == 0)
		return 0;
	return 1;
}

uint32_t Fat::cluster_to_lba(uint32_t cluster)
{
	return cluster_begin_lba + (cluster - 2) * sectors_per_cluster;
}

uint32_t Fat::lba_to_cluster(uint32_t lba)
{
	return (lba - cluster_begin_lba) / sectors_per_cluster + 2;
}

void Fat::enqueue(_fat_ioresult* ior)
{
	queue_walk();

	// TODO: atomic
	_fat_ioresult* w = work_queue;
	ior->next = NULL;
	ior->fini = 0;
	if (w)
	{
		while (w->next)
		{
			if (w == ior)
				return;
			w = w->next;
		}
		if (w == ior)
			return;
		w->next = ior;
	}
	else
	{
		work_queue = w = ior;

		if ((w->lba == dentry_lba) && (w->buffer == dentry_buf))
			process_buffer(w->buffer, w->lba);
		else if ((w->lba == fat_lba) && (w->buffer == fat_buf))
			process_buffer(w->buffer, w->lba);
		else
			sd->begin_read(w->lba, w->buffer, this);

		// else do nothing
	}

	queue_walk();
}

void Fat::dequeue(_fat_ioresult* w)
{
	if (work_queue == w)
		work_queue = w->next;
	else
	{
		_fat_ioresult* j = work_queue;
		while (j->next && j->next != w)
			j = j->next;
		if (j)
			j->next = w->next;
	}

	w->fini = 1;
}

int  Fat::f_open( _fat_file_ioresult* ior, const char* path)
{
	// skip leading slash
	while (path[0] == '/')
		path++;

	int l = strlen(path);
	ior->file.path = (char*) malloc(l + 1);
	strncpy(ior->file.path, path, l);

	ior->action    = IOACTION_OPEN;
	ior->ready     = 1;
	if (ior->buffer == NULL)
	{
		ior->buffer    = dentry_buf;
		ior->buflen    = 512;
	}
	ior->lba       = root_dir_sector;

	ior->file.root_cluster     = 0;
	ior->file.direntry_cluster = 0;
	ior->file.direntry_index   = 0;
	ior->file.current_cluster  = 0;
	ior->file.byte_in_cluster  = 0;
	ior->file.pathname_traversed_bytes = 0;

	ior->next = NULL;

	printf("FAT: Open %s\n", path);
	enqueue(ior);

	return 0;
}

int  Fat::f_read( _fat_file_ioresult* ior, void* buffer, uint32_t buflen)
{
	printf("FAT: READ %s (%p)!\n", ior->file.path, ior);

	ior->action = IOACTION_READ_ONE;

	ior->buffer = (uint8_t*) buffer;
	ior->buflen = buflen;

	if (ior->file.byte_in_cluster >= 512 * sectors_per_cluster)
	{
		if (fat_cache(fat_begin_lba + (ior->file.current_cluster >> 7)))
		{
			ior->file.current_cluster = ((uint32_t*) fat_buf)[ior->file.current_cluster & 0x7F];
			ior->file.cluster_index++;
			ior->file.byte_in_cluster -= (512 * sectors_per_cluster);
		}
	}

	ior->bytes_remaining = ior->buflen;

	ior->lba    = cluster_to_lba(ior->file.current_cluster) + (ior->file.byte_in_cluster >> 9);

	enqueue(ior);

	return 0;
}

int  Fat::f_write(_fat_file_ioresult* ior, void* buffer, uint32_t buflen)
{
	printf("f_write: unimplementeed\n");
	return 0;
}

int  Fat::f_close(_fat_file_ioresult* ior)
{
	return 0;
}

// void Fat::_sd_callback(_sd_work_stack* w)
void Fat::sd_read_complete(SD*, uint32_t sector, void* buf, int err)
{
	if (err == 0)
	{
		printf("FAT: lba %lu read ok\n", sector);

		process_buffer((uint8_t*) buf, sector);

		printf("FAT: end process lba %lu\n", sector);
		return;
	}

	printf("FAT: lba %lu read ERROR!\n", sector);
}

void Fat::sd_write_complete(SD*, uint32_t sector, void* buf, int err)
{
	// TODO:
}

void Fat::process_buffer(uint8_t* buffer, uint32_t lba)
{
	printf("FAT: --PROCBUF-- (%p lba %lu)\n", buffer, lba);

	for (int i = 0; i < 512; i++)
	{
		printf("0x%X%c", buffer[i], ((i & 31) == 31)?'\n':' ');
	}

	if (work_queue == NULL)
		return;

	_fat_ioresult* w = work_queue;

	if (buffer == fat_buf)
		fat_lba = lba;
	if (buffer == dentry_buf)
		dentry_lba = lba;

	printf("FAT: action is %u (%s)\n", w->action, action_name((_fat_ioaction) w->action));

	switch(w->action)
	{
		case IOACTION_MOUNT:
			ioaction_mount((_fat_mount_ioresult*) w, buffer, lba);
			break;
		case IOACTION_OPEN:
			ioaction_open((_fat_file_ioresult*) w, buffer, lba);
			break;
		case IOACTION_READ_ONE:
			ioaction_read_one((_fat_file_ioresult*) w, buffer, lba);
			break;
	}
}

void Fat::ioaction_mount(_fat_mount_ioresult* w, uint8_t* buffer, uint32_t lba)
{
	while ((w->stage == FAT_MOUNT_STAGE_ROOT_DIR) && (lba >= root_dir_sector) && (lba <= w->root_dir_end))
	{
		printf("FAT: got Root Dir at LBA %lu, searching for Volume Label\n", lba);
		_fat_direntry* d = (_fat_direntry*) buffer;
		// TODO: don't assume that volume label is in first sector of root cluster
		for (int i = 0; i < 16; i++)
		{
			if ((d[i].name[0] == 0) || (w->lba >= w->root_dir_end))
			{
				printf("FAT: mount succeeded! End of Root Dir, no label found\n");
				w->label[0] = 0;
				w->fini = 1;
				dequeue(w);
				return;
			}

			if (d[i].attr == 0x0F)
			{
				char name[14];
				// LFN entry
				_fat_lfnentry* lfn = (_fat_lfnentry*) &d[i];
				for (int j = 0; j < 13; j++)
				{
					if ((j >= 0) && (j <= 4))
						name[j] = lfn->name0[j];
					if ((j >= 5) && (j <= 10))
						name[j] = lfn->name1[j - 5];
					if ((j >= 11) && (j <= 12))
						name[j] = lfn->name2[j - 11];
					if ((name[j] > 127) || (name[j] < 32))
						name[j] = '?';
				}
				name[13] = 0;
				if (d[i].name[0] == 0xE5)
					printf("\tLFN:     %s [deleted]\n", name);
				else
					printf("\tLFN: (%d) %s %s\n", lfn->sequence, name, (lfn->final?"[last]":""));
			}
			else
			{
				char name[12];
				// normal entry
				for (int j = 0; j < 11; j++)
				{
					name[j] = d[i].name[j];
					if ((name[j] > 127) || (name[j] < 32))
						name[j] = '?';
				}
				name[11] = 0;
				printf("\t%s, attr: 0x%X, cluster: %lu, size: %lub %s\n", name, d[i].attr, (((uint32_t) d[i].ch) << 16) | d[i].cl, d[i].size, (d[i].name[0] == 0xE5)?"[deleted]":"");
			}

			if (d[i].attr == 0x08)
			{
				memcpy(w->label, d[i].name, 11);
				w->label[11] = 0;
				w->fini = 1;

				printf("FAT: mount succeeded! label is %s\n", w->label);

				dequeue(w);

				return;
			}
		}

		w->lba++;

		if (dentry_cache(w->lba) == 0) return;
	}

	_fat_bootblock* bootblock = (_fat_bootblock*) buffer;

	if (bootblock->magic != 0xAA55)
	{
		printf("bad magic at LBA %lu, corrupt disk?\n", lba);
		return;
	}

	printf("FAT: magic ok, trying partition table\n");
	for (int i = 0; i < 4; i++)
	{
		printf("FAT: Partition table %u:\n\ttype: %X\n\tlba_begin: %lu\n\tn_sectors: %lu\n\tend: %lu\n\tdisk blocks: %lu\n",
			i,
			bootblock->partition[i].type,
			bootblock->partition[i].lba_begin,
			bootblock->partition[i].n_sectors,
			bootblock->partition[i].lba_begin + bootblock->partition[i].n_sectors,
			sd->n_sectors()
		);
		if (
			(
				bootblock->partition[i].type == 0x01 ||
				bootblock->partition[i].type == 0x04 ||
				bootblock->partition[i].type == 0x06 ||
				bootblock->partition[i].type == 0x0B ||
				bootblock->partition[i].type == 0x0C ||
				bootblock->partition[i].type == 0x0E ||
				bootblock->partition[i].type == 0x0F
			) &&
			bootblock->partition[i].lba_begin < sd->n_sectors() &&
			bootblock->partition[i].n_sectors + bootblock->partition[i].lba_begin <= sd->n_sectors()
		)
		{
			printf("FAT: Found a partition!\n");

			if (dentry_cache(bootblock->partition[i].lba_begin) == 0) return;
		}
	}

	printf("FAT: didn't look like a partition table, trying FAT superblock\n");

	_fat_volid*   volid     = (_fat_volid  *) buffer;

	uint32_t nsec   = (volid->total_sectors)?volid->total_sectors:volid->total_sectors_32;
	uint32_t nclust = nsec / volid->sectors_per_cluster;

	printf("FAT: superblock:\n\tid: %c%c%c%c%c%c%c%c\n\tbytes_per_sector: %u\n\tn_fats: %u\n\tsectors_per_cluster: %u\n\tn_reserved_sectors: %u\n\tsectors_per_fat: %u\n\t\n\thidden_sectors: %lu\n\ttotal_sectors: %lu (%luMB)\n",
			volid->oem_id[0],volid->oem_id[1],volid->oem_id[2],volid->oem_id[3],volid->oem_id[4],volid->oem_id[5],volid->oem_id[6],volid->oem_id[7],
		volid->bytes_per_sector,
		volid->num_fats,
		volid->sectors_per_cluster,
		volid->num_boot_sectors,
		volid->sectors_per_fat,
		volid->hidden_sectors,
		nsec,
		nsec / 2048
	);

	if (volid->bytes_per_sector == 512 &&
		volid->num_fats == 2             &&
		(	volid->sectors_per_cluster == 1   ||
			volid->sectors_per_cluster == 2   ||
			volid->sectors_per_cluster == 4   ||
			volid->sectors_per_cluster == 8   ||
			volid->sectors_per_cluster == 16  ||
			volid->sectors_per_cluster == 32  ||
			volid->sectors_per_cluster == 64  ||
			volid->sectors_per_cluster == 128
		) &&
		(nsec <= sd->n_sectors())
	)
	{
		fat_type = 12;
		if (nclust >= 4086U)
			fat_type = 16;
		if (nclust >= 65526U)
			fat_type = 32;

		printf("FAT: Found a FAT%d superblock!\n", fat_type);
		// looks like a volid

		fat_begin_lba       = lba + volid->num_boot_sectors;
		sectors_per_cluster = volid->sectors_per_cluster;
// 		bytes_per_sector    = volid->bytes_per_sector;

		if (fat_type == 32)
		{
			uint32_t nsec_per_fat = volid->fat32.sectors_per_fat_32;
			cluster_begin_lba     = lba + volid->num_boot_sectors + (volid->num_fats * nsec_per_fat);
			root_dir_sector       = cluster_to_lba(volid->fat32.root_dir_cluster);
			w->root_dir_end       = cluster_to_lba(volid->fat32.root_dir_cluster + 1) - 1;

			printf("\tSectors per fat: %lu\n", nsec_per_fat);
			printf("\tRoot dir cluster: %lu (LBA:%lu)\n", volid->fat32.root_dir_cluster, root_dir_sector);
			printf("\tCluster begin LBA: %lu\n", lba + volid->num_boot_sectors + (volid->num_fats * nsec_per_fat));
		}
		else
		{
			uint32_t nsec_per_fat = volid->sectors_per_fat;
			root_dir_sector       = lba + volid->num_boot_sectors + (volid->num_fats * nsec_per_fat);
			cluster_begin_lba     = root_dir_sector + (volid->num_root_dir_ents / 16);
			w->root_dir_end       = cluster_begin_lba - 1;
		}

		w->stage = FAT_MOUNT_STAGE_ROOT_DIR;
		w->lba = root_dir_sector;

		if (dentry_cache(w->lba) == 0) return;
	}

	printf("did not recognise disk image: looks like neither FAT volid or partition table.\n");
	dequeue(w);
	return;
}

void Fat::ioaction_open(_fat_file_ioresult* w, uint8_t* buffer, uint32_t lba)
{
	// in f_open, we pre-request the root dir
	// so now we're free to scan each direntry and traverse as necessary

	// scan for file of interest

	char* fn = w->file.path + w->file.pathname_traversed_bytes;
	char matchname[14];
	int i, j;
	uint8_t dir = 0;
	for (i = 0, j = 0; i < 11; i++, j++)
	{
		if (fn[i] == '/')
		{
			dir = i;
			break;
		}
		if (fn[i] == '.')
		{
			for (;i < 8; i++)
				matchname[i] = 32;
			i--;
		}
		else
			matchname[i] = fn[j];
	}
	matchname[i] = 0;

	printf("FAT: Matchname is '%s'\n", matchname);

	dump_buffer(buffer);

	_fat_direntry* d = (_fat_direntry*) buffer;
	_fat_lfnentry* l = (_fat_lfnentry*) buffer;
	int lfn_match = 0;
	for (i = 0; i < 16 && d[i].name[0] != 0; i++)
	{
		if (dir == 0 && (d[i].attr & 0x1F) == 0 && d[i].name[0] != 0xE5)
		{
			// FILE entry
			printf("checking '%s' vs '%s'(%d): %d\n", matchname, d[i].name, d[i].attr, strncasecmp(matchname, (char*) d[i].name, 11));
			if (strncasecmp(matchname, (char*) d[i].name, 11) == 0 || lfn_match)
			{
				// found it!
				printf("Found! First cluster: %u, size: %lub\n", (d[i].ch << 16) | d[i].cl, d[i].size);

				w->action                = IOACTION_READ_ONE;

				w->file.direntry_cluster = lba_to_cluster(lba);
				w->file.direntry_index   = i;

				w->file.root_cluster     = (d[i].ch << 16) | d[i].cl;

				w->file.current_cluster  = w->file.root_cluster;
				w->file.byte_in_cluster  = 0;
				w->file.cluster_index    = 0;

				w->file.size             = d[i].size;

				w->lba                   = cluster_to_lba(w->file.root_cluster);

				dequeue(w);

				break;
			}
		}
		// 				else if (d[i].attr & 0x8)
		// 				{
		// VOLUME LABEL
		// 				}
		else if (dir == 0 && d[i].attr == 0xF)
		{
			// LFN entry
			if (l[i].final || lfn_match)
			{
// 				char lfn[14];
// 				lfn[0]  = l[i].name0[0];
// 				lfn[1]  = l[i].name0[1];
// 				lfn[2]  = l[i].name0[2];
// 				lfn[3]  = l[i].name0[3];
// 				lfn[4]  = l[i].name0[4];
// 				lfn[5]  = l[i].name1[0];
// 				lfn[6]  = l[i].name1[1];
// 				lfn[7]  = l[i].name1[2];
// 				lfn[8]  = l[i].name1[3];
// 				lfn[9]  = l[i].name1[4];
// 				lfn[10] = l[i].name1[5];
// 				lfn[11] = l[i].name2[0];
// 				lfn[12] = l[i].name2[1];
// 				lfn[13] = 0;
//
// 				int off = (l[i].sequence - 1) * 13;
			}
		}
		else if (dir == 1 && d[i].attr & 0x10)
		{
			// FOLDER entry
			if (strncasecmp(matchname, (char*) d[i].name, 11) == 0 || lfn_match)
			{
				w->file.pathname_traversed_bytes += dir + 1;

				w->file.direntry_cluster = (d[i].ch << 16) | d[i].cl;

				w->lba = cluster_to_lba(w->file.direntry_cluster);

				if (dentry_cache(cluster_to_lba(w->file.direntry_cluster)) == 0) return;
			}
		}
	}
}

void Fat::ioaction_read_one(_fat_file_ioresult* w, uint8_t* buffer, uint32_t lba)
{
	if (w->file.byte_in_cluster >= 512 * sectors_per_cluster)
	{
		if (fat_cache(fat_begin_lba + (w->file.current_cluster >> 7)))
		{
			w->file.current_cluster = ((uint32_t*) fat_buf)[w->file.current_cluster & 0x7F];
			w->file.cluster_index++;
			w->file.byte_in_cluster -= (512 * sectors_per_cluster);
		}
		else
			return;
	}

	uint32_t l = cluster_to_lba(w->file.current_cluster) + (w->file.byte_in_cluster >> 9);

	if (l == lba)
	{
		w->file.byte_in_cluster += 512;
		w->bytes_remaining -= 512;
		if (w->bytes_remaining == 0)
			dequeue(w);
			if (w->owner)
				w->owner->_fat_io(w);
	}
}

void Fat::queue_walk()
{
	printf("FAT: Queue walk\n");
	_fat_ioresult* j = work_queue;
	while (j)
	{
		printf("FAT: Queue item %p:\n\taction : %d (%s)\n\tbuffer : %p\n\tbuflen : %lu\n\tcluster: %lu\n\tlba    : %lu\n\towner  : %p\n\tnext   : %p\n", j, j->action, action_name((_fat_ioaction) j->action), j->buffer, j->buflen, lba_to_cluster(j->lba), j->lba, j->owner, j->next);
		j = j->next;
	}
	printf("FAT: end queue\n");
}

int Fat::fat_cache(uint32_t lba)
{
	printf("Fat cache: %s on %lu\n", (fat_lba == lba)?"hit":"miss", lba);

	if (fat_lba == lba)
		return 1;

	sd->begin_read(lba, fat_buf, this);

	return 0;
}

int Fat::dentry_cache(uint32_t lba)
{
	printf("Dentry cache: %s on %lu\n", (dentry_lba == lba)?"hit":"miss", lba);

	if (dentry_lba == lba)
		return 1;

	sd->begin_read(lba, dentry_buf, this);

	return 0;
}
