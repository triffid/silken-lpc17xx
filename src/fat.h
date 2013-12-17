#ifndef _FAT_H
#define _FAT_H

#include "SD.h"

#include "fat_struct.h"

/*
 * Asynchronous FAT Filesystem
 * for DMA driven microcontroller applications
 * Copyright (c) Michael Moon 2013
 *
 * Released under Gnu GPL
 */

/*
 * Operation of Asynchronous FAT:
 *
 * All calls return immediately. We NEVER busyloop, waiting for data
 *
 * Whenever new data is required, we request a DMA transfer and return
 *
 * We maintain a queue of tasks. External tasks (eg open, read, readdir) are
 *     placed at the bottom of the queue.
 *     Internal tasks (eg traverse FAT, read directory, etc) are placed at the
 *     head of the queue.
 *
 * When a new block of data arrives, we examine the first item in the queue.
 * Usually, it can do something useful with the newly arrived data.
 *
 * When a queue item has finished, it is removed from the queue, and the next
 *     item is activated.
 *
 * This, in combination with placing internal tasks on the top of the queue,
 *     means that the following sequence might occur:
 *
 * external -> f_open(&FIL, "/path/to/file.txt")
 *
 * [ add file_open action to TAIL of stack ]
 * cache_test(root_dir_cluster) -> fail
 * [ PUSH directory_traverse action to HEAD of stack ]
 * request dir cluster from disk, return
 *
 * disk->read_complete()
 * [ POP request from head of stack -> directory_traverse action ]
 * cache_test(root_dir_cluster) -> success
 * find "path/" in root_dir_cluster.
 * cache_test(cluster for "path/") -> fail
 * [ PUSH directory_traverse action to HEAD of stack ]
 * request cluster from disk, return
 *
 * disk->read_complete()
 * [ POP request from head of stack -> directory_traverse action ]
 * cache_test(cluster for "path/") -> success
 * find "to/" in cluster for "path/"
 * cache_test(cluster for "to/") -> fail
 * [ PUSH directory_traverse action to HEAD of stack ]
 * request from disk, return
 *
 * disk->read_complete()
 * [ POP request from head of stack -> directory_traverse action ]
 * cache_test(cluster for "to/") -> success
 * find "file.txt" in cluster for "to/" -> fail
 * not in first 16 entries, cache_test(2nd cluster for "to/") ->fail
 * [ PUSH directory_traverse action to HEAD of stack ]
 * request from disk, return
 *
 * disk->read_complete()
 * [ POP request from head of stack -> directory_traverse action ]
 * cache_test(2nd cluster for "to/") -> success
 * find "file.txt" in cluster -> success
 * [ POP request from head of stack -> file_open action ]
 * fill the FIL structure stored in the file_open action with data from the
 *     dir cluster
 *
 * fire callback stored in file_open action
 *
 * stack empty!
 *
 * external -> f_read(&FIL, buffer, size)
 * cache_test(first cluster, from &FIL) -> fail
 * [ PUSH file_read to TAIL of stack ]
 * request from disk, return
 *
 * disk->read_complete()
 * [ POP request from stack -> file_read ]
 * cache_test(first cluster from &FIL) -> success
 *
 * fire read_complete callback
 *
 * stack empty!
 *
 * external -> f_read(&FIL, buffer, size)
 * [ PUSH file_read to TAIL of stack ]
 * find next cluster
 * cache_test(FAT cluster map) -> fail
 * [ PUSH cluster_traverse to HEAD of stack ]
 * request cluster map from disk, return
 *
 * disk->read_complete()
 * [ POP stack -> cluster_traverse ]
 * find relevant cluster
 * cache_test(FAT cluster map) -> success
 * update file_read action
 * [ POP stack -> file_read ]
 * cache_test(2nd cluster) -> fail
 * [ PUSH file_read action back to HEAD of stack (not finished) ]
 * request cluster from disk, return
 *
 * disk->read_complete()
 * [ POP stack -> file_read ]
 * cache_test(file data) -> success!
 *
 * trigger file_read callback
 *
 * stack empty!
 *
 */

class _fat_ioreceiver
{
public:
	virtual void _fat_io(_fat_ioresult*) = 0;
};

class Fat : public SD_async_receiver
{
public:
	Fat();

	/*
	 * should be a fairly familiar set of functions
	 *
	 * however, note that they all return *immediately*, having queued the
	 *     requested action to happen at a later time
	 *
	 * applications should either poll the 'fini' flag (discouraged)
	 * or inherit SD_async_receiver and register as owner (preferred)
	 *
	 */
	void f_mount(_fat_mount_ioresult*, SD*);
	int  f_open( _fat_file_ioresult*, const char*);
	int  f_seek( _fat_file_ioresult*, uint32_t);
	int  f_read( _fat_file_ioresult*, void*, uint32_t);
	int  f_write(_fat_file_ioresult*, void*, uint32_t);
	int  f_close(_fat_file_ioresult*);

	int  f_mounted(void);

	/*
	 * this method receives completion messages from the disk
	 */
// 	void _sd_callback(_sd_work_stack*);
	void sd_read_complete(SD*, uint32_t sector, void* buf, int err);
	void sd_write_complete(SD*, uint32_t sector, void* buf, int err);

	/*
	 * this is where we crunch received (or cached) data
	 */
	void process_buffer(uint8_t* buffer, uint32_t lba);

	void ioaction_mount(    _fat_mount_ioresult* w, uint8_t* buffer, uint32_t lba);
	void ioaction_open(     _fat_file_ioresult*  w, uint8_t* buffer, uint32_t lba);
	void ioaction_read_one( _fat_file_ioresult*  w, uint8_t* buffer, uint32_t lba);
	void ioaction_write_one(_fat_file_ioresult*  w, uint8_t* buffer, uint32_t lba);

	/*
	 * debug function, prints queue contents
	 */
	void queue_walk(void);
protected:
	/*
	 * these four variables uniquely define a FAT filesystem at some position on a disk
	 */
	uint32_t fat_begin_lba;
	uint32_t cluster_begin_lba;
	uint32_t sectors_per_cluster;
	uint32_t root_dir_cluster;

	uint32_t bytes_per_sector; // I don't feel comfortable assuming that this is always 512

	/*
	 * conversion between cluster and lba
	 */
	uint32_t cluster_to_lba(uint32_t cluster);
	uint32_t lba_to_cluster(uint32_t lba);

	/*
	 * queueing functions
	 */
	void     enqueue(_fat_ioresult*);
	void     dequeue(_fat_ioresult*);
	void     byte2cluster(_fat_ioresult*, uint32_t);

	int      fat_cache(   uint32_t lba);
	int      dentry_cache(uint32_t lba);

private:
	SD* sd;
	/*
	 * these buffers are used internally for traversing cluster chains and directory entries
	 */
	uint8_t* fat_buf;
	uint32_t fat_lba;
	uint8_t* dentry_buf;
	uint32_t dentry_lba;

	/*
	 * this is the head of the queue, which is a linked list
	 */
	_fat_ioresult* work_queue;
};

#endif /* _FAT_H */
