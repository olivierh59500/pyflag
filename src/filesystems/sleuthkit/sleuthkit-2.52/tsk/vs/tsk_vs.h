/*
 * The Sleuth Kit
 *
 * $Date: 2007/12/20 20:32:39 $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2003-2007 Brian Carrier.  All rights reserved
 */

/**
 * \file tsk_vs.h
 * External header file for media management (volume system) support.
 * Note that this file is not meant to be directly included.  
 * It is included by both libtsk.h and tsk_vs_i.h.
 */
#ifndef _TSK_VS_H
#define _TSK_VS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Structures */
    typedef struct TSK_MM_INFO TSK_MM_INFO;
    typedef struct TSK_MM_PART TSK_MM_PART;

/* walk action functions */
    typedef TSK_WALK_RET_ENUM(*TSK_MM_PART_WALK_CB) (TSK_MM_INFO *, TSK_PNUM_T,
        TSK_MM_PART *, int, void *);



    /**
     * Flags for the partition type
     */
    enum TSK_MM_INFO_TYPE_ENUM {
        TSK_MM_INFO_TYPE_UNSUPP = 0,    ///< Unsupported
        TSK_MM_INFO_TYPE_DOS = 1,       ///< DOS Partition table
        TSK_MM_INFO_TYPE_BSD = 2,       ///< BSD Partition table
        TSK_MM_INFO_TYPE_SUN = 3,       ///< Sun VTOC
        TSK_MM_INFO_TYPE_MAC = 4,       ///< Mac partition table
        TSK_MM_INFO_TYPE_GPT = 5,       ///< GPT partition table
    };
    typedef enum TSK_MM_INFO_TYPE_ENUM TSK_MM_INFO_TYPE_ENUM;


    /**
     * Data structure used to store state and basic information
     * for open volume systems.
     */
    struct TSK_MM_INFO {
        TSK_IMG_INFO *img_info;     ///< Pointer to disk image that VS is in
        TSK_MM_INFO_TYPE_ENUM mmtype;   ///< Type of volume system / media management
        TSK_DADDR_T offset;         ///< Byte offset where VS starts in disk image
        const char *str_type;       ///< String  name of VS types
        unsigned int block_size;    ///< Size of blocks in bytes
        unsigned int dev_bsize;     ///< Size of device blocks in bytes (typically 512)

        TSK_ENDIAN_ENUM endian; ///< Endian ordering of data

        TSK_MM_PART *part_list;  ///< Linked list of partitions

        TSK_PNUM_T first_part;      ///< number of first partition 
        TSK_PNUM_T last_part;       ///< number of last partition 

        /* media management type specific function pointers */
         uint8_t(*part_walk) (TSK_MM_INFO *, TSK_PNUM_T, TSK_PNUM_T, int,
            TSK_MM_PART_WALK_CB, void *);
        void (*close) (TSK_MM_INFO *);
    };




/***************************************************************
 * Generic structures  for partitions / slices
 */

    /** 
     * Flag values that describe the partitions in the VS
     */
    enum TSK_MM_PART_TYPE_ENUM {
        TSK_MM_PART_TYPE_DESC = (1 << 0),       ///< Entry is for sectors of metadata
        TSK_MM_PART_TYPE_VOL = (1 << 1) ///< Entry is for sectors in a volume
    };
    typedef enum TSK_MM_PART_TYPE_ENUM TSK_MM_PART_TYPE_ENUM;

    /**
     * Linked list entry that describes a volume in a generic way. 
     */
    struct TSK_MM_PART {
        TSK_MM_PART *prev;      ///< POinter to previous partition (or NULL)
        TSK_MM_PART *next;      ///< Pointer to next partition (or NULL)

        TSK_DADDR_T start;          ///< Sector offset of start of partition
        TSK_DADDR_T len;            ///< Number of sectors in partition
        char *desc;             ///< UTF-8 description of partition
        int8_t table_num;       ///< Table address that describes this partition
        int8_t slot_num;        ///< Entry in the table that describes this partition
        TSK_MM_PART_TYPE_ENUM type;     ///< Type of partition
    };



    extern uint8_t tsk_mm_part_unused(TSK_MM_INFO *);
    extern void tsk_mm_part_print(TSK_MM_INFO *);
    extern TSK_MM_PART *tsk_mm_part_add(TSK_MM_INFO *, TSK_DADDR_T, TSK_DADDR_T,
        TSK_MM_PART_TYPE_ENUM, char *, int8_t, int8_t);
    extern void tsk_mm_part_free(TSK_MM_INFO *);

    /***** TYPES *****/
    extern TSK_MM_INFO_TYPE_ENUM tsk_mm_parse_type(const TSK_TCHAR *);
    extern const char *tsk_mm_get_type(TSK_MM_INFO_TYPE_ENUM);


/**************************************************************8
 * Generic routines.
 */
    extern TSK_MM_INFO *tsk_mm_open(TSK_IMG_INFO *, TSK_DADDR_T,
        const TSK_TCHAR *);
    extern ssize_t tsk_mm_read_block_nobuf(TSK_MM_INFO *, char *, size_t,
        TSK_DADDR_T);
    extern void tsk_mm_print_types(FILE *);

    extern TSK_MM_INFO *tsk_mm_dos_open(TSK_IMG_INFO *, TSK_DADDR_T, uint8_t);
    extern TSK_MM_INFO *tsk_mm_mac_open(TSK_IMG_INFO *, TSK_DADDR_T);
    extern TSK_MM_INFO *tsk_mm_bsd_open(TSK_IMG_INFO *, TSK_DADDR_T);
    extern TSK_MM_INFO *tsk_mm_sun_open(TSK_IMG_INFO *, TSK_DADDR_T);
    extern TSK_MM_INFO *tsk_mm_gpt_open(TSK_IMG_INFO *, TSK_DADDR_T);


// Endian macros - actual functions in misc/
#define tsk_mm_guessu16(mm, x, mag)   \
	tsk_guess_end_u16(&(mm->endian), (x), (mag))

#define tsk_mm_guessu32(mm, x, mag)   \
	tsk_guess_end_u32(&(mm->endian), (x), (mag))


#ifdef __cplusplus
}
#endif
#endif
