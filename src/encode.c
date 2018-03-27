/*****************************************************************************/
/*  LibreDWG - free implementation of the DWG file format                    */
/*                                                                           */
/*  Copyright (C) 2009, 2010 Free Software Foundation, Inc.                  */
/*  Copyright (C) 2010 Thien-Thi Nguyen                                      */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 3 of the License, or (at your option) any later version.  */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*****************************************************************************/

/*
 * encode.c: encoding functions
 * written by Felipe Castro
 * modified by Felipe Corrêa da Silva Sances
 * modified by Rodrigo Rodrigues da Silva
 * modified by Thien-Thi Nguyen
 * modified by Till Heuschmann
 * modified by Anderson Pierre Cardoso
 * modified by Reini Urban
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "bits.h"
#include "dwg.h"
#include "encode.h"

/* The logging level for the write (encode) path.  */
static unsigned int loglevel;
/* the current version per spec block */
static unsigned int cur_ver = 0;

#ifdef USE_TRACING
/* This flag means we have checked the environment variable
   LIBREDWG_TRACE and set `loglevel' appropriately.  */
static bool env_var_checked_p;

#define DWG_LOGLEVEL loglevel
#endif  /* USE_TRACING */

#include "logging.h"

/*--------------------------------------------------------------------------------
 * Welcome to the dark side of the moon...
 * MACROS
 */

#define IS_ENCODER

#define ANYCODE -1
#define REFS_PER_REALLOC 100

#define FIELD(name,type)\
  bit_write_##type(dat, _obj->name);\
  FIELD_TRACE(name,type)
#define FIELD_TRACE(name,type)\
  if (loglevel>=2)\
    {\
        LOG_TRACE(#name ": " FORMAT_##type "\n", _obj->name)\
    }
#define FIELD_CAST(name,type,cast)\
  bit_write_##type(dat, (BITCODE_##type)_obj->name);\
  FIELD_TRACE(name,cast)

#define FIELD_VALUE(name) _obj->name

#define FIELD_B(name) FIELD(name, B);
#define FIELD_BB(name) FIELD(name, BB);
#define FIELD_3B(name) FIELD(name, 3B);
#define FIELD_BS(name) FIELD(name, BS);
#define FIELD_BL(name) FIELD(name, BL);
#define FIELD_BLL(name) FIELD(name, BLL);
#define FIELD_BD(name) FIELD(name, BD);
#define FIELD_RC(name) FIELD(name, RC);
#define FIELD_RS(name) FIELD(name, RS);
#define FIELD_RD(name) FIELD(name, RD);
#define FIELD_RL(name) FIELD(name, RL);
#define FIELD_RLL(name) FIELD(name, RLL);
#define FIELD_MC(name) FIELD(name, MC);
#define FIELD_MS(name) FIELD(name, MS);
#define FIELD_TV(name) \
  IF_ENCODE_FROM_EARLIER { _obj->name = strdup(""); } FIELD(name, TV);
#define FIELD_T FIELD_TV /*TODO: implement version dependant string fields */
#define FIELD_BT(name) FIELD(name, BT);

#define FIELD_DD(name, _default) bit_write_DD(dat, FIELD_VALUE(name), _default);
#define FIELD_2DD(name, d1, d2) FIELD_DD(name.x, d1); FIELD_DD(name.y, d2);

#define FIELD_2RD(name) FIELD(name.x, RD); FIELD(name.y, RD);
#define FIELD_2BD(name) FIELD(name.x, BD); FIELD(name.y, BD);
#define FIELD_3RD(name) FIELD(name.x, RD); FIELD(name.y, RD); FIELD(name.z, RD);
#define FIELD_3BD(name) FIELD(name.x, BD); FIELD(name.y, BD); FIELD(name.z, BD);
#define FIELD_3DPOINT(name) FIELD_3BD(name)
#define FIELD_4BITS(name) bit_write_4BITS(dat,_obj->name);

#define FIELD_CMC(name)\
  {\
    bit_write_CMC(dat, &_obj->name);\
  }

#define FIELD_BE(name)\
  bit_write_BE(dat, FIELD_VALUE(name.x), FIELD_VALUE(name.y), FIELD_VALUE(name.z));

#define FIELD_2RD_VECTOR(name, size)\
  for (vcount=0; vcount < (long)_obj->size; vcount++)\
    {\
      FIELD_2RD(name[vcount]);\
    }

#define FIELD_2DD_VECTOR(name, size)\
  FIELD_2RD(name[0]);\
  for (vcount = 1; vcount < (long)_obj->size; vcount++)\
    {\
      FIELD_2DD(name[vcount], FIELD_VALUE(name[vcount - 1].x), FIELD_VALUE(name[vcount - 1].y));\
    }

#define FIELD_3DPOINT_VECTOR(name, size)\
  for (vcount=0; vcount < (long)_obj->size; vcount++)   \
    {\
      FIELD_3DPOINT(name[vcount]);\
    }

#define REACTORS(code)\
  for (vcount=0; vcount < (long)obj->tio.object->num_reactors; vcount++) \
    {\
      FIELD_HANDLE_N(reactors[vcount], vcount, code);    \
    }
    
#define XDICOBJHANDLE(code)\
  SINCE(R_2004)\
    {\
      if (!obj->tio.object->xdic_missing_flag) \
        {\
          FIELD_HANDLE(xdicobjhandle, code);\
        }\
    }\
  PRIOR_VERSIONS\
    {\
      FIELD_HANDLE(xdicobjhandle, code);\
    }

//XXX need a review
#define ENT_XDICOBJHANDLE(code)\
  SINCE(R_2004)\
    {\
      if (!obj->tio.entity->xdic_missing_flag)\
        {\
          FIELD_HANDLE(xdicobjhandle, code);\
        }\
    }\
  PRIOR_VERSIONS\
    {\
      FIELD_HANDLE(xdicobjhandle, code);\
    }


//FIELD_VECTOR_N(name, type, size):
// writes a 'size' elements vector of data of the type indicated by 'type'
#define FIELD_VECTOR_N(name, type, size)\
  if (size>0)\
    {\
      for (vcount=0; vcount < (long)size; vcount++)\
        {\
          bit_write_##type(dat, _obj->name[vcount]);\
          if (loglevel>=2)\
            {\
              LOG_TRACE(#name "[%ld]: " FORMAT_##type "\n", (long)vcount, _obj->name[vcount]) \
            }\
        }\
    }

#define FIELD_VECTOR(name, type, size) \
  FIELD_VECTOR_N(name, type, _obj->size)

#define FIELD_HANDLE(name, handle_code) \
  IF_ENCODE_FROM_EARLIER { ; } else {   \
  assert(_obj->name); \
  if (handle_code != ANYCODE && _obj->name->handleref.code != handle_code) \
    { \
      LOG_WARN("Expected a CODE %d handle, got a %d", \
                handle_code, _obj->name->handleref.code); \
    } \
  bit_write_H(dat, &_obj->name->handleref); \
  }

#define FIELD_HANDLE_N(name, vcount, handle_code)\
  FIELD_HANDLE(name, handle_code)

#define HANDLE_VECTOR_N(name, size, code)\
  if (size>0) \
    assert(_obj->name); \
  for (vcount=0; vcount < (long)size; vcount++)\
    {\
      assert(_obj->name[vcount]); \
      FIELD_HANDLE_N(name[vcount], vcount, code);   \
    }

#define FIELD_INSERT_COUNT(insert_count, type)   \
      FIELD_RL(insert_count)

#define HANDLE_VECTOR(name, sizefield, code) \
  HANDLE_VECTOR_N(name, FIELD_VALUE(sizefield), code)

#define FIELD_XDATA(name, size)

#define COMMON_ENTITY_HANDLE_DATA  \
 dwg_encode_common_entity_handle_data(dat, obj);

//TODO unify REPEAT macros
#define REPEAT_N(times, name, type) \
  for (rcount=0; (long)rcount<(long)times; rcount++)

#define REPEAT(times, name, type) \
  for (rcount=0; (long)rcount<(long)_obj->times; rcount++)

#define REPEAT2(times, name, type) \
  for (rcount2=0; (long)rcount2<(long)_obj->times; rcount2++)

#define REPEAT3(times, name, type) \
  for (rcount3=0; (long)rcount3<(long)_obj->times; rcount3++)

#define DWG_ENTITY(token) \
  static void dwg_encode_##token (Bit_Chain * dat, Dwg_Object* obj)	\
{ \
  long vcount, rcount, rcount2, rcount3; \
  Dwg_Data* dwg = obj->parent; \
  Dwg_Entity_##token * _obj = obj->tio.entity->tio.token; \
  if (dwg_encode_entity (obj, dat)) return;       \
  LOG_INFO("Entity " #token ":\n") \

#define DWG_ENTITY_END }

#define DWG_OBJECT(token) \
  static void dwg_encode_##token (Bit_Chain * dat, Dwg_Object* obj) \
{ \
  long vcount, rcount, rcount2, rcount3; \
  Dwg_Data* dwg = obj->parent; \
  Dwg_Object_##token * _obj = obj->tio.object->tio.token; \
  if (dwg_encode_object (obj, dat)) return;  \
  LOG_INFO("Object " #token " handle: %d.%d.%lu\n",\
    obj->handle.code, \
    obj->handle.size, \
    obj->handle.value)

#define DWG_OBJECT_END }

#define ENT_REACTORS(code)\
  FIELD_VALUE(reactors) = (BITCODE_H*) malloc(sizeof(BITCODE_H) * obj->tio.entity->num_reactors);\
  if (!FIELD_VALUE(reactors)) { LOG_ERROR("Out of memory"); } \
  for (vcount=0; vcount<obj->tio.entity->num_reactors; vcount++)\
    {\
      FIELD_HANDLE_N(reactors[vcount], vcount, code);      \
    }


/*--------------------------------------------------------------------------------*/
typedef struct
{
  long int handle;
  long int address;
  unsigned int idc;
} Object_Map;

/*--------------------------------------------------------------------------------
 * Private functions prototypes
 */

static int
dwg_encode_entity(Dwg_Object * obj, Bit_Chain * dat);
static int
dwg_encode_object(Dwg_Object * obj, Bit_Chain * dat);
static void
dwg_encode_common_entity_handle_data(Bit_Chain * dat, Dwg_Object * obj);
static void
dwg_encode_header_variables(Bit_Chain* dat, Dwg_Data * dwg);
static int
dwg_encode_variable_type(Dwg_Data * dwg, Bit_Chain * dat, Dwg_Object* obj);
void
dwg_encode_handleref(Bit_Chain * dat, Dwg_Object * obj, Dwg_Data* dwg, Dwg_Object_Ref* ref);
void 
dwg_encode_handleref_with_code(Bit_Chain * dat, Dwg_Object * obj, Dwg_Data* dwg,
                               Dwg_Object_Ref* ref, unsigned int code);
void
dwg_encode_add_object(Dwg_Object * obj, Bit_Chain * dat, unsigned long address);

/*--------------------------------------------------------------------------------
 * Public variables
 */

/*--------------------------------------------------------------------------------
 * Public functions
 */
int
dwg_encode_chains(Dwg_Data * dwg, Bit_Chain * dat)
{
  int ckr_missing = 1;
  int i;
  long unsigned int j;
  long unsigned int section_address;
  unsigned char pvzbit;
  long unsigned int pvzadr;
  long unsigned int pvzadr_2;
  unsigned int ckr;
  unsigned int sekcisize = 0;
  long unsigned int last_address;
  long unsigned int last_handle;
  Object_Map *omap;
  Object_Map pvzmap;

#ifdef USE_TRACING
  /* Before starting, set the logging level, but only do so once.  */
  if (! env_var_checked_p)
    {
      char *probe = getenv ("LIBREDWG_TRACE");

      if (probe)
        loglevel = atoi (probe);
      env_var_checked_p = true;
    }
#endif  /* USE_TRACING */

  bit_chain_alloc(dat);

  /*------------------------------------------------------------
   * Header
   */
  strcpy ((char *)dat->chain, version_codes[dwg->header.version]); // Chain version
  dat->byte += 6;

  {
    struct Dwg_Header* _obj = &dwg->header;
    Dwg_Object *obj = NULL;

    #include "header.spec"
  }

  PRE(R_2004) {
    if (!dwg->header.num_sections) /* Usually 3-5, max 6 */
      dwg->header.num_sections = 6;
    bit_write_RL(dat, dwg->header.num_sections);
    section_address = dat->byte; // Jump to section address
    dat->byte += (dwg->header.num_sections * 9);
    bit_read_CRC(dat); // Check crc

    bit_write_sentinel(dat, dwg_sentinel(DWG_SENTINEL_HEADER_END));

    /*------------------------------------------------------------
     * AuxHeader section 5
     * R2000+, mostly redundant file header information
     */
    dwg->header.section[5].number = 5;
    dwg->header.section[5].address = dat->byte;
    //dwg->header.section[5].size = 0;
    if (dwg->header.num_sections == 6)
      {
        struct Dwg_AuxHeader* _obj = &dwg->auxheader;
        Dwg_Object *obj = NULL;

        #include "auxheader.spec"

        /*
        dwg->header.section[5].size = DWG_UNKNOWN5_SIZE;
        dwg->unknown5.size = dwg->header.section[5].size;
        dwg->unknown5.byte = dwg->unknown5.bit = 0;
        while (dat->byte + dwg->unknown5.size >= dat->size)
          bit_chain_alloc(dat);
        memcpy(&dat->chain[dat->byte], dwg->unknown5.chain, dwg->unknown5.size);
        dat->byte += dwg->unknown5.size;
        */
      }
  }

  /*------------------------------------------------------------
   * Picture (Pre-R13C3?)
   */
  if (dwg->header.preview_addr)
    {
      dat->byte = dwg->header.preview_addr;
      //dwg->picture.size = 0; // If one desires not to copy pictures,
      // should un-comment this line
      bit_write_sentinel(dat, dwg_sentinel(DWG_SENTINEL_PICTURE_BEGIN));
      for (j = 0; j < dwg->picture.size; j++)
        bit_write_RC(dat, dwg->picture.chain[j]);
      if (dwg->picture.size == 0)
        {
          bit_write_RL(dat, 5);
          bit_write_RC(dat, 0);
        }
      bit_write_sentinel(dat, dwg_sentinel(DWG_SENTINEL_PICTURE_END));
    }

  /*------------------------------------------------------------
   * Header Variables
   */
  dwg->header.section[0].number = 0;
  dwg->header.section[0].address = dat->byte;
  bit_write_sentinel(dat, dwg_sentinel(DWG_SENTINEL_VARIABLE_BEGIN));
  pvzadr = dat->byte; // Afterwards one must rewrite the correct values of size here

  bit_write_RL(dat, 0); // Size of the section

  // encode 
  dwg_encode_header_variables(dat, dwg);

  /* Write the size of the section at its beginning
   */
  pvzadr_2 = dat->byte;
  pvzbit = dat->bit;
  dat->byte = pvzadr;
  dat->bit = 0;
  bit_write_RL(dat, pvzadr_2 - pvzadr - (pvzbit ? 3 : 4));
  dat->byte = pvzadr_2;
  dat->bit = pvzbit;
  //printf ("Size: %lu\n", pvzadr_2 - pvzadr - (pvzbit ? 3 : 4));

  /* CRC and sentinel
   */
  bit_write_CRC(dat, pvzadr, 0xC0C1);

  //XXX trying to fix CRC 2-byte overflow. Must find actual reason
  dat->byte -= 2;

  bit_write_sentinel(dat, dwg_sentinel(DWG_SENTINEL_VARIABLE_END));
  dwg->header.section[0].size = dat->byte - dwg->header.section[0].address;

  /*------------------------------------------------------------
   * Classes
   */
  dwg->header.section[1].number = 1;
  dwg->header.section[1].address = dat->byte;
  bit_write_sentinel(dat, dwg_sentinel(DWG_SENTINEL_CLASS_BEGIN));
  pvzadr = dat->byte; // Afterwards one must rewrite the correct values of size here
  bit_write_RL(dat, 0); // Size of the section

  for (j = 0; j < dwg->num_classes; j++)
    {
      bit_write_BS(dat, dwg->dwg_class[j].number);
      bit_write_BS(dat, dwg->dwg_class[j].proxyflag);
      bit_write_TV(dat, dwg->dwg_class[j].appname);
      bit_write_TV(dat, dwg->dwg_class[j].cppname);
      bit_write_TV(dat, dwg->dwg_class[j].dxfname);
      bit_write_B(dat,  dwg->dwg_class[j].wasazombie);
      bit_write_BS(dat, dwg->dwg_class[j].item_class_id);
    }

  /* Write the size of the section at its beginning
   */
  pvzadr_2 = dat->byte;
  pvzbit = dat->bit;
  dat->byte = pvzadr;
  dat->bit = 0;
  bit_write_RL(dat, pvzadr_2 - pvzadr - (pvzbit ? 3 : 4));
  dat->byte = pvzadr_2;
  dat->bit = pvzbit;
  //printf ("Size: %lu\n", pvzadr_2 - pvzadr - (pvzbit ? 3 : 4));

  /* CRC and sentinel
   */
  bit_write_CRC(dat, pvzadr, 0xC0C1);

  bit_write_sentinel(dat, dwg_sentinel(DWG_SENTINEL_CLASS_END));
  dwg->header.section[1].size = dat->byte - dwg->header.section[1].address;

  bit_write_RL(dat, 0x00000000); // 0xDCA Unknown bitlong inter class and objects

  /*------------------------------------------------------------
   * Objects
   */

  pvzadr = dat->byte;

  /* Define object-map
   */
  omap = (Object_Map *) malloc(dwg->num_objects * sizeof(Object_Map));
  if (!omap) {
    LOG_ERROR("Out of memory"); return 2;
  }
  for (j = 0; j < dwg->num_objects; j++)
    {
      Bit_Chain nkn;
      Dwg_Handle tkt;

      /* Define the handle of each object, including unknown */
      omap[j].idc = j;
      if (dwg->object[j].supertype == DWG_SUPERTYPE_ENTITY)
        omap[j].handle = dwg->object[j].handle.value;
      else if (dwg->object[j].supertype == DWG_SUPERTYPE_OBJECT)
        omap[j].handle = dwg->object[j].handle.value;
      else if (dwg->object[j].supertype == DWG_SUPERTYPE_UNKNOWN)
        {
          nkn.chain = dwg->object[j].tio.unknown;
          nkn.size = dwg->object[j].size;
          nkn.byte = nkn.bit = 0;
          //FIXME read?? should write
          bit_read_BS(&nkn);
          bit_read_RL(&nkn);
          bit_read_H(&nkn, &tkt);
          omap[j].handle = tkt.value;
        }
      else
        omap[j].handle = 0x7FFFFFFF; /* Error! */

      /* Arrange the sequence of handles according to a growing order  */
      if (j > 0)
        {
          unsigned long k = j;
          while (omap[k].handle < omap[k - 1].handle)
            {
              pvzmap.handle = omap[k].handle;
              pvzmap.idc    = omap[k].handle;

              omap[k - 1].handle = pvzmap.handle;
              omap[k - 1].idc    = pvzmap.idc;

              omap[k].handle = omap[k - 1].handle;
              omap[k].idc    = omap[k - 1].idc;

              k--;
              if (k == 0)
                break;
            }
        }
    }
  //for (i = 0; i < dwg->num_objects; i++)
  //  printf ("Handle(%i): %lu / Idc: %u\n", i, omap[i].handle, omap[i].idc);

  /* Write the objects
   */
  for (j = 0; j < dwg->num_objects; j++)
    {
      Dwg_Object *obj;
      omap[j].address = dat->byte;
      obj = &dwg->object[omap[j].idc];
      if (obj->supertype == DWG_SUPERTYPE_UNKNOWN)
        {
          bit_write_MS(dat, obj->size);
          if (dat->byte + obj->size >= dat->size - 2)
            bit_chain_alloc(dat);
          memcpy(&dat->chain[dat->byte], obj->tio.unknown, obj->size);
          dat->byte += obj->size;
        }
      else
        {
	  if (obj->supertype == DWG_SUPERTYPE_ENTITY ||
              obj->supertype == DWG_SUPERTYPE_OBJECT)
	    dwg_encode_add_object(obj, dat, dat->byte);
	  /*
          if (obj->supertype == DWG_SUPERTYPE_ENTITY)
            dwg_encode_entity(obj, dat);
          else if (obj->supertype == DWG_SUPERTYPE_OBJECT)
            dwg_encode_object(obj, dat);
	  */
          else
            {
              LOG_ERROR("Error: undefined (super)type of object");
              exit(-1);
            }
        }
      bit_write_CRC(dat, omap[j].address, 0xC0C1);
    }
    for (j = 0; j < dwg->num_objects; j++) 
      LOG_INFO ("Object(%lu): %6lu / Address: %08lX / Idc: %u\n", 
		 j, omap[j].handle, omap[j].address, omap[j].idc);

  /* Unknown bitdouble between objects and object map
   */
  bit_write_RS(dat, 0);

  /*------------------------------------------------------------
   * Object-map
   */
  dwg->header.section[2].number = 2;
  dwg->header.section[2].address = dat->byte; // Value of size should be calculated later
  //printf ("Begin: 0x%08X\n", dat->byte);

  sekcisize = 0;
  pvzadr = dat->byte; // Correct value of section size must be written later
  dat->byte += 2;
  last_address = 0;
  last_handle = 0;
  for (j = 0; j < dwg->num_objects; j++)
    {
      unsigned int idc;
      long int pvz;

      idc = omap[j].idc;

      pvz = omap[idc].handle - last_handle;
      bit_write_MC(dat, pvz);
      //printf ("Handle(%i): %6lu / ", j, pvz);
      last_handle = omap[idc].handle;

      pvz = omap[idc].address - last_address;
      bit_write_MC(dat, pvz);
      //printf ("Address: %08X\n", pvz);
      last_address = omap[idc].address;

      //dwg dwg_encode_add_object(dwg->object[j], dat, last_address);

      ckr_missing = 1;
      if (dat->byte - pvzadr > 2030) // 2029
        {
          ckr_missing = 0;
          sekcisize = dat->byte - pvzadr;
          dat->chain[pvzadr] = sekcisize >> 8;
          dat->chain[pvzadr + 1] = sekcisize & 0xFF;
          bit_write_CRC(dat, pvzadr, 0xC0C1);

          pvzadr = dat->byte;
          dat->byte += 2;
          last_address = 0;
          last_handle = 0;
        }
    }
  //printf ("Obj size: %u\n", i);
  if (ckr_missing)
    {
      sekcisize = dat->byte - pvzadr;
      dat->chain[pvzadr] = sekcisize >> 8;
      dat->chain[pvzadr + 1] = sekcisize & 0xFF;
      bit_write_CRC(dat, pvzadr, 0xC0C1);
    }
  pvzadr = dat->byte;
  bit_write_RC(dat, 0);
  bit_write_RC(dat, 2);
  bit_write_CRC(dat, pvzadr, 0xC0C1);

  /* Calculate and write the size of the object map
   */
  dwg->header.section[2].size = dat->byte - dwg->header.section[2].address;
  free(omap);

  /*------------------------------------------------------------
   * Second header, section 3. R13-R2000 only.
   * But partially also since r2004.
   */
  SINCE(R_13)
  {
    struct _dwg_second_header* _obj = &dwg->second_header;
    Dwg_Object * obj = NULL;
    long vcount;

    dwg->header.section[3].number = 3;
    dwg->header.section[3].address = dwg->second_header.address;
    dwg->header.section[3].size = dwg->second_header.size;
    bit_write_sentinel(dat, dwg_sentinel(DWG_SENTINEL_SECOND_HEADER_BEGIN));

    LOG_INFO("\n=======> Second Header: %8X\n", (unsigned int) dat->byte - 16)
    pvzadr = dat->byte; // Keep the first address of the section to write its size later
    LOG_TRACE("pvzadr: %lx\n", pvzadr)

    FIELD_RL(size);
    if (FIELD_VALUE(address) != (BITCODE_RL)(pvzadr - 16))
      {
        LOG_WARN("second_header->address %x != %x",
                 FIELD_VALUE(address), (unsigned)(pvzadr - 16));
        FIELD_VALUE(address) = pvzadr - 16;
      }
    FIELD_BL(address);

    // AC1012, AC1014 or AC1015. This is a char[11], zero padded.
    // with \n at 12.
    for (i = 0; i < 12; i++)
      bit_write_RC(dat, _obj->version[i]);
    LOG_TRACE("version: %s\n", _obj->version)

    for (i = 0; i < 4; i++)
      {
        FIELD_B(null_b[i]);
      }
    // documented as 0x18,0x78,0x01,0x04 for R13, 0x18,0x78,0x01,0x05 for R14
    // but it is 0x10,0x7d,0xf4,0x78 on r14
    // also: 10 7d f4 78 on 2004
    for (i = 0; i < 4; i++)
      {
        FIELD_RC(unknown_rc4[i]);
      }

    UNTIL (R_2000) {
      /*
      // Fixed chain
      bit_write_RC(dat, 0x0F);
      bit_write_RC(dat, 0x14);
      bit_write_RC(dat, 0x64);
      bit_write_RC(dat, 0x78);
      bit_write_RC(dat, 0x01);
      bit_write_RC(dat, 0x06);
      */

      /* Addresses
       */
      for (i = 0; i < _obj->num_sections; i++)
        {
          FIELD_RC(sections[i].nr);
          FIELD_BL(sections[i].address);
          FIELD_BL(sections[i].size);
        }
      /*
        for (i = 0; i < 6; i++)
        {
        bit_write_RC(dat, 0);
        bit_write_BL(dat, dwg->header.section[0].address);
        bit_write_BL(dat, dwg->header.section[0].size);
        }
      */
      /* Handles */
      FIELD_BS(num_handlers); // 14, resp. 16 in r14

      if (FIELD_VALUE(num_handlers) != 14) {
        LOG_ERROR("Second header num_handlers != 14: %d\n", FIELD_VALUE(num_handlers));
        if (FIELD_VALUE(num_handlers) > 16)
          FIELD_VALUE(num_handlers) = 14;
      }
      for (i = 0; i < FIELD_VALUE(num_handlers); i++)
        {
          FIELD_RC(handlers[i].size);
          FIELD_RC(handlers[i].nr);
          FIELD_VECTOR(handlers[i].data, RC, handlers[i].size);
        }
      /*
        bit_write_BS(dat, 14);
        for (i = 0; i < 14; i++)
        {
        int i2;
        struct _handler *handle = &dwg->second_header.handlers[i];
        bit_write_RC(dat, handle->size);
        bit_write_RC(dat, i);
        for (i2 = 0; i2 < handle->size; i2++)
        bit_write_RC(dat, handle->data[i2]);
        }
      */

      /* Go back to begin to write the size
         pvzadr_2 = dat->byte;
         dat->byte = pvzadr;
         bit_write_RL(dat, pvzadr_2 - pvzadr + 10);
         dat->byte = pvzadr_2;
      */

      bit_write_CRC(dat, pvzadr, 0xC0C1);

      VERSION(R_14) {
        FIELD_RL(junk_r14_1);
        FIELD_RL(junk_r14_2);
      }
    }
    bit_write_sentinel(dat, dwg_sentinel(DWG_SENTINEL_SECOND_HEADER_END));

  } else if (dwg->header.num_sections >= 3) {
    dwg->header.section[3].number = 3;
    dwg->header.section[3].address = 0;
    dwg->header.section[3].size = 0;
  }

  /*------------------------------------------------------------
   * MEASUREMENT Section 4
   */
  if (dwg->header.num_sections >= 4)
    {
      dwg->header.section[4].number = 4;
      dwg->header.section[4].address = dat->byte;
      dwg->header.section[4].size = 4;
      bit_write_RL(dat, dwg->measurement);
    }

  /* End of the file
   */
  dat->size = dat->byte;

  /* Write section addresses
   */
  dat->byte = section_address;
  dat->bit = 0;
  for (j = 0; j < dwg->header.num_sections; j++)
    {
      bit_write_RC(dat, dwg->header.section[j].number);
      bit_write_RL(dat, dwg->header.section[j].address);
      bit_write_RL(dat, dwg->header.section[j].size);
    }

  /* Write CRC's
   */
  bit_write_CRC(dat, 0, 0);
  dat->byte -= 2;
  ckr = bit_read_CRC(dat);
  dat->byte -= 2;
  switch (dwg->header.num_sections)
    {
    case 3:
      bit_write_RS(dat, ckr ^ 0xA598);
      break;
    case 4:
      bit_write_RS(dat, ckr ^ 0x8101);
      break;
    case 5:
      bit_write_RS(dat, ckr ^ 0x3CC4);
      break;
    case 6:
      bit_write_RS(dat, ckr ^ 0x8461);
      break;
    default:
      bit_write_RS(dat, ckr);
    }

  return 0;
}

#include "dwg.spec"

/** dwg_encode_variable_type
 * encode object by class name, not type. if type > 500.
 * returns 1 if object could be encoded and 0 otherwise.
 */
static int
dwg_encode_variable_type(Dwg_Data * dwg, Bit_Chain * dat, Dwg_Object* obj)
{
  int i;
  char *dxfname;
  int is_entity;
  Dwg_Class *klass;

  if ((obj->type - 500) > dwg->num_classes)
    {
      LOG_WARN("Invalid object type %d, only %d classes", obj->type, dwg->num_classes);
      return 0;
    }

  i = obj->type - 500;
  klass = &dwg->dwg_class[i];
  dxfname = obj->dxfname = klass->dxfname;
  // almost always false
  is_entity = dwg_class_is_entity(klass);

#define UNHANDLED_CLASS \
      LOG_WARN("Unhandled Class %s %d %s (0x%x%s)", is_entity ? "entity" : "object",\
               klass->number, dxfname, klass->proxyflag,\
               klass->wasazombie ? " was proxy" : "")
#define UNTESTED_CLASS \
      LOG_WARN("Untested Class %s %d %s (0x%x%s)", is_entity ? "entity" : "object",\
               klass->number, dxfname, klass->proxyflag,\
               klass->wasazombie ? " was proxy" : "")

  if (!strcmp(dxfname, "ACDBDICTIONARYWDFLT"))
    {
      assert(!is_entity);
      dwg_encode_DICTIONARYWDLFT(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "DICTIONARYVAR"))
    {
      assert(!is_entity);
      dwg_encode_DICTIONARYVAR(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "HATCH"))
    {
      assert(!is_entity);
      dwg_encode_HATCH(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "GROUP"))
    {
      assert(!is_entity);
      dwg_encode_GROUP(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "IDBUFFER"))
    {
      assert(!is_entity);
      dwg_encode_IDBUFFER(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "IMAGE"))
    {
      assert(!is_entity);
      dwg_encode_IMAGE(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "IMAGEDEF"))
    {
      assert(!is_entity);
      dwg_encode_IMAGEDEF(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "IMAGEDEF_REACTOR"))
    {
      assert(!is_entity);
      dwg_encode_IMAGEDEF_REACTOR(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "LAYER_INDEX"))
    {
      assert(!is_entity);
      dwg_encode_LAYER_INDEX(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "LAYOUT"))
    {
      assert(!is_entity);
      dwg_encode_LAYOUT(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "LWPLINE"))
    {
      assert(!is_entity);
      dwg_encode_LWPLINE(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "MLEADER"))
    {
      UNTESTED_CLASS;
      dwg_encode_MLEADER(dat, obj);
      return 0;
    }
  if (!strcmp(dxfname, "MLEADERSTYLE"))
    {
      UNTESTED_CLASS;
      //dwg_encode_MLEADERSTYLE(dat, obj);
      return 0;
    }
  if (!strcmp(dxfname, "OLE2FRAME"))
    {
      assert(!is_entity);
      dwg_encode_OLE2FRAME(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "OBJECTCONTEXTDATA")
      || strcmp(klass->cppname, "AcDbObjectContextData"))
    {
      dwg_encode_OBJECTCONTEXTDATA(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "ACDBPLACEHOLDER"))
    {
      dwg_encode_PLACEHOLDER(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "PROXY"))
    {
      dwg_encode_PROXY(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "RASTERVARIABLES"))
    {
      dwg_encode_RASTERVARIABLES(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "SORTENTSTABLE"))
    {
      dwg_encode_SORTENTSTABLE(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "SPATIAL_FILTER"))
    {
      dwg_encode_SPATIAL_FILTER(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "SPATIAL_INDEX"))
    {
      dwg_encode_SPATIAL_INDEX(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "TABLE"))
    {
      dwg_encode_TABLE(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "XRECORD"))
    {
      dwg_encode_XRECORD(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "WIPEOUT"))
    {
      dwg_encode_WIPEOUT(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "FIELDLIST"))
    {
      UNTESTED_CLASS;
      dwg_encode_FIELDLIST(dat, obj);
      return 1;
    }
  if (!strcmp(dxfname, "AcDbField")) //???
    {
      UNTESTED_CLASS;
      dwg_encode_FIELD(dat, obj);
      return 0;
    }
  if (!strcmp(dxfname, "CELLSTYLEMAP"))
    {
      UNTESTED_CLASS;
      assert(!is_entity);
      dwg_encode_CELLSTYLEMAP(dat, obj);
      return 0;
    }
  if (!strcmp(dxfname, "VBA_PROJECT"))
    {
      // Has its own section?
      UNTESTED_CLASS;
      dwg_encode_VBA_PROJECT(dat, obj);
      return 0;
    }
  if (!strcmp(dxfname, "WIPEOUTVARIABLE"))
    {
      UNHANDLED_CLASS;
      assert(!is_entity);
      //dwg_encode_WIPEOUTVARIABLE(dat, obj);
      return 0;
    }
  if (!strcmp(dxfname, "VISUALSTYLE"))
    {
      UNHANDLED_CLASS;
      assert(!is_entity);
      //dwg_encode_VISUALSTYLE(dat, obj);
      return 0;
    }
  if (!strcmp(dxfname, "DIMASSOC"))
    {
      UNHANDLED_CLASS;
      assert(!is_entity);
      //dwg_encode_DIMASSOC(dat, obj);
      return 0;
    }
  if (!strcmp(dxfname, "MATERIAL"))
    {
      UNHANDLED_CLASS;
      assert(!is_entity);
      //dwg_encode_MATERIAL(dat, obj);
      return 0;
    }
  if (!strcmp(dxfname, "SCALE"))
    {
      UNTESTED_CLASS;
      assert(!is_entity);
      //SCALE has a name, bitsizes: 199,207,215,343,335,351,319
      dwg_encode_SCALE(dat, obj);
      return 0;
    }
  if (!strcmp(dxfname, "MLEADERSTYLE"))
    {
      UNHANDLED_CLASS;
      assert(!is_entity);
      //dwg_decode_MLEADERSTYLE(dat, obj);
      return 0;
    }
  if (!strcmp(dxfname, "TABLESTYLE"))
    {
      UNHANDLED_CLASS;
      assert(!is_entity);
      //dwg_decode_TABLESTYLE(dat, obj);
      return 0;
    }
  if (!strcmp(dxfname, "ACDBSECTIONVIEWSTYLE"))
    {
      UNHANDLED_CLASS;
      assert(!is_entity);
      //dwg_decode_SECTIONVIEWSTYLE(dat, obj);
      return 0;
    }
  if (!strcmp(dxfname, "ACDBDETAILVIEWSTYLE"))
    {
      UNHANDLED_CLASS;
      assert(!is_entity);
      //dwg_decode_DETAILVIEWSTYLE(dat, obj);
      return 0;
    }
  if (!strcmp(dxfname, "DBCOLOR"))
    {
      UNHANDLED_CLASS;
      //assert(!is_entity);
      //dwg_decode_DBCOLOR(dat, obj);
      return 0;
    }
  if (!strcmp(dxfname, "MLEADER"))
    {
      UNHANDLED_CLASS;
      //assert(!is_entity);
      //dwg_decode_MLEADER(dat, obj);
      return 0;
    }

  LOG_WARN("Unknown Class %s %d %s (0x%x%s)", is_entity ? "entity" : "object", \
           klass->number, dxfname, klass->proxyflag,                    \
           klass->wasazombie ? " was proxy" : "")

  /* TODO: CELLSTYLEMAP, DBCOLOR, MATERIAL, MLEADER, MLEADERSTYLE,
     PLOTSETTINGS, SCALE, TABLEGEOMETRY,
     TABLESTYLE, VBA_PROJECT, VISUALSTYLE, WIPEOUTVARIABLE,
     ACDBSECTIONVIEWSTYLE, ACDBDETAILVIEWSTYLE,
     NPOCOLLECTION, EXACXREFPANELOBJECT,
     ARCALIGNEDTEXT (2000+)
  */
#undef UNHANDLED_CLASS
#undef UNTESTED_CLASS

  return 0;
}

void
dwg_encode_add_object(Dwg_Object * obj, Bit_Chain * dat,
                      unsigned long address)
{
  unsigned long previous_address;
  unsigned long object_address;
  unsigned char previous_bit;

  /* Keep the previous address
   */
  previous_address = dat->byte;
  previous_bit = dat->bit;

  /* Use the indicated address for the object
   */
  dat->byte = address;
  dat->bit = 0;

  LOG_INFO("\n\n======================\nObject number: %u",
           obj->index)

  bit_write_MS(dat, obj->size);
  object_address = dat->byte;
  //  ktl_lastaddress = dat->byte + obj->size; /* (calculate the bitsize) */
  
  bit_write_BS(dat, obj->type);

  LOG_INFO(" Type: %d\n", obj->type)

  /* Check the type of the object
   */
  switch (obj->type)
    {
  case DWG_TYPE_TEXT:
    dwg_encode_TEXT(dat, obj);
    break;
  case DWG_TYPE_ATTRIB:
    dwg_encode_ATTRIB(dat, obj);
    break;
  case DWG_TYPE_ATTDEF:
    dwg_encode_ATTDEF(dat, obj);
    break;
  case DWG_TYPE_BLOCK:
    dwg_encode_BLOCK(dat, obj);
    break;
  case DWG_TYPE_ENDBLK:
    dwg_encode_ENDBLK(dat, obj);
    break;
  case DWG_TYPE_SEQEND:
    dwg_encode_SEQEND(dat, obj);
    break;
  case DWG_TYPE_INSERT:
    dwg_encode_INSERT(dat, obj);
    break;
  case DWG_TYPE_MINSERT:
    dwg_encode_MINSERT(dat, obj);
    break;
  case DWG_TYPE_VERTEX_2D:
    dwg_encode_VERTEX_2D(dat, obj);
    break;
  case DWG_TYPE_VERTEX_3D:
    dwg_encode_VERTEX_3D(dat, obj);
    break;
  case DWG_TYPE_VERTEX_MESH:
    dwg_encode_VERTEX_MESH(dat, obj);
    break;
  case DWG_TYPE_VERTEX_PFACE:
    dwg_encode_VERTEX_PFACE(dat, obj);
    break;
  case DWG_TYPE_VERTEX_PFACE_FACE:
    dwg_encode_VERTEX_PFACE_FACE(dat, obj);
    break;
  case DWG_TYPE_POLYLINE_2D:
    dwg_encode_POLYLINE_2D(dat, obj);
    break;
  case DWG_TYPE_POLYLINE_3D:
    dwg_encode_POLYLINE_3D(dat, obj);
    break;
  case DWG_TYPE_ARC:
    dwg_encode_ARC(dat, obj);
    break;
  case DWG_TYPE_CIRCLE:
    dwg_encode_CIRCLE(dat, obj);
    break;
  case DWG_TYPE_LINE:
    dwg_encode_LINE(dat, obj);
    break;
  case DWG_TYPE_DIMENSION_ORDINATE:
    dwg_encode_DIMENSION_ORDINATE(dat, obj);
    break;
  case DWG_TYPE_DIMENSION_LINEAR:
    dwg_encode_DIMENSION_LINEAR(dat, obj);
    break;
  case DWG_TYPE_DIMENSION_ALIGNED:
    dwg_encode_DIMENSION_ALIGNED(dat, obj);
    break;
  case DWG_TYPE_DIMENSION_ANG3PT:
    dwg_encode_DIMENSION_ANG3PT(dat, obj);
    break;
  case DWG_TYPE_DIMENSION_ANG2LN:
    dwg_encode_DIMENSION_ANG2LN(dat, obj);
    break;
  case DWG_TYPE_DIMENSION_RADIUS:
    dwg_encode_DIMENSION_RADIUS(dat, obj);
    break;
  case DWG_TYPE_DIMENSION_DIAMETER:
    dwg_encode_DIMENSION_DIAMETER(dat, obj);
    break;
  case DWG_TYPE_POINT:
    dwg_encode_POINT(dat, obj);
    break;
  case DWG_TYPE__3DFACE:
    dwg_encode__3DFACE(dat, obj);
    break;
  case DWG_TYPE_POLYLINE_PFACE:
    dwg_encode_POLYLINE_PFACE(dat, obj);
    break;
  case DWG_TYPE_POLYLINE_MESH:
    dwg_encode_POLYLINE_MESH(dat, obj);
    break;
  case DWG_TYPE_SOLID:
    dwg_encode_SOLID(dat, obj);
    break;
  case DWG_TYPE_TRACE:
    dwg_encode_TRACE(dat, obj);
    break;
  case DWG_TYPE_SHAPE:
    dwg_encode_SHAPE(dat, obj);
    break;
  case DWG_TYPE_VIEWPORT:
    dwg_encode_VIEWPORT(dat, obj);
    break;
  case DWG_TYPE_ELLIPSE:
    dwg_encode_ELLIPSE(dat, obj);
    break;
  case DWG_TYPE_SPLINE:
    dwg_encode_SPLINE(dat, obj);
    break;
  case DWG_TYPE_REGION:
    dwg_encode_REGION(dat, obj);
    break;
  case DWG_TYPE_3DSOLID:
    dwg_encode__3DSOLID(dat, obj);
    break;
  case DWG_TYPE_BODY:
    dwg_encode_BODY(dat, obj);
    break;
  case DWG_TYPE_RAY:
    dwg_encode_RAY(dat, obj);
    break;
  case DWG_TYPE_XLINE:
    dwg_encode_XLINE(dat, obj);
    break;
  case DWG_TYPE_DICTIONARY:
    dwg_encode_DICTIONARY(dat, obj);
    break;
  case DWG_TYPE_MTEXT:
    dwg_encode_MTEXT(dat, obj);
    break;
  case DWG_TYPE_LEADER:
    dwg_encode_LEADER(dat, obj);
    break;
  case DWG_TYPE_TOLERANCE:
    dwg_encode_TOLERANCE(dat, obj);
    break;
  case DWG_TYPE_MLINE:
    dwg_encode_MLINE(dat, obj);
    break;
  case DWG_TYPE_BLOCK_CONTROL:
    dwg_encode_BLOCK_CONTROL(dat, obj);
    break;
  case DWG_TYPE_BLOCK_HEADER:
    dwg_encode_BLOCK_HEADER(dat, obj);
    break;
  case DWG_TYPE_LAYER_CONTROL:
    dwg_encode_LAYER_CONTROL(dat, obj);
    break;
  case DWG_TYPE_LAYER:
    dwg_encode_LAYER(dat, obj);
    break;
  case DWG_TYPE_SHAPEFILE_CONTROL:
    dwg_encode_SHAPEFILE_CONTROL(dat, obj);
    break;
  case DWG_TYPE_SHAPEFILE:
    dwg_encode_SHAPEFILE(dat, obj);
    break;
  case DWG_TYPE_LTYPE_CONTROL:
    dwg_encode_LTYPE_CONTROL(dat, obj);
    break;
  case DWG_TYPE_LTYPE:
    dwg_encode_LTYPE(dat, obj);
    break;
  case DWG_TYPE_VIEW_CONTROL:
    dwg_encode_VIEW_CONTROL(dat, obj);
    break;
  case DWG_TYPE_VIEW:
    dwg_encode_VIEW(dat, obj);
    break;
  case DWG_TYPE_UCS_CONTROL:
    dwg_encode_UCS_CONTROL(dat, obj);
    break;
  case DWG_TYPE_UCS:
    dwg_encode_UCS(dat, obj);
    break;
  case DWG_TYPE_VPORT_CONTROL:
    dwg_encode_VPORT_CONTROL(dat, obj);
    break;
  case DWG_TYPE_VPORT:
    dwg_encode_VPORT(dat, obj);
    break;
  case DWG_TYPE_APPID_CONTROL:
    dwg_encode_APPID_CONTROL(dat, obj);
    break;
  case DWG_TYPE_APPID:
    dwg_encode_APPID(dat, obj);
    break;
  case DWG_TYPE_DIMSTYLE_CONTROL:
    dwg_encode_DIMSTYLE_CONTROL(dat, obj);
    break;
  case DWG_TYPE_DIMSTYLE:
    dwg_encode_DIMSTYLE(dat, obj);
    break;
  case DWG_TYPE_VP_ENT_HDR_CONTROL:
    dwg_encode_VP_ENT_HDR_CONTROL(dat, obj);
    break;
  case DWG_TYPE_VP_ENT_HDR:
    dwg_encode_VP_ENT_HDR(dat, obj);
    break;
  case DWG_TYPE_GROUP:
    dwg_encode_GROUP(dat, obj);
    break;
  case DWG_TYPE_MLINESTYLE:
    dwg_encode_MLINESTYLE(dat, obj);
    break;
  case DWG_TYPE_OLE2FRAME:
    dwg_encode_OLE2FRAME(dat, obj);
    break;
  case DWG_TYPE_DUMMY:
    dwg_encode_DUMMY(dat, obj);
    break;
  case DWG_TYPE_LONG_TRANSACTION:
    dwg_encode_LONG_TRANSACTION(dat, obj);
    break;
  case DWG_TYPE_LWPLINE:
    dwg_encode_LWPLINE(dat, obj);
    break;
  case DWG_TYPE_HATCH:
    dwg_encode_HATCH(dat, obj);
    break;
  case DWG_TYPE_XRECORD:
    dwg_encode_XRECORD(dat, obj);
    break;
  case DWG_TYPE_PLACEHOLDER:
    dwg_encode_PLACEHOLDER(dat, obj);
    break;
  case DWG_TYPE_PROXY_ENTITY:
    dwg_encode_PROXY_ENTITY(dat, obj);
    break;
  case DWG_TYPE_OLEFRAME:
    dwg_encode_OLEFRAME(dat, obj);
    break;
  case DWG_TYPE_VBA_PROJECT:
    LOG_ERROR("Unhandled Object VBA_PROJECT. Has its own section");
    //dwg_encode_VBA_PROJECT(dat, obj);
    break;
  case DWG_TYPE_LAYOUT:
    dwg_encode_LAYOUT(dat, obj);
    break;
  default:
      if (obj->type == obj->parent->layout_number)
        {
          dwg_encode_LAYOUT(dat, obj);
        }
      /* > 500:
         TABLE, DICTIONARYWDLFT, IDBUFFER, IMAGE, IMAGEDEF, IMAGEDEFREACTOR,
         LAYER_INDEX, OLE2FRAME, PROXY, RASTERVARIABLES, SORTENTSTABLE, SPATIAL_FILTER,
         SPATIAL_INDEX
      */
      else if (!dwg_encode_variable_type(obj->parent, dat, obj))
        {
          Dwg_Data *dwg = obj->parent;
          int is_entity;
          int i = obj->type - 500;
          Dwg_Class *klass = NULL;

          dat->byte = address;   // restart and write into the UNKNOWN_OBJ object
          dat->bit = 0;
          bit_write_MS(dat, obj->size); // size
          bit_write_BS(dat, obj->type); // type

          if (i <= (int)dwg->num_classes)
            {
              klass = &dwg->dwg_class[i];
              is_entity = dwg_class_is_entity(klass);
            }
          // properly dwg_decode_object/_entity for eed, reactors, xdic
          if (klass && !is_entity)
            {
              dwg_encode_UNKNOWN_OBJ(dat, obj);
            }
          else if (klass)
            {
              dwg_encode_UNKNOWN_ENT(dat, obj);
            }
          else // not a class
            {
              LOG_WARN("Unknown object, skipping eed/reactors/xdic");
              SINCE(R_2000)
              {
                bit_write_RL(dat, obj->bitsize);
                LOG_INFO("Object bitsize: " FORMAT_RL " @%lu.%u\n", obj->bitsize,
                         dat->byte, dat->bit);
              }
              bit_write_H(dat, &(obj->handle));
              LOG_INFO("Object handle: %d.%d.%lu\n",
                       obj->handle.code, obj->handle.size, obj->handle.value);
              object_address = dat->byte;
              // write obj->size bytes, excl. bitsize and handle
              // overshoot the bitsize and handle size
              for (i=0; i<(int)obj->size; i++) {
                bit_write_RC(dat, obj->tio.unknown[i]);
              }
              dat->byte = object_address;
            }
        }
    }

  /*
   if (obj->supertype != DWG_SUPERTYPE_UNKNOWN)
   {
     fprintf (stderr, "Begin address:\t%10lu\n", address);
     fprintf (stderr, "Last address:\t%10lu\tSize: %10lu\n", dat->byte, obj->size);
     fprintf (stderr, "End address:\t%10lu (calculated)\n", address + 2 + obj->size);
   }
   */

  /* Register the previous addresses for return
   */
  dat->byte = previous_address;
  dat->bit = previous_bit;
}

/* The first common part of every entity.

   The last common part is common_entity_handle_data.spec
   called by COMMON_ENTITY_HANDLE_DATA in dwg.spec
   See DWG_SUPERTYPE_ENTITY in dwg_encode_chains.
 */
static int
dwg_encode_entity(Dwg_Object * obj, Bit_Chain * dat)
{
  BITCODE_BS i, num_eed;
  BITCODE_BL bitsize;
  Dwg_Object_Entity* ent = obj->tio.entity;

  SINCE(R_2000)
    {
      bitsize = ent->bitsize;
      bit_write_RL(dat, bitsize);
    }
  bit_write_H(dat, &(obj->handle));

  num_eed = ent->num_eed;
  if (!num_eed) {
    bit_write_BS(dat, 0);
  } else {
    bit_write_BS(dat, ent->eed[0].size);
    for (i = 0; i < num_eed; i++)
      {
        BITCODE_BS j;
        LOG_TRACE("EED[%u] size: " FORMAT_BS "\n", i, ent->eed[i].size)
        bit_write_H(dat, &(ent->eed[i].handle));
        bit_write_RC(dat, ent->eed[i].data->code);
        LOG_TRACE("EED[%u] code: " FORMAT_RC "\n", i, ent->eed[i].data->code)
        for (j=0; j < ent->eed[i].size-2; j++)
          bit_write_RC(dat, ent->eed[i].data->u.raw[j]);

        if (i+1 < num_eed)
          bit_write_BS(dat, ent->eed[i+1].size);
        else
          bit_write_BS(dat, 0);
      }
  }

  bit_write_B(dat, ent->picture_exists);
  if (ent->picture_exists)
    {
      VERSIONS(R_13,R_2007)
        {
          bit_write_RL(dat, (BITCODE_RL)ent->picture_size);
        }
      SINCE(R_2007)
        {
          bit_write_BLL(dat, ent->picture_size);
        }
      if (ent->picture_size < 210210)
        {
          for (i=0; i< ent->picture_size; i++)
            bit_write_RC(dat, ent->picture[i]);
        }
      else 
        {
          LOG_ERROR(
              "dwg_encode_entity:  Absurd! Picture-size: %ld kB. "
              "Object: %lu (handle).",
              (long)(ent->picture_size / 1000), obj->handle.value)
          bit_advance_position(dat, -(4 * 8 + 1));
        }
     }
  
  VERSIONS(R_13,R_14)
    {
      bit_write_RL(dat, ent->bitsize);
    }

  bit_write_BB(dat, ent->entity_mode);
  bit_write_BL(dat, ent->num_reactors);

  SINCE(R_2004)
    {
     bit_write_B(dat, ent->xdic_missing_flag);
    }

  SINCE(R_2013)
    {
      bit_write_B(dat, ent->has_ds_binary_data);
    }

  VERSIONS(R_13,R_14)
    {
      bit_write_B(dat, ent->isbylayerlt );
    }

  bit_write_B(dat, ent->nolinks );
  bit_write_CMC(dat, &ent->color);
  bit_write_BD(dat, ent->linetype_scale);

  SINCE(R_2000)
    {
       bit_write_BB(dat, ent->linetype_flags);
       bit_write_BB(dat, ent->plotstyle_flags);
    }

  SINCE(R_2007)
    {
       bit_write_BB(dat, ent->material_flags);
       bit_write_RC(dat, ent->shadow_flags);
    }

  SINCE(R_2010)
    {
      bit_write_B(dat, ent->has_full_visualstyle);
      bit_write_B(dat, ent->has_face_visualstyle);
      bit_write_B(dat, ent->has_edge_visualstyle);
    }

   bit_write_BS(dat, ent->invisible);

  SINCE(R_2000)
    {
       bit_write_RC(dat, ent->lineweight);
    }
  return 0;
}

static void
dwg_encode_common_entity_handle_data(Bit_Chain * dat, Dwg_Object * obj)
{
  Dwg_Object_Entity *ent;
  Dwg_Data *dwg = obj->parent;
  int i;
  long unsigned int vcount;
  Dwg_Object_Entity *_obj;
  ent = obj->tio.entity;
  _obj = ent;

  #include "common_entity_handle_data.spec"
  
}

void
dwg_encode_handleref(Bit_Chain* dat, Dwg_Object* obj, Dwg_Data* dwg, Dwg_Object_Ref* ref)
{
  //this function should receive a Object_Ref without an abs_ref, calculate it and return a Dwg_Handle
  //this should be a higher level function 
  //not sure if the prototype is correct
  assert(obj);
}

void 
dwg_encode_handleref_with_code(Bit_Chain* dat, Dwg_Object* obj, Dwg_Data* dwg,
                               Dwg_Object_Ref* ref, unsigned int code)
{
  //XXX fixme. will this function be necessary?
  //create the handle, then check the code. will it be necessary?
  dwg_encode_handleref(dat, obj, dwg, ref);
  if (ref->handleref.code != code)
    {
      LOG_INFO("Warning: trying to write handle with wrong code.\n"
               "Expected code=%d, got %d.\n", code, ref->handleref.code)
    }
}

/* The first common part of every object.

   There is no COMMON_ENTITY_HANDLE_DATA for objects.
   See DWG_SUPERTYPE_OBJECT in dwg_encode_chains.
*/
static int
dwg_encode_object(Dwg_Object * obj, Bit_Chain * dat)
{
  BITCODE_BS i, num_eed;
  Dwg_Object_Object* ord = obj->tio.object;
  
  SINCE(R_2000)
    {
       bit_write_RL(dat, ord->bitsize);
    }

  bit_write_H(dat, &ord->object->handle);

  num_eed = ord->num_eed;
  if (!num_eed) {
    bit_write_BS(dat, 0);
  } else {
    bit_write_BS(dat, ord->eed[0].size);
    for (i = 0; i < num_eed; i++)
      {
        BITCODE_BS j;
        LOG_TRACE("EED[%u] size: " FORMAT_BS "\n", i, ord->eed[i].size)
        bit_write_H(dat, &(ord->eed[i].handle));
        bit_write_RC(dat, ord->eed[i].data->code);
        LOG_TRACE("EED[%u] code: " FORMAT_RC "\n", i, ord->eed[i].data->code)
        for (j=0; j < ord->eed[i].size-2; j++)
          bit_write_RC(dat, ord->eed[i].data->u.raw[j]);

        if (i+1 < num_eed)
          bit_write_BS(dat, ord->eed[i+1].size);
        else
          bit_write_BS(dat, 0);
      }
  }

  VERSIONS(R_13,R_14)
    {
       bit_write_RL(dat, ord->bitsize);
    }

   bit_write_BL(dat, ord->num_reactors);

  SINCE(R_2004)
    {
       bit_write_B(dat, ord->xdic_missing_flag);
    }
  return 0;
}

static void
dwg_encode_header_variables(Bit_Chain* dat, Dwg_Data * dwg)
{
  Dwg_Header_Variables* _obj = &dwg->header_vars;
  Dwg_Object* obj = NULL;

  #include "header_variables.spec"
}

#undef IS_ENCODER
