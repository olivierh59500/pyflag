/*
 * The Sleuth Kit
 *
 * $Date: 2007/04/05 16:01:57 $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
 * Copyright (c) 2003-2005 Brian Carrier.  All rights reserved
 * 
 * Copyright (c) 1997,1998,1999, International Business Machines          
 * Corporation and others. All Rights Reserved.
 *
 *
 * LICENSE
 *	This software is distributed under the IBM Public License.
 * AUTHOR(S)
 *	Wietse Venema
 *	IBM T.J. Watson Research
 *	P.O. Box 704
 *	Yorktown Heights, NY 10598, USA
--*/

/** \file fs_io.c
 * Contains functions to read data from a disk image and wrapper functions to read file content.
 */



#include <errno.h>
#include "fs_tools_i.h"
#include "ntfs.h"


/**
 * Read a file system block into a TSK_DATA_BUF structure.  
 * This is actually a wrapper around the fs_read_random function,
 * but it allows the starting location to be specified as a block address. 
 * @param fs The file system structure.
 * @param buf The buffer to store the block in.
 * @param len The number of bytes to read (must be a multiple of the device block size)
 * @param addr The starting block file system address. 
 * @return The number of bytes read or -1 on error. 
 */
SSIZE_T
tsk_fs_read_block(TSK_FS_INFO * fs, TSK_DATA_BUF * buf, OFF_T len,
    DADDR_T addr)
{
    OFF_T offs;
    SSIZE_T cnt;

    if (len % fs->dev_bsize) {
        tsk_error_reset();
        tsk_errno = TSK_ERR_FS_READ;
        snprintf(tsk_errstr, TSK_ERRSTR_L,
            "tsk_fs_read_block: length %" PRIuOFF " not a multiple of %d",
            len, fs->dev_bsize);
        return -1;
    }


    if (len > buf->size) {
        tsk_error_reset();
        tsk_errno = TSK_ERR_FS_READ;
        snprintf(tsk_errstr, TSK_ERRSTR_L,
            "tsk_fs_read_block: Buffer too small - %"
            PRIuOFF " > %Zd", len, buf->size);
        return -1;
    }

    if (addr > fs->last_block) {
        tsk_error_reset();
        tsk_errno = TSK_ERR_FS_READ;
        snprintf(tsk_errstr, TSK_ERRSTR_L,
            "tsk_fs_read_block: Address is too large: %" PRIuDADDR ")",
            addr);
        return -1;
    }

    buf->addr = addr;
    offs = (OFF_T) addr *fs->block_size;

    cnt =
        fs->img_info->read_random(fs->img_info, fs->offset, buf->data, len,
        offs);
    buf->used = cnt;
    return cnt;
}


/**
 * Read a file system block into a char* buffer.  
 * This is actually a wrapper around the fs_read_random function,
 * but it allows the starting location to be specified as a block address. 
 *
 * @param fs The file system structure.
 * @param buf The char * buffer to store the block in.
 * @param len The number of bytes to read (must be a multiple of the device block size)
 * @param addr The starting block file system address. 
 * @return The number of bytes read or -1 on error. 
 */
SSIZE_T
tsk_fs_read_block_nobuf(TSK_FS_INFO * fs, char *buf, OFF_T len,
    DADDR_T addr)
{
    if (len % fs->dev_bsize) {
        tsk_error_reset();
        tsk_errno = TSK_ERR_FS_READ;
        snprintf(tsk_errstr, TSK_ERRSTR_L,
            "tsk_fs_read_block_nobuf: length %" PRIuOFF
            " not a multiple of %d", len, fs->dev_bsize);
        return -1;
    }

    if (addr > fs->last_block) {
        tsk_error_reset();
        tsk_errno = TSK_ERR_FS_READ;
        snprintf(tsk_errstr, TSK_ERRSTR_L,
            "tsk_fs_read_block: Address is too large: %" PRIuDADDR ")",
            addr);
        return -1;
    }

    return fs->img_info->read_random(fs->img_info, fs->offset, buf, len,
        (OFF_T) addr * fs->block_size);
}


/**
 * \internal
 * Copy a block from a file into the data structure passed. 
 */
static uint8_t
fs_load_file_act(TSK_FS_INFO * fs, DADDR_T addr, char *buf, size_t size,
    TSK_FS_BLOCK_FLAG_ENUM flags, void *ptr)
{
    TSK_FS_LOAD_FILE *buf1 = (TSK_FS_LOAD_FILE *) ptr;
    size_t cp_size;

    if (size > buf1->left)
        cp_size = buf1->left;
    else
        cp_size = size;

    memcpy(buf1->cur, buf, cp_size);
    buf1->left -= cp_size;
    buf1->cur = (char *) ((uintptr_t) buf1->cur + cp_size);

    if (buf1->left > 0)
        return TSK_WALK_CONT;
    else
        return TSK_WALK_STOP;
}


/**
 * Load the contents of a file into a buffer. 
 * 
 * @param fs The file system structure.
 * @param fsi The inode structure of the file to read.
 * @param type The type of attribute to load (ignored if TSK_FS_FILE_FLAG_NOID is given)
 * @param id The id of attribute to load (ignored if TSK_FS_FILE_FLAG_NOID is given)
 * @param flags Flag values of type TSK_FS_FILE_FLAG_*
 * @return The buffer with the file content (must be freed by caller)
 */
char *
tsk_fs_load_file(TSK_FS_INFO * fs, TSK_FS_INODE * fsi, uint32_t type,
    uint16_t id, int flags)
{
    TSK_FS_LOAD_FILE lf;

    if (NULL == (lf.base = (char *) tsk_malloc((size_t) fsi->size))) {
        return NULL;
    }
    lf.left = lf.total = (size_t) fsi->size;
    lf.cur = lf.base;

    if (fs->file_walk(fs, fsi, type, id, flags, fs_load_file_act,
            (void *) &lf)) {
        free(lf.base);
        strncat(tsk_errstr2, " - tsk_fs_load_file",
            TSK_ERRSTR_L - strlen(tsk_errstr2));
        return NULL;
    }

    /* Not all of the file was copied */
    if (lf.left > 0) {
        tsk_error_reset();
        tsk_errno = TSK_ERR_FS_FWALK;
        snprintf(tsk_errstr, TSK_ERRSTR_L,
            "tsk_fs_load_file: Error reading file %" PRIuINUM, fsi->addr);
        free(lf.base);
        return NULL;
    }

    return lf.base;
}


// This size is based on the speed of the hard drive
// and the expected amount of fragmentation. 
#define FS_READ_FILE_CACHE_SZ	8 * 1024

/**
 * \internal
 * Used to hold the state as we seek through the file to where we 
 * need to read data from by fs_read_file().
 */
typedef struct {
    char *base;                 ///< Base pointer to the buffer
    char *cur;                  ///< Pointer to next location to write to in buffer
    size_t size_to_copy;        ///< Total amount of data to copy 
    size_t size_left;           ///< Amount of data left to copy
    size_t offset_left;         ///< Amount left to seek until we start to copy
    char cache[FS_READ_FILE_CACHE_SZ];  ///< Cache buffer
    DADDR_T cache_base;         ///< Block address of data in cache
    uint8_t cache_inuse;        ///< Set to 1 when cache has been loaded
} FS_READ_FILE;


/**
 * \internal
 * Copies relevant data from a file to the buffer.  This is used when 
 * the callback is given the block contents.  Use fs_read_file_act_aonly
 * if the AONLY flag was given. 
 */
static uint8_t
fs_read_file_act_data(TSK_FS_INFO * fs, DADDR_T addr, char *buf,
    size_t size, TSK_FS_BLOCK_FLAG_ENUM flags, void *ptr)
{
    FS_READ_FILE *buf1 = (FS_READ_FILE *) ptr;
    size_t cp_size;
    size_t blk_offset;

    /* Is this block too early in the stream? */
    if (buf1->offset_left > size) {
        buf1->offset_left -= size;
        return TSK_WALK_CONT;
    }

    blk_offset = buf1->offset_left;
    buf1->offset_left = 0;

    /* How much of the block are we going to copy? */
    if ((size - blk_offset) > buf1->size_left)
        cp_size = buf1->size_left;
    else
        cp_size = size - blk_offset;

    memcpy(buf1->cur, &buf[blk_offset], cp_size);
    buf1->cur = (char *) ((uintptr_t) buf1->cur + cp_size);

    buf1->size_left -= cp_size;
    if (buf1->size_left > 0)
        return TSK_WALK_CONT;
    else
        return TSK_WALK_STOP;
}


/**
 * \internal
 * Copies relevant data from a file to the buffer.  This is used when 
 * the callback is given only the address.  Use fs_read_file_act_data
 * if the AONLY flag was NOT given. 
 */
static uint8_t
fs_read_file_act_aonly(TSK_FS_INFO * fs, DADDR_T addr, char *buf,
    size_t size, TSK_FS_BLOCK_FLAG_ENUM flags, void *ptr)
{
    FS_READ_FILE *buf1 = (FS_READ_FILE *) ptr;
    size_t cp_size;
    size_t blk_offset;

    /* Is this block too early in the stream? */
    if (buf1->offset_left > size) {
        buf1->offset_left -= size;
        return TSK_WALK_CONT;
    }

    blk_offset = buf1->offset_left;
    buf1->offset_left = 0;

    /* How much of the block are we going to copy? */
    if ((size - blk_offset) > buf1->size_left)
        cp_size = buf1->size_left;
    else
        cp_size = size - blk_offset;

    /* If the block is sparse, then simply write zeros */
    if (flags & TSK_FS_BLOCK_FLAG_SPARSE) {
        memset(buf1->cur, 0, cp_size);
    }
    else {
        /* First check if it is in the cache */
        if ((buf1->cache_inuse) &&
            (addr >= buf1->cache_base) &&
            ((addr - buf1->cache_base) * fs->block_size <
                FS_READ_FILE_CACHE_SZ)) {

            size_t cache_offset =
                blk_offset + (size_t) ((addr -
                    buf1->cache_base) * fs->block_size);

            /* Check if the data we want starts in the cache, but is not fully in
             * it.  From the check that starts the cache for the first time, we 
             * know that cp_size will be less than the cache size (if we assume
             * that all sizes in the call back are the same -- which is true 
             * except for compressed NTFS -- which do not use this callback)
             */
            if (cache_offset + cp_size > FS_READ_FILE_CACHE_SZ) {
                tsk_fs_read_random(fs, buf1->cache, FS_READ_FILE_CACHE_SZ,
                    (addr * fs->block_size));
                buf1->cache_base = addr;
                cache_offset = blk_offset;
            }
            memcpy(buf1->cur, &buf1->cache[cache_offset], cp_size);
        }
        /* This case can start the cache and will be used when the data in the
         * cache is not what we want 
         * Make sure that we only use the cache we need more than 1 block and
         * if the size of each callback is less than the cache size. */
        else if ((buf1->size_left > fs->block_size) &&
            (size < FS_READ_FILE_CACHE_SZ)) {
            tsk_fs_read_random(fs, buf1->cache, FS_READ_FILE_CACHE_SZ,
                (addr * fs->block_size));

            buf1->cache_inuse = 1;
            buf1->cache_base = addr;
            memcpy(buf1->cur, &buf1->cache[blk_offset], cp_size);
        }
        /* Fallback case where we simply read into the buffer and ignore
         * the cache */
        else {
            tsk_fs_read_random(fs, buf1->cur, cp_size,
                (addr * fs->block_size) + blk_offset);
        }
    }

    buf1->cur = (char *) ((uintptr_t) buf1->cur + cp_size);

    buf1->size_left -= cp_size;
    if (buf1->size_left > 0)
        return TSK_WALK_CONT;
    else
        return TSK_WALK_STOP;
}


/**
 * \internal
 * Internal method for reading files using a standard read type interface.
 * This is called by the two wrapper functions (the difference the two is
 * based on if a type and id are used (only NTFS uses them)
 *
 * @param fs The file system structure.
 * @param fsi The inode structure of the file to read.
 * @param type The type of attribute to load (ignored if TSK_FS_FILE_FLAG_NOID is given)
 * @param id The id of attribute to load (ignored if TSK_FS_FILE_FLAG_NOID is given)
 * @param offset The byte offset to start reading from.
 * @param size The number of bytes to read from the file.
 * @param buf The buffer to read the data into.
 * @param flagsBase The base set of flags to set.
 * @returns The number of bytes read or -1 on error.
 */
static SSIZE_T
fs_read_file_int(TSK_FS_INFO * fs, TSK_FS_INODE * fsi, uint32_t type,
    uint16_t id, SSIZE_T offset, SSIZE_T size, char *buf, int flagsBase)
{
    FS_READ_FILE lf;
    int flags = flagsBase;

    lf.base = lf.cur = buf;
    lf.size_to_copy = lf.size_left = size;
    lf.offset_left = offset;
    lf.cache_inuse = 0;

    if (fsi->flags & TSK_FS_INODE_FLAG_UNALLOC) {
        flags |= TSK_FS_FILE_FLAG_RECOVER;
    }

    /* For compressed files, we must do a normal walk.  For non-compressed
     * files, we can simply do an AONLY walk and then read only the blocks 
     * that we need
     */
    if (fsi->flags & TSK_FS_INODE_FLAG_COMP) {
        if (fs->file_walk(fs, fsi, type, id, flags, fs_read_file_act_data,
                (void *) &lf)) {
            strncat(tsk_errstr2, " - tsk_fs_read_file",
                TSK_ERRSTR_L - strlen(tsk_errstr2));
            return -1;
        }
    }
    else {
        /* We need to check if the file in question is resident,
         * they require special treatment.  Look up the attribute.
         */
        if ((fs->ftype & TSK_FS_INFO_TYPE_FS_MASK) ==
            TSK_FS_INFO_TYPE_NTFS_TYPE) {
            TSK_FS_DATA *fs_data;

            // @@@ This is bad since it duplicates much of ntfs_file_walk...
            if (fsi->attr == NULL) {
                tsk_error_reset();
                tsk_errno = TSK_ERR_FS_ARG;
                snprintf(tsk_errstr, TSK_ERRSTR_L,
                    "fs_read_file: attributes are NULL");
                return -1;
            }

            if(!type) {
            if ((fsi->mode & TSK_FS_INODE_MODE_FMT) ==
                TSK_FS_INODE_MODE_DIR)
                type = NTFS_ATYPE_IDXROOT;
            else
                type = NTFS_ATYPE_DATA;
            }

            if (flags & TSK_FS_FILE_FLAG_NOID)
                fs_data = tsk_fs_data_lookup_noid(fsi->attr, type);
            else
                fs_data = tsk_fs_data_lookup(fsi->attr, type, id);

            if (fs_data == NULL) {
                tsk_error_reset();
                tsk_errno = TSK_ERR_FS_ARG;
                snprintf(tsk_errstr, TSK_ERRSTR_L,
                    "fs_read_file: Data not found in file");
                return -1;
            }

            /* The attribute is resident, so use the data callback, otherwise use
             * the normal aonly callback and flags 
             */
            if (fs_data->flags & TSK_FS_DATA_RES) {
                if (fs->file_walk(fs, fsi, type, id, flags,
                        fs_read_file_act_data, (void *) &lf)) {
                    strncat(tsk_errstr2, " - tsk_fs_read_file",
                        TSK_ERRSTR_L - strlen(tsk_errstr2));
                    return -1;
                }
                return lf.size_to_copy - lf.size_left;
            }
        }

        flags |= TSK_FS_FILE_FLAG_AONLY;
        if (fs->file_walk(fs, fsi, type, id, flags, fs_read_file_act_aonly,
                (void *) &lf)) {
            strncat(tsk_errstr2, " - tsk_fs_read_file",
                TSK_ERRSTR_L - strlen(tsk_errstr2));
            return -1;
        }
    }

    return lf.size_to_copy - lf.size_left;
}

/**
 * Read the contents of a file using a typical read() type interface.
 * 
 * @param fs The file system structure.
 * @param fsi The inode structure of the file to read.
 * @param offset The byte offset to start reading from.
 * @param size The number of bytes to read from the file.
 * @param buf The buffer to read the data into.
 * @returns The number of bytes read or -1 on error.
 */
SSIZE_T
tsk_fs_read_file_noid(TSK_FS_INFO * fs, TSK_FS_INODE * fsi,
    SSIZE_T offset, SSIZE_T size, char *buf)
{
    return fs_read_file_int(fs, fsi, 0, 0, offset, size, buf,
        TSK_FS_FILE_FLAG_NOID);
}

/**
 * Read the contents of a specific attribute of a file using a typical read() type interface.
 * 
 * @param fs The file system structure.
 * @param fsi The inode structure of the file to read.
 * @param type The type of attribute to load 
 * @param id The id of attribute to load
 * @param offset The byte offset to start reading from.
 * @param size The number of bytes to read from the file.
 * @param buf The buffer to read the data into.
 * @returns The number of bytes read or -1 on error.
 */
SSIZE_T
tsk_fs_read_file(TSK_FS_INFO * fs, TSK_FS_INODE * fsi, uint32_t type,
    uint16_t id, SSIZE_T offset, SSIZE_T size, char *buf)
{
    return fs_read_file_int(fs, fsi, type, id, offset, size, buf, 0);
}

/** versions of the above which include slack space
*/


SSIZE_T
tsk_fs_read_file_noid_slack(TSK_FS_INFO * fs, TSK_FS_INODE * fsi,
    SSIZE_T offset, SSIZE_T size, char *buf)
{
    return fs_read_file_int(fs, fsi, 0, 0, offset, size, buf,
        TSK_FS_FILE_FLAG_NOID|TSK_FS_FILE_FLAG_SLACK);
}

SSIZE_T
tsk_fs_read_file_slack(TSK_FS_INFO * fs, TSK_FS_INODE * fsi, uint32_t type,
    uint16_t id, SSIZE_T offset, SSIZE_T size, char *buf)
{
    return fs_read_file_int(fs, fsi, type, id, offset, size, buf, TSK_FS_FILE_FLAG_SLACK);
}
