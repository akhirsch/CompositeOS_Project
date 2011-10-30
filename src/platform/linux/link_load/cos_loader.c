/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu; 2010 The George
 * Washington University, Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Linker and loader for the Composite system: takes a collection of
 * services with their trust relationships explicitly expressed,
 * dynamically generates their stub code to connect them, communicates
 * capability information info with the runtime system, creates the
 * user-level static capability structures, and loads the services
 * into the current address space which will be used as a template for
 * the run-time system for creating each service protection domain
 * (ie. copying the entries in the pgd to new address spaces.  
 *
 * This is trusted code, and any mistakes here compromise the entire
 * system.  Essentially, control flow is restricted/created here.
 *
 * Going by the man pages, I think I might be going to hell for using
 * strtok so much.  Suffice to say, don't multithread this program.
 */

#include <cos_config.h>
#ifdef LINUX_HIGHEST_PRIORITY
#define HIGHEST_PRIO
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <signal.h>

#include <bfd.h>

/* composite includes */
#include <spd.h>
#include <thread.h>
#include <ipc.h>

#include <cobj_format.h>

enum {PRINT_NONE = 0, PRINT_HIGH, PRINT_NORMAL, PRINT_DEBUG} print_lvl = PRINT_HIGH;

#define printl(lvl,format, args...)				\
	{							\
		if (lvl <= print_lvl) {				\
			printf(format, ## args);		\
			fflush(stdout);				\
		}						\
	}

#define NUM_ATOMIC_SYMBS 10 
#define NUM_KERN_SYMBS 1

const char *COMP_INFO   = "cos_comp_info";

const char *INIT_COMP   = "c0.o";
char *ROOT_SCHED        = NULL; // this is set to the first listed scheduler
const char *MPD_MGR     = "cg.o"; // the component graph!
const char *CONFIG_COMP = "schedconf.o";
const char *BOOT_COMP   = "boot.o";
const char *BOOT_COMP2  = "bootr.o";
const char *INIT_FILE   = "init.o", *INIT_FILE_NAME = "init.tar";

const char *ATOMIC_USER_DEF[NUM_ATOMIC_SYMBS] = 
{ "cos_atomic_cmpxchg",
  "cos_atomic_cmpxchg_end",
  "cos_atomic_user1",
  "cos_atomic_user1_end",
  "cos_atomic_user2",
  "cos_atomic_user2_end",
  "cos_atomic_user3",
  "cos_atomic_user3_end",
  "cos_atomic_user4",
  "cos_atomic_user4_end" };

#define CAP_CLIENT_STUB_DEFAULT "SS_ipc_client_marshal_args"
#define CAP_CLIENT_STUB_POSTPEND "_call"
#define CAP_SERVER_STUB_POSTPEND "_inv"

const char *SCHED_CREATE_FN = "sched_create_thread";
const char *fault_handlers[] = {"fault_page_fault_handler", NULL};

static inline int 
fault_handler_num(char *fn_name)
{
	int i;

	for (i = 0 ; fault_handlers[i] ; i++) {
		if (!strcmp(fault_handlers[i], fn_name)) return i;
	}
	return -1;
}

#define BASE_SERVICE_ADDRESS SERVICE_START
#define DEFAULT_SERVICE_SIZE SERVICE_SIZE

#define LINKER_BIN "/usr/bin/ld"
#define STRIP_BIN "/usr/bin/strip"
#define GCC_BIN "gcc"

/** 
 * gabep1: the SIG_S is a section devoted to the signal handling and
 * it must be in a page on its own one page away from the rest of the
 * module 
 */
enum {TEXT_S, RODATA_S, DATA_S, BSS_S, EH_FRAME_S, MAXSEC_S};

struct sec_info {
	asection *s;
	int offset;
};

unsigned long ro_start;
unsigned long data_start;

char script[64];
char tmp_exec[128];

int spdid_inc = -1;

struct sec_info srcobj[MAXSEC_S];
struct sec_info ldobj[MAXSEC_S];

#define UNDEF_SYMB_TYPE 0x1
#define EXPORTED_SYMB_TYPE 0x2
#define MAX_SYMBOLS 1024
#define MAX_TRUSTED 32
#define MAX_SYMB_LEN 256

typedef int (*observer_t)(asymbol *, void *data);

struct service_symbs;

struct symb {
	char *name;
	vaddr_t addr;
	struct service_symbs *exporter;
	struct symb *exported_symb;
};

struct symb_type {
	int num_symbs;
	struct symb symbs[MAX_SYMBOLS];

	struct service_symbs *parent;
};

struct dependency {
	struct service_symbs *dep;
	char *modifier;
	int mod_len;
	/* has a capability been created for this dependency */
	int resolved;
};

typedef enum {
	SERV_SECT_RO = 0,
	SERV_SECT_DATA,
	SERV_SECT_BSS,
	SERV_SECT_NUM
} serv_sect_type;

struct service_section {
	unsigned long offset;
	int size;
};

struct service_symbs {
	char *obj, *init_str;
	unsigned long lower_addr, size, allocated, heap_top;
	
	struct service_section sections[SERV_SECT_NUM];

	int is_composite_loaded;
	struct cobj_header *cobj;

	int is_scheduler;
	struct service_symbs *scheduler;
	
	struct spd *spd;
	struct symb_type exported, undef;
	int num_dependencies;
	struct dependency dependencies[MAX_TRUSTED];
	struct service_symbs *next;
	int depth;

	void *extern_info;
};

typedef enum {TRANS_CAP_NIL = 0, 
	      TRANS_CAP_FAULT, 
	      TRANS_CAP_OTHER} trans_cap_t;

static inline trans_cap_t
is_transparent_capability(struct symb *s) {
	char *n = s->name;
	
	if (!strcmp(n, SCHED_CREATE_FN)) return TRANS_CAP_OTHER;
	if (-1 != fault_handler_num(n))  return TRANS_CAP_FAULT;
	return TRANS_CAP_NIL;
}

static unsigned long getsym(bfd *obj, char* symbol)
{
	long storage_needed;
	asymbol **symbol_table;
	long number_of_symbols;
	int i;
	
	storage_needed = bfd_get_symtab_upper_bound (obj);
	
	if (storage_needed <= 0){
		printl(PRINT_DEBUG, "no symbols in object file\n");
		exit(-1);
	}
	
	symbol_table = (asymbol **) malloc (storage_needed);
	number_of_symbols = bfd_canonicalize_symtab(obj, symbol_table);

	//notes: symbol_table[i]->flags & (BSF_FUNCTION | BSF_GLOBAL)
	for (i = 0; i < number_of_symbols; i++) {
		if(!strcmp(symbol, symbol_table[i]->name)){
			return symbol_table[i]->section->vma + symbol_table[i]->value;
		} 
	}

	printl(PRINT_DEBUG, "Unable to find symbol named %s\n", symbol);
	return 0;
}

#ifdef DEBUG
static void print_syms(bfd *obj)
{
	long storage_needed;
	asymbol **symbol_table;
	long number_of_symbols;
	int i;
	
	storage_needed = bfd_get_symtab_upper_bound (obj);
	
	if (storage_needed <= 0){
		printl(PRINT_DEBUG, "no symbols in object file\n");
		exit(-1);
	}
	
	symbol_table = (asymbol **) malloc (storage_needed);
	number_of_symbols = bfd_canonicalize_symtab(obj, symbol_table);

	//notes: symbol_table[i]->flags & (BSF_FUNCTION | BSF_GLOBAL)
	for (i = 0; i < number_of_symbols; i++) {
		printl(PRINT_DEBUG, "name: %s, addr: %d, flags: %s, %s%s%s, in sect %s%s%s.\n",  
		       symbol_table[i]->name,
		       (unsigned int)(symbol_table[i]->section->vma + symbol_table[i]->value),
		       (symbol_table[i]->flags & BSF_GLOBAL) ? "global" : "local", 
		       (symbol_table[i]->flags & BSF_FUNCTION) ? "function" : "data",
		       symbol_table[i]->flags & BSF_SECTION_SYM ? ", section": "", 
		       symbol_table[i]->flags & BSF_FILE ? ", file": "", 
		       symbol_table[i]->section->name, 
		       bfd_is_und_section(symbol_table[i]->section) ? ", undefined" : "", 
		       symbol_table[i]->section->flags & SEC_RELOC ? ", relocate": "");
		//if(!strcmp(executive_entry_symbol, symbol_table[i]->name)){
		//return symbol_table[i]->section->vma + symbol_table[i]->value;
		//} 
	}

	free(symbol_table);
	//printl(PRINT_DEBUG, "Unable to find symbol named %s\n", executive_entry_symbol);
	//return -1;
	return;
}
#endif

static void findsections(bfd *abfd, asection *sect, PTR obj)
{
	struct sec_info *sec_arr = obj;
	
	if(!strcmp(sect->name, ".text")){
		sec_arr[TEXT_S].s = sect;
	}
	else if(!strcmp(sect->name, ".data")){
		sec_arr[DATA_S].s = sect;
	}
	else if(!strcmp(sect->name, ".rodata")){
		sec_arr[RODATA_S].s = sect;
	}
	else if(!strcmp(sect->name, ".bss")){
		sec_arr[BSS_S].s = sect;
	}
	else if (!strcmp(sect->name, ".eh_frame")) {
		sec_arr[EH_FRAME_S].s = sect;
	}
}

static int calc_offset(int offset, asection *sect)
{
	int align;
	
	if(!sect){
		return offset;
	}
	align = (1 << sect->alignment_power) - 1;
	
	if(offset & align){
		offset -= offset & align;
		offset += align + 1;
	}
	return offset;
}

static int calculate_mem_size(int first, int last) 
{
	int offset = 0;
	int i;
	
	for (i = first; i < last; i++){
		if(srcobj[i].s == NULL) {
			printl(PRINT_DEBUG, "Warning: could not find section for sectno %d.\n", i);
			continue;
		}
		offset = calc_offset(offset, srcobj[i].s);
		srcobj[i].offset = offset;
		offset += bfd_get_section_size(srcobj[i].s);
	}
	
	return offset;
}

static void emit_address(FILE *fp, unsigned long addr)
{
	fprintf(fp, ". = 0x%x;\n", (unsigned int)addr);
}

static void emit_section(FILE *fp, char *sec)
{
	/*
	 * The kleene star after the section will collapse
	 * .rodata.str1.x for all x into the only case we deal with
	 * which is .rodata
	 */
//	fprintf(fp, ".%s : { *(.%s*) }\n", sec, sec);
	fprintf(fp, ".%s : { *(.%s*) }\n", sec, sec);
}

/* Look at sections and determine sizes of the text and
 * and data portions of the object file once loaded */

static int genscript(int with_addr)
{
	FILE *fp;
	static unsigned int cnt = 0;
	
	sprintf(script, "/tmp/loader_script.%d", getpid());
	sprintf(tmp_exec, "/tmp/loader_exec.%d.%d.%d", with_addr, getpid(), cnt);
	cnt++;

	fp = fopen(script, "w");
	if(fp == NULL){
		perror("fopen failed");
		exit(-1);
	}
	
	fprintf(fp, "SECTIONS\n{\n");

	//if (with_addr) emit_address(fp, sig_start);
	//emit_section(fp, "signal_handling");
	if (with_addr) emit_address(fp, ro_start);
	emit_section(fp, "text");
	emit_section(fp, "rodata");
	if (with_addr) emit_address(fp, data_start);
	emit_section(fp, "data");
	emit_section(fp, "bss");
	if (with_addr) emit_address(fp, 0);
	emit_section(fp, "eh_frame");
	
	fprintf(fp, "}\n");
	fclose(fp);

	return 0;
}

static void run_linker(char *input_obj, char *output_exe)
{
	char linker_cmd[256];
	sprintf(linker_cmd, LINKER_BIN " -T %s -o %s %s", script, output_exe,
		input_obj);
	printl(PRINT_DEBUG, "%s", linker_cmd);
	fflush(stdout);
	system(linker_cmd);
}

#define bfd_sect_size(bfd, x) (bfd_get_section_size(x)/bfd_octets_per_byte(bfd))

int set_object_addresses(bfd *obj, struct service_symbs *obj_data)
{
	struct symb_type *st = &obj_data->exported;
	int i;

/* debug:
	unsigned int *retaddr;
	char **blah;
	typedef int (*fn_t)(void);
	fn_t fn;
	char *str = "hi, this is";

	if ((retaddr = (unsigned int*)getsym(obj, "ret")))
		printl(PRINT_DEBUG, "ret %x is %d.\n", (unsigned int)retaddr, *retaddr);
	if ((retaddr = (unsigned int*)getsym(obj, "blah")))
		printl(PRINT_DEBUG, "blah %x is %d.\n", (unsigned int)retaddr, *retaddr);
	if ((retaddr = (unsigned int*)getsym(obj, "zero")))
		printl(PRINT_DEBUG, "zero %x is %d.\n", (unsigned int)retaddr, *retaddr);
	if ((blah = (char**)getsym(obj, "str")))
		printl(PRINT_DEBUG, "str %x is %s.\n", (unsigned int)blah, *blah);
	if ((retaddr = (unsigned int*)getsym(obj, "other")))
		printl(PRINT_DEBUG, "other %x is %d.\n", (unsigned int)retaddr, *retaddr);
	if ((fn = (fn_t)getsym(obj, "foo")))
		printl(PRINT_DEBUG, "retval from foo: %d (%d).\n", fn(), (int)*str);
*/
	for (i = 0 ; i < st->num_symbs ; i++) {
		char *symb = st->symbs[i].name;
		unsigned long addr = getsym(obj, symb);
/*
		printl(PRINT_DEBUG, "Symbol %s at address 0x%x.\n", symb, 
		       (unsigned int)addr);
*/
		if (addr == 0) {
			printl(PRINT_DEBUG, "Symbol %s has invalid address.\n", symb);
			return -1;
		}

		st->symbs[i].addr = addr;
	}
	
	return 0;
}

vaddr_t get_symb_address(struct symb_type *st, const char *symb)
{
	int i;

	for (i = 0 ; i < st->num_symbs ; i++ ) {
		if (!strcmp(st->symbs[i].name, symb)) {
			return st->symbs[i].addr;
		}
	}
	return 0;
}

static int make_cobj_symbols(struct service_symbs *s, struct cobj_header *h);
static int make_cobj_caps(struct service_symbs *s, struct cobj_header *h);

static int load_service(struct service_symbs *ret_data, unsigned long lower_addr, 
			unsigned long size)
{
	bfd *obj, *objout;
	void *tmp_storage;

	int text_size, ro_size, ro_size_unaligned;
	int alldata_size, data_size;
	void *ret_addr;
	char *service_name = ret_data->obj; 
	struct cobj_header *h;
	int i;

	if (!service_name) {
		printl(PRINT_DEBUG, "Invalid path to executive.\n");
		return -1;
	}

	printl(PRINT_NORMAL, "Processing object %s:\n", service_name);

	genscript(0);
	run_linker(service_name, tmp_exec);
	
	obj = bfd_openr(tmp_exec, "elf32-i386");
	if(!obj){
		bfd_perror("object open failure");
		return -1;
	}
	if(!bfd_check_format(obj, bfd_object)){
		printl(PRINT_DEBUG, "Not an object file!\n");
		return -1;
	}
	
	for (i = 0 ; i < MAXSEC_S ; i++) srcobj[i].s = NULL;
	bfd_map_over_sections(obj, findsections, srcobj);

	ro_start = lower_addr;
	/* Determine the size of and allocate the text and Read-Only data area */
	text_size = calculate_mem_size(TEXT_S, RODATA_S);
	ro_size = calculate_mem_size(RODATA_S, DATA_S);
	printl(PRINT_DEBUG, "\tRead only text (%x) and data section (%x): %x:%x.\n",
	       (unsigned int)text_size, (unsigned int)ro_size, 
	       (unsigned int)ro_start, (unsigned int)text_size+ro_size);

	/* see calculate_mem_size for why we do this...not intelligent */
	ro_size_unaligned = calculate_mem_size(TEXT_S, DATA_S);
	ro_size = round_up_to_page(ro_size_unaligned);

	if (!ret_data->is_composite_loaded) {
		/**
		 * FIXME: needing PROT_WRITE is daft, should write to
		 * file, then map in ro
		 */
		assert(0 != ro_size);
		ret_addr = mmap((void*)ro_start, ro_size,
				PROT_EXEC | PROT_READ | PROT_WRITE,
				MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS,
				0, 0);
		if (MAP_FAILED == ret_addr){
			/* If you get outrageous sizes here, there is
			 * a required section missing (such as
			 * rodata).  Add a const. */
			printl(PRINT_DEBUG, "Error mapping 0x%x(0x%x)\n", 
			       (unsigned int)ro_start, (unsigned int)ro_size);
			perror("Couldn't map text segment into address space");
			return -1;
		}
	}
	
	data_start = ro_start + ro_size;
	/* Allocate the read-writable areas .data .bss */
	alldata_size = calculate_mem_size(DATA_S, MAXSEC_S);
	data_size = bfd_sect_size(obj, srcobj[DATA_S].s);

	printl(PRINT_DEBUG, "\tData section: %x:%x\n",
	       (unsigned int)data_start, (unsigned int)alldata_size);

	alldata_size = round_up_to_page(alldata_size);
	if (alldata_size != 0 && !ret_data->is_composite_loaded) {
		ret_addr = mmap((void*)data_start, alldata_size,
				PROT_WRITE | PROT_READ,
				MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS,
				0, 0);

		if (MAP_FAILED == ret_addr){
			perror("Couldn't map data segment into address space");
			return -1;
		}
	}
	
	tmp_storage = malloc(ro_size > alldata_size ? ro_size : alldata_size);
	if(tmp_storage == NULL)
	{
		perror("Memory allocation failed\n");
		return -1;
	}

	if (ret_data->is_composite_loaded) {
		u32_t size, obj_size;
		u32_t nsymbs, ncaps;
		char *mem;

		size = ro_size_unaligned + bfd_sect_size(obj, srcobj[DATA_S].s);
		nsymbs = ret_data->exported.num_symbs;
		ncaps = ret_data->undef.num_symbs;

		obj_size = cobj_size_req(3, size, nsymbs, ncaps);
		mem = malloc(obj_size);
		if (!mem) {
			printl(PRINT_HIGH, "could not allocate memory for composite-loaded %s.\n", service_name);
			return -1;
		}
		//printl(PRINT_HIGH, "Allocated %d byte cobj for %s w/ %d bytes data\n", obj_size, ret_data->obj, size);

		h = cobj_create(0, 3, size, nsymbs, ncaps, mem, obj_size);
		if (!h) {
			printl(PRINT_HIGH, "boot component: couldn't create cobj.\n");
			return -1;
		}
		ret_data->cobj = h;
		assert(obj_size == h->size);
	}
	
	unlink(tmp_exec);
	genscript(1);
	run_linker(service_name, tmp_exec);
	unlink(script);
	objout = bfd_openr(tmp_exec, "elf32-i386");
	if(!objout){
		bfd_perror("Object open failure\n");
		return -1;
	}
	if(!bfd_check_format(objout, bfd_object)){
		printl(PRINT_DEBUG, "Not an object file!\n");
		return -1;
	}
	
	bfd_map_over_sections(objout, findsections, &ldobj[0]);

	/* get the text and ro sections in a buffer */
	bfd_get_section_contents(objout, ldobj[TEXT_S].s,
				 tmp_storage + srcobj[TEXT_S].offset, 0,
				 bfd_sect_size(objout, srcobj[TEXT_S].s));
	printl(PRINT_DEBUG, "\tretreiving TEXT at offset %d of size %x.\n", 
	       srcobj[TEXT_S].offset, (unsigned int)bfd_sect_size(objout, srcobj[TEXT_S].s));

	if(srcobj[RODATA_S].s && ldobj[RODATA_S].s){
		bfd_get_section_contents(objout, ldobj[RODATA_S].s,
					 tmp_storage + srcobj[RODATA_S].offset, 0,
					 bfd_sect_size(objout, srcobj[RODATA_S].s));
		printl(PRINT_DEBUG, "\tretreiving RODATA at offset %d of size %x.\n", 
		       srcobj[RODATA_S].offset, (unsigned int)bfd_sect_size(objout, srcobj[RODATA_S].s));
	}

	ret_data->sections[SERV_SECT_RO].offset = srcobj[TEXT_S].offset;
	ret_data->sections[SERV_SECT_RO].size = bfd_sect_size(objout, srcobj[TEXT_S].s);
	if (srcobj[RODATA_S].s && ldobj[RODATA_S].s) {
		ret_data->sections[SERV_SECT_RO].size += bfd_sect_size(objout, srcobj[RODATA_S].s);
	}
//	assert((int)round_up_to_page(ret_data->sections[SERV_SECT_RO].size) == ro_size);
	assert(0 == ret_data->sections[SERV_SECT_RO].offset);

	if (!ret_data->is_composite_loaded) {
		/* 
		 * ... and copy that buffer into the actual memory location
		 * object is linked for (load it!)
		 */
		printl(PRINT_DEBUG, "\tCopying RO to %x from %x of size %x.\n", 
		       (unsigned int) ro_start, (unsigned int)tmp_storage, (unsigned int)ro_size);
		memcpy((void*)ro_start, tmp_storage, ro_size);
	} else {
		char *sect_loc;

		if (cobj_sect_init(h, 0, COBJ_SECT_READ, ro_start, ret_data->sections[SERV_SECT_RO].size)) {
			printl(PRINT_HIGH, "Could not create read-only section in cobj for %s\n", service_name);
			return -1;
		}
		sect_loc = cobj_sect_contents(h, 0);
		printl(PRINT_DEBUG, "\tSection @ %d, size %d, addr %x, sect start %d\n", (u32_t)sect_loc-(u32_t)h, 
		       cobj_sect_size(h, 0), cobj_sect_addr(h, 0), cobj_sect_content_offset(h));
		assert(sect_loc);
		memcpy(sect_loc, tmp_storage, ret_data->sections[SERV_SECT_RO].size);
	}

	printl(PRINT_DEBUG, "\tretreiving DATA at offset %x of size %x.\n", 
	       srcobj[DATA_S].offset, (unsigned int)bfd_sect_size(objout, srcobj[DATA_S].s));
	printl(PRINT_DEBUG, "\tCopying data from object to %x:%x.\n", 
	       (unsigned int)tmp_storage + srcobj[DATA_S].offset, 
	       (unsigned int)bfd_sect_size(obj, srcobj[DATA_S].s));

	/* and now do the same for the data and BSS */
	bfd_get_section_contents(objout, ldobj[DATA_S].s,
				 tmp_storage + srcobj[DATA_S].offset, 0,
				 bfd_sect_size(obj, srcobj[DATA_S].s));

	printl(PRINT_DEBUG, "\tretreiving BSS at offset %x of size %x.\n", 
	       srcobj[BSS_S].offset, (unsigned int)bfd_sect_size(objout, srcobj[BSS_S].s));
	printl(PRINT_DEBUG, "\tZeroing out BSS from %x of size %x.\n", 
	       (unsigned int)tmp_storage + srcobj[BSS_S].offset,
	       (unsigned int)bfd_sect_size(obj, srcobj[BSS_S].s));

	/* Zero bss */
	memset(tmp_storage + srcobj[BSS_S].offset, 0,
	       bfd_sect_size(obj, srcobj[BSS_S].s));

	ret_data->sections[SERV_SECT_DATA].offset = srcobj[DATA_S].offset;
	ret_data->sections[SERV_SECT_DATA].size = bfd_sect_size(objout, srcobj[DATA_S].s);
	ret_data->sections[SERV_SECT_BSS].offset = srcobj[BSS_S].offset;
	ret_data->sections[SERV_SECT_BSS].size = bfd_sect_size(objout, srcobj[BSS_S].s);

//	assert((ret_data->sections[SERV_SECT_BSS].offset - ret_data->sections[SERV_SECT_DATA].offset +
//		round_up_to_page(ret_data->sections[SERV_SECT_BSS].size)) == (unsigned int)alldata_size);

	if (!ret_data->is_composite_loaded) {
		printl(PRINT_DEBUG, "\tCopying DATA to %x from %x of size %x.\n", 
		       (unsigned int) data_start, (unsigned int)tmp_storage, (unsigned int)alldata_size);
		
		memcpy((void*)data_start, tmp_storage, alldata_size);
	} else {
		char *sect_loc;
		int ret;

		if (cobj_sect_init(h, 1, COBJ_SECT_READ | COBJ_SECT_WRITE, 
				   (u32_t)((char*)data_start + ret_data->sections[SERV_SECT_DATA].offset), 
				   ret_data->sections[SERV_SECT_DATA].size)) {
			printl(PRINT_HIGH, "Could not create data section in cobj for %s\n", service_name);
			return -1;
		}

		sect_loc = cobj_sect_contents(h, 1);
		printl(PRINT_DEBUG, "\tSection @ %d, size %d, addr %x, sect start %d\n", (u32_t)sect_loc-(u32_t)h, 
		       cobj_sect_size(h, 1), cobj_sect_addr(h, 1), cobj_sect_content_offset(h));
		memcpy(sect_loc, tmp_storage, ret_data->sections[SERV_SECT_DATA].size);

		if ((ret = cobj_sect_init(h, 2, COBJ_SECT_READ | COBJ_SECT_WRITE | COBJ_SECT_ZEROS, 
					  data_start + ret_data->sections[SERV_SECT_BSS].offset, 
					  ret_data->sections[SERV_SECT_BSS].size))) {
			printl(PRINT_HIGH, "Could not create bss section in cobj for %s (%d)\n", service_name, ret);
			return -1;
		}
 		sect_loc = cobj_sect_contents(h, 2);
		printl(PRINT_DEBUG, "Section @ %d, size %d, addr %x, sect start %d\n", 
		       sect_loc ? (u32_t)sect_loc-(u32_t)h : 0, 
		       cobj_sect_size(h, 2), cobj_sect_addr(h, 2), cobj_sect_content_offset(h));
	}
	
	free(tmp_storage);

	if (set_object_addresses(objout, ret_data)) {
		printl(PRINT_DEBUG, "Could not find all object symbols.\n");
		return -1;
	}

	ret_data->lower_addr = lower_addr;
	ret_data->size = size;
	ret_data->allocated = alldata_size + ro_size;
	ret_data->heap_top = (int)data_start + alldata_size;
	
	if (ret_data->is_composite_loaded) {
		if (make_cobj_symbols(ret_data, h)) {
			printl(PRINT_HIGH, "Could not create symbols in cobj for %s\n", service_name);
			return -1;
		}
/* 		if (make_cobj_caps(ret_data, h)) { */
/* 			printl(PRINT_HIGH, "Could not create capabilities in cobj for %s\n", service_name); */
/* 			return -1; */
/* 		} */
	}


	bfd_close(obj);
	bfd_close(objout);

	printl(PRINT_NORMAL, "Object %s processed as %s with script %s.\n", 
	       service_name, tmp_exec, script);
	unlink(tmp_exec);

	return 0;

	/* 
	 * FIXME: unmap sections, free memory, unlink file, close bfd
	 * objects, etc... for the various error positions, i.e. all
	 * return -1;s
	 */
}

/* FIXME: should modify code to use
static struct service_symbs *get_dependency_by_index(struct service_symbs *s,
						     int index)
{
	if (index >= s->num_dependencies) {
		return NULL;
	}

	return s->dependencies[index].dep;
}
*/
static int __add_service_dependency(struct service_symbs *s, struct service_symbs *dep, 
				    char *modifier, int mod_len)
{
	struct dependency *d;

	if (!s || !dep || s->num_dependencies == MAX_TRUSTED) {
		return -1;
	}
	if (!s->is_composite_loaded && dep->is_composite_loaded) {
		printl(PRINT_HIGH, "Error: Non-Composite-loaded component dependent on composite loaded component.\n");
		return -1;
	}

	d = &s->dependencies[s->num_dependencies];
	d->dep = dep;
	d->modifier = modifier;
	d->mod_len = mod_len;
	d->resolved = 0;
	s->num_dependencies++;

	return 0;
}

static int add_service_dependency(struct service_symbs *s, 
				  struct service_symbs *dep)
{
	return __add_service_dependency(s, dep, NULL, 0);
}

static int add_modified_service_dependency(struct service_symbs *s,
					   struct service_symbs *dep, 
					   char *modifier, int mod_len)
{
	char *new_mod;

	assert(modifier);
	new_mod = malloc(mod_len+1);
	assert(new_mod);
	memcpy(new_mod, modifier, mod_len);
	new_mod[mod_len] = '\0';

	return __add_service_dependency(s, dep, new_mod, mod_len);
}

static int initialize_service_symbs(struct service_symbs *str)
{
	memset(str, 0, sizeof(struct service_symbs));
	str->exported.parent = str;
	str->undef.parent = str;
	str->next = NULL;
	str->exported.num_symbs = str->undef.num_symbs = 0;
	str->num_dependencies = 0;
	str->depth = -1;

	return 0;
}

struct component_traits {
	int sched, composite_loaded;
};

static void parse_component_traits(char *name, struct component_traits *t, int *off)
{
	switch(name[*off]) {
	case '*': {
		t->sched = 1;
		if (!ROOT_SCHED) {
			char *r = malloc(strlen(name+1)+1);
			strcpy(r, name+1);
			ROOT_SCHED = r;
		}
		break;
	}
	case '!': t->composite_loaded = 1; break;
	default:  /* base case */          return;
	}
	(*off)++;
	parse_component_traits(name, t, off);
	return;
}

static struct service_symbs *alloc_service_symbs(char *obj)
{
	struct service_symbs *str;
	char *obj_name = malloc(strlen(obj)+1), *cpy, *orig, *pos;
	const char lassign = '(', *rassign = ")", *assign = "=";
	struct component_traits t = {.sched = 0, .composite_loaded = 0};
	int off = 0;

	parse_component_traits(obj, &t, &off);
	assert(obj_name);
	/* Do we have a value assignment (a component copy)?  Syntax
	 * is (newval=oldval),... */
	if (obj[off] == lassign) {
		char copy_cmd[256];
		int ret;
		
		off++;
		parse_component_traits(obj, &t, &off);

		cpy = strtok_r(obj+off, assign, &pos);
		orig = strtok_r(pos, rassign, &pos);
		sprintf(copy_cmd, "cp %s %s", orig, cpy);
		ret = system(copy_cmd);
		assert(-1 != ret);
		obj = cpy;
		off = 0;
	}
	printl(PRINT_DEBUG, "Processed object %s (%s%s)\n", obj, t.sched ? "scheduler " : "", 
	       t.composite_loaded ? "booted" : "");
	str = malloc(sizeof(struct service_symbs));
	if (!str || initialize_service_symbs(str)) {
		return NULL;
	}

	strcpy(obj_name, &obj[off]);
	str->obj = obj_name;

	str->is_scheduler = t.sched;
	str->scheduler = NULL;
	str->is_composite_loaded = t.composite_loaded;

	return str;
}

static void free_symbs(struct symb_type *st)
{
	int i;

	for (i = 0 ; i < st->num_symbs ; i++) {
		free(st->symbs[i].name);
	}
}

static void free_service_symbs(struct service_symbs *str)
{
	free(str->obj);
	free_symbs(&str->exported);
	free_symbs(&str->undef);
	free(str);

	return;
}

static int obs_serialize(asymbol *symb, void *data)
{
	struct symb_type *symbs = data;
	char *name;

	/* So that we can later add into the exported symbols the user
         * capability table
	 */
	if (symbs->num_symbs >= MAX_SYMBOLS-NUM_KERN_SYMBS) {
		printl(PRINT_DEBUG, "Have exceeded the number of allowed "
		       "symbols for object %s.\n", symbs->parent->obj);
		return -1;
	}

	/* Ignore main */
	if (!strcmp("main", symb->name)) {
		return 0;
	}

	name = malloc(strlen(symb->name) + 1);
	strcpy(name, symb->name);
	
	symbs->symbs[symbs->num_symbs].name = name;
	symbs->symbs[symbs->num_symbs].addr = 0;
	symbs->num_symbs++;

	return 0;
}

static int for_each_symb_type(bfd *obj, int symb_type, observer_t o, void *obs_data)
{
	long storage_needed;
	asymbol **symbol_table;
	long number_of_symbols;
	int i;
	
	storage_needed = bfd_get_symtab_upper_bound(obj);
	
	if (storage_needed <= 0){
		printl(PRINT_DEBUG, "no symbols in object file\n");
		exit(-1);
	}
	
	symbol_table = (asymbol **) malloc (storage_needed);
	number_of_symbols = bfd_canonicalize_symtab(obj, symbol_table);

	for (i = 0; i < number_of_symbols; i++) {
		/* 
		 * Invoke the observer if we are interested in a type,
		 * and the symbol is of that type where type is either
		 * undefined or exported, currently
		 */
		if ((symb_type & UNDEF_SYMB_TYPE &&
		    bfd_is_und_section(symbol_table[i]->section)) 
		    ||
		    (symb_type & EXPORTED_SYMB_TYPE &&
		    symbol_table[i]->flags & BSF_FUNCTION &&
		    symbol_table[i]->flags & BSF_GLOBAL)) {
			if ((*o)(symbol_table[i], obs_data)) {
				return -1;
			}
		}
	}

	free(symbol_table);

	return 0;
}

/*
 * Fill in the symbols of service_symbs for the object passed in as
 * the tmp_exec
 */
static int obj_serialize_symbols(char *tmp_exec, int symb_type, struct service_symbs *str) 
{
 	bfd *obj; 
	struct symb_type *st;

	obj = bfd_openr(tmp_exec, "elf32-i386");
	if(!obj){
		printl(PRINT_HIGH, "Attempting to open %s\n", str->obj);
		bfd_perror("Object open failure");
		return -1;
	}
	
	if(!bfd_check_format(obj, bfd_object)){
		printl(PRINT_DEBUG, "Not an object file!\n");
		return -1;
	}
	
	if (symb_type == UNDEF_SYMB_TYPE) {
		st = &str->undef;
	} else if (symb_type == EXPORTED_SYMB_TYPE) {
		st = &str->exported;
	} else {
		printl(PRINT_HIGH, "attempt to view unknown symbol type\n");
		exit(-1);
	}
	for_each_symb_type(obj, symb_type, obs_serialize, st);

	bfd_close(obj);

	return 0;
}

static inline void print_symbs(struct symb_type *st)
{
	int i;

	for (i = 0 ; i < st->num_symbs ; i++) {
		printl(PRINT_DEBUG, "%s, ", st->symbs[i].name);
	}

	return;
}

static void print_objs_symbs(struct service_symbs *str)
{
	if (print_lvl < PRINT_DEBUG) return;
	while (str) {
		printl(PRINT_DEBUG, "Service %s:\n\tExported functions: ", str->obj);
		print_symbs(&str->exported);
		printl(PRINT_DEBUG, "\n\tUndefined: ");
		print_symbs(&str->undef);
		printl(PRINT_DEBUG, "\n\n");

		str = str->next;
	}

	return;
}

/*
 * Has this service already been processed?
 */
static inline int service_processed(char *obj_name, struct service_symbs *services)
{
	while (services) {
		if (!strcmp(services->obj, obj_name)) {
			return 1;
		}
		services = services->next;
	}

	return 0;
}

static inline void 
__add_symb(const char *name, struct symb_type *exp_undef)
{
	exp_undef->symbs[exp_undef->num_symbs].name = malloc(strlen(name)+1);
	strcpy(exp_undef->symbs[exp_undef->num_symbs].name, name);
	exp_undef->num_symbs++;
	assert(exp_undef->num_symbs <= MAX_SYMBOLS);
}

static inline void add_kexport(struct service_symbs *ss, const char *name)
{
	struct symb_type *ex = &ss->exported;
	__add_symb(name, ex);
	return;
}

static inline void add_undef_symb(struct service_symbs *ss, const char *name)
{
	struct symb_type *ud = &ss->undef;
	__add_symb(name, ud);
	return;
}

/* 
 * Assume that these are added LAST.  The last NUM_KERN_SYMBS are
 * ignored for most purposes so they must be the actual kern_syms.
 *
 * The kernel needs to know where a few symbols are, add them:
 * user_caps, cos_sched_notifications
 */
static void add_kernel_exports(struct service_symbs *service)
{
	add_kexport(service, COMP_INFO);

	return;
}

/* 
 * Obtain the list of undefined and exported symbols for a collection
 * of services.
 *
 * services is an array of comma delimited addresses to the services
 * we wish to get the symbol information for.  Note that all ',' in
 * the services string are replaced with '\0', and that this function
 * is not thread-safe due to use of strtok.
 *
 * Returns a linked list struct service_symbs data structure with the
 * arrays within each service_symbs filled in to reflect the symbols
 * within that service.
 */
static struct service_symbs *prepare_service_symbs(char *services)
{
	struct service_symbs *str, *first;
	const char *init_delim = ",", *serv_delim = ";";
	char *tok, *init_str;
	int len;
	
	printl(PRINT_DEBUG, "Prepare the list of components.\n");
	
	tok = strtok(services, init_delim);
	first = str = alloc_service_symbs(tok);
	init_str = strtok(NULL, serv_delim);
	len = strlen(init_str)+1;
	str->init_str = malloc(len);
	assert(str->init_str);
	memcpy(str->init_str, init_str, len);

	do {
		if (obj_serialize_symbols(str->obj, EXPORTED_SYMB_TYPE, str) ||
		    obj_serialize_symbols(str->obj, UNDEF_SYMB_TYPE, str)) {
			printl(PRINT_DEBUG, "Could not operate on object %s: error.\n", tok);
			return NULL;
		}
		add_kernel_exports(str);
		tok = strtok(NULL, init_delim);
		if (tok) {
			str->next = alloc_service_symbs(tok);
			str = str->next;

			init_str = strtok(NULL, serv_delim);
			len = strlen(init_str)+1;
			str->init_str = malloc(len);
			assert(str->init_str);
			memcpy(str->init_str, init_str, len);
		}
	} while (tok);
		
	return first;
}


/*
 * Find the exporter for a specific symbol from amongst a list of
 * exporters.
 */
static inline 
struct service_symbs *find_symbol_exporter_mark_resolved(struct symb *s, 
							 struct dependency *exporters,
							 int num_exporters, struct symb **exported)
{
	int i,j;

	for (i = 0 ; i < num_exporters ; i++) {
		struct dependency *exporter;
		struct symb_type *exp_symbs;

		exporter = &exporters[i];
		exp_symbs = &exporter->dep->exported;

		for (j = 0 ; j < exp_symbs->num_symbs ; j++) {
			if (!strcmp(s->name, exp_symbs->symbs[j].name)) {
				*exported = &exp_symbs->symbs[j];
				exporters[i].resolved = 1;
				return exporters[i].dep;
			}
			if (exporter->modifier && 
			    !strncmp(s->name, exporter->modifier, exporter->mod_len) && 
			    !strcmp(s->name + exporter->mod_len, exp_symbs->symbs[j].name)) {
				*exported = &exp_symbs->symbs[j];
				exporters[i].resolved = 1;
				return exporters[i].dep;
			}
		}
	}

	return NULL;
}

static int 
create_transparent_capabilities(struct service_symbs *service)
{
	int i, j;
	struct dependency *dep = service->dependencies;

	for (i = 0 ; i < service->num_dependencies ; i++) {
		struct symb_type *symbs = &dep[i].dep->exported;

		if (dep[i].resolved) continue;

		for (j = 0 ; j < symbs->num_symbs ; j++) {
			trans_cap_t r;
			r = is_transparent_capability(&symbs->symbs[j]);
			switch (r) {
			case TRANS_CAP_FAULT: 
			case TRANS_CAP_OTHER: 
			{
				struct symb_type *st;
				struct symb *s;

				add_undef_symb(service, symbs->symbs[j].name);
				st = &service->undef;
				s = &st->symbs[st->num_symbs-1];
				s->exporter = dep[i].dep;
				s->exported_symb = &symbs->symbs[j];

				//printf("<<<<<<<<<<<<%s-%s, %s>>>>>>>>>>>>\n", 
				//service->obj, s->exporter->obj, s->exported_symb->name);
			
				dep[i].resolved = 1;
				break;
			}
			case TRANS_CAP_NIL: break;
			}
		}
		if (!dep[i].resolved) {
			printl(PRINT_HIGH, "Warning: dependency %s-%s "
			       "is not creating a capability.\n", 
			       service->obj, dep[i].dep->obj);
		}
	}

	return 0;
}

/*
 * Verify that all symbols can be resolved by the present dependency
 * relations.  This is an equivalent to programming language
 * "completeness".
 *
 * Assumptions: All exported and undefined symbols are defined for
 * each service (prepare_service_symbs has been called), and that the
 * tree of services has been established designating the dependents of
 * each service (process_dependencies has been called).
 */
static int verify_dependency_completeness(struct service_symbs *services)
{
	struct service_symbs *start = services;
	int ret = 0;
	int i;

	/* for each of the services... */
	for (; services ; services = services->next) {
		struct symb_type *undef_symbs = &services->undef;

		/* ...go through each of its undefined symbols... */
		for (i = 0 ; i < undef_symbs->num_symbs ; i++) {
			struct symb *symb = &undef_symbs->symbs[i];
			struct symb *exp_symb;
			struct service_symbs *exporter;

			/* 
			 * ...and make sure they are matched to an
			 * exported function in a service we are
			 * dependent on.
			 */
			exporter = find_symbol_exporter_mark_resolved(symb, services->dependencies, 
								      services->num_dependencies, &exp_symb);
			if (!exporter) {
				printl(PRINT_HIGH, "Could not find exporter of symbol %s in service %s.\n",
				       symb->name, services->obj);

				ret = -1;
				goto exit;
			} else {
				symb->exporter = exporter;
				symb->exported_symb = exp_symb;
			}

			/* if (exporter->is_scheduler) { */
			/* 	if (NULL == services->scheduler) { */
			/* 		services->scheduler = exporter; */
			/* 		//printl(PRINT_HIGH, "%s has scheduler %s.\n", services->obj, exporter->obj); */
			/* 	} else if (exporter != services->scheduler) { */
			/* 		printl(PRINT_HIGH, "Service %s is dependent on more than one scheduler (at least %s and %s).  Error.\n", services->obj, exporter->obj, services->scheduler->obj); */
			/* 		ret = -1; */
			/* 		goto exit; */
			/* 	} */
			/* } */
		}
	}

	for (services = start ; services ; services = services->next) {
		create_transparent_capabilities(services);
	}
 exit:
	return ret;
}

static int rec_verify_dag(struct service_symbs *services,
			  int current_depth, int max_depth)
{
	int i;

	/* cycle */
	if (current_depth > max_depth) {
		return -1;
	}

	if (current_depth > services->depth) {
		services->depth = current_depth;
	}

	for (i = 0 ; i < services->num_dependencies ; i++) {
		struct service_symbs *d = services->dependencies[i].dep;

		if (rec_verify_dag(d, current_depth+1, max_depth)) {
			printl(PRINT_HIGH, "Component %s found in cycle\n", d->obj);
			return -1;
		}
	}

	return 0;
}

/*
 * FIXME: does not check for disjoint graphs at this time.
 *
 * The only soundness we can really check here is that services are
 * arranged in a DAG, i.e. that no cycles exist.  O(N^2*E).
 *
 * Assumptions: All exported and undefined symbols are defined for
 * each service (prepare_service_symbs has been called), and that the
 * tree of services has been established designating the dependents of
 * each service (process_dependencies has been called).
 */
static int verify_dependency_soundness(struct service_symbs *services)
{
	struct service_symbs *tmp_s = services;
	int cnt = 0;

	while (tmp_s) {
		cnt++;
		tmp_s = tmp_s->next;
	}

	while (services) {
		if (rec_verify_dag(services, 0, cnt)) {
			printl(PRINT_DEBUG, "Cycle found in dependencies.  Not linking.\n");
			return -1;
		}

		services = services->next;
	}

	return 0;
}

static inline struct service_symbs *get_service_struct(char *name, 
						       struct service_symbs *list)
{
	while (list) {
		assert(name);
		assert(list && list->obj);
		if (!strcmp(name, list->obj)) {
			return list;
		}

		list = list->next;
	}

	return NULL;
}

/*
 * Add to the service_symbs structures the dependents.
 * 
 * deps is formatted as "sa-sb|sc|...|sn;sd-se|sf|...;...", or a list
 * of "service" hyphen "dependencies...".  In the above example, sa
 * depends on functions within sb, sc, and sn.
 */
static int deserialize_dependencies(char *deps, struct service_symbs *services)
{
	char *next, *current;
	char *serial = "-";
	char *parallel = "|";
	char inter_dep = ';';
	char open_modifier = '[';
	char close_modifier = ']';

	if (!deps) return -1;
	next = current = deps;

	/* go through each dependent-trusted|... relation */
	while (current) {
		struct service_symbs *s, *dep;
		char *tmp;

		next = strchr(current, inter_dep);
		if (next) {
			*next = '\0';
			next++;
		}
		/* the dependent */
		tmp = strtok(current, serial);
		s = get_service_struct(tmp, services);
		if (!s) {
			printl(PRINT_HIGH, "Could not find service %s.\n", tmp);
			return -1;
		}

		/* go through the | invoked services */
		tmp = strtok(NULL, parallel);
		while (tmp) {
			char *mod = NULL;
			/* modifier! */
			if (tmp[0] == open_modifier) {
				mod = tmp+1;
				tmp = strchr(tmp, close_modifier);
				if (!tmp) {
					printl(PRINT_HIGH, "Could not find closing modifier ] in %s\n", mod);
					return -1;
				}
				*tmp = '\0';
				tmp++;
				assert(mod);
				assert(tmp);
			}
			dep = get_service_struct(tmp, services);
			if (!dep) {
				printl(PRINT_HIGH, "Could not find service %s.\n", tmp);
				return -1;
			} 
			if (dep == s) {
				printl(PRINT_HIGH, "Reflexive relations not allowed (for %s).\n", 
				       s->obj);
				return -1;
			}

			if (!s->is_composite_loaded && dep->is_composite_loaded) {
				printl(PRINT_HIGH, "Error: Non-Composite-loaded component %s dependent "
				       "on composite loaded component %s.\n", s->obj, dep->obj);
				return -1;
			}

			if (mod) add_modified_service_dependency(s, dep, mod, strlen(mod));
			else add_service_dependency(s, dep);

			if (dep->is_scheduler) {
				if (NULL == s->scheduler) {
					s->scheduler = dep;
				} else if (dep != s->scheduler) {
					printl(PRINT_HIGH, "Service %s is dependent on more than "
					       "one scheduler (at least %s and %s).  Error.\n", 
					       s->obj, dep->obj, s->scheduler->obj);
					return -1;
				}
			}

			tmp = strtok(NULL, parallel);
		} 

		current = next;
	}

	return 0;
}

static char *strip_prepended_path(char *name)
{
	char *tmp;

	tmp = strrchr(name, '/');

	if (!tmp) {
		return name;
	} else {
		return tmp+1;
	}
}

/*
 * Produces a number of object files in /tmp named objname.o.pid.o
 * with no external dependencies.
 *
 * gen_stub_prog is the address to the client stub generation prog
 * st_object is the address of the symmetric trust object.
 *
 * This is kind of a big hack.
 */
static void gen_stubs_and_link(char *gen_stub_prog, struct service_symbs *services)
{
	int pid = getpid();
	char tmp_str[2048];

	while (services) {
		int i;
		struct symb_type *symbs = &services->undef;
		char dest[256];
		char tmp_name[256];
		char *obj_name, *orig_name, *str;

		orig_name = services->obj;
		obj_name = strip_prepended_path(services->obj);
		sprintf(tmp_name, "/tmp/%s.%d", obj_name, pid);
		
/*		if (symbs->num_symbs == 0) {
			sprintf(tmp_str, "cp %s %s.o", 
				orig_name, tmp_name);
			system(tmp_str);

			str = malloc(strlen(tmp_name)+3);
			strcpy(str, tmp_name);
			strcat(str, ".o");
			free(services->obj);
			services->obj = str;

			services = services->next;
			continue;
		}
*/
		/* make the command line for an invoke the stub generator */
		strcpy(tmp_str, gen_stub_prog);

		if (symbs->num_symbs > 0) {
			strcat(tmp_str, " ");
			strcat(tmp_str, symbs->symbs[0].name);
		}
		for (i = 1 ; i < symbs->num_symbs ; i++) {
			strcat(tmp_str, ",");
			strcat(tmp_str, symbs->symbs[i].name);
		}

		/* invoke the stub generator */
		sprintf(dest, " > %s_stub.S", tmp_name);
		strcat(tmp_str, dest);
		system(tmp_str);

		/* compile the stub */
		sprintf(tmp_str, GCC_BIN " -c -o %s_stub.o %s_stub.S", 
			tmp_name, tmp_name);
		system(tmp_str);

		/* link the stub to the service */
		sprintf(tmp_str, LINKER_BIN " -r -o %s.o %s %s_stub.o", 
			tmp_name, orig_name, tmp_name);
		system(tmp_str);

		/* Make service names reflect their new linked versions */
		str = malloc(strlen(tmp_name)+3);
		strcpy(str, tmp_name);
		strcat(str, ".o");
		free(services->obj);
		services->obj = str;
		
		sprintf(tmp_str, "rm %s_stub.o %s_stub.S", tmp_name, tmp_name);
		system(tmp_str);

		services = services->next;
	}

	return;
}

/*
 * Load into the current address space all of the services.  
 *
 * FIXME: Load intelligently, from the most trusted to the least in
 * some order instead of randomly.  This will be important when we do
 * dynamically loading.
 *
 * Assumes that a file exists for each service in /tmp/service.o.pid.o
 * (i.e. that gen_stubs_and_link has been called.)
 */
static int load_all_services(struct service_symbs *services)
{
	unsigned long service_addr = BASE_SERVICE_ADDRESS;

	service_addr += DEFAULT_SERVICE_SIZE;

	while (services) {
		if (load_service(services, service_addr, DEFAULT_SERVICE_SIZE)) {
			return -1;
		}

		service_addr += DEFAULT_SERVICE_SIZE;
		if (strstr(services->obj, BOOT_COMP)) {
			service_addr += DEFAULT_SERVICE_SIZE;
		}

		printl(PRINT_DEBUG, "\n");
		services = services->next;
	}

	return 0;
}

static void print_kern_symbs(struct service_symbs *services)
{
	const char *u_tbl = COMP_INFO;

	while (services) {
		vaddr_t addr;

		if ((addr = get_symb_address(&services->exported, u_tbl))) {
			printl(PRINT_DEBUG, "Service %s:\n\tusr_cap_tbl: %x\n",
			       services->obj, (unsigned int)addr);
		}
		
		services = services->next;
	}
}

/* static void add_spds(struct service_symbs *services) */
/* { */
/* 	struct service_symbs *s = services; */
	
/* 	/\* first, make sure that all services have spds *\/ */
/* 	while (s) { */
/* 		int num_undef = s->undef.num_symbs; */
/* 		struct usr_inv_cap *ucap_tbl; */

/* 		ucap_tbl = (struct usr_inv_cap*)get_symb_address(&s->exported,  */
/* 								 USER_CAP_TBL_NAME); */
/* 		/\* no external dependencies, no caps *\/ */
/* 		if (!ucap_tbl) { */
/* 			s->spd = spd_alloc(0, MNULL); */
/* 		} else { */
/* //			printl(PRINT_DEBUG, "Requesting %d caps.\n", num_undef); */
/* 			s->spd = spd_alloc(num_undef, ucap_tbl); */
/* 		} */

/* //		printl(PRINT_DEBUG, "Service %s has spd %x.\n", s->obj, (unsigned int)s->spd); */

/* 		s = s->next; */
/* 	} */

/* 	/\* then add the capabilities *\/ */
/* 	while (services) { */
/* 		int i; */
/* 		int num_undef = services->undef.num_symbs; */

/* 		for (i = 0 ; i < num_undef ; i++) { */
/* 			struct spd *owner_spd, *dest_spd; */
/* 			struct service_symbs *dest_service; */
/* 			vaddr_t dest_entry_fn; */
/* 			struct symb *symb; */
			
/* 			owner_spd = services->spd; */
/* 			symb = &services->undef.symbs[i]; */
/* 			dest_service = symb->exporter; */

/* 			symb = symb->exported_symb; */
/* 			dest_spd = dest_service->spd; */
/* 			dest_entry_fn = symb->addr; */

/* 			if ((spd_add_static_cap(services->spd, dest_entry_fn, dest_spd, IL_ST) == 0)) { */
/* 				printl(PRINT_DEBUG, "Could not add capability for %s to %s.\n",  */
/* 				       symb->name, dest_service->obj); */
/* 			} */
/* 		} */

/* 		services = services->next; */
/* 	} */

/* 	return; */
/* } */

/* void start_composite(struct service_symbs *services) */
/* { */
/* 	struct thread *thd; */

/* 	spd_init(); */
/* 	ipc_init(); */
/* 	thd_init(); */

/* 	add_spds(services); */

/* 	thd = thd_alloc(services->spd); */

/* 	if (!thd) { */
/* 		printl(PRINT_DEBUG, "Could not allocate thread.\n"); */
/* 		return; */
/* 	} */

/* 	thd_set_current(thd); */

/* 	return; */
/* } */

#include "../module/aed_ioctl.h"

/*
 * FIXME: all the exit(-1) -> return NULL, and handling in calling
 * function.
 */
/*struct cap_info **/
int create_invocation_cap(struct spd_info *from_spd, struct service_symbs *from_obj, 
			  struct spd_info *to_spd, struct service_symbs *to_obj,
			  int cos_fd, char *client_fn, char *client_stub, 
			  char *server_stub, char *server_fn, int flags)
{
	struct cap_info cap;
	struct symb_type *st = &from_obj->undef;

	vaddr_t addr;
	int i;
	
	/* find in what position the symbol was inserted into the
	 * user-level capability table (which position was opted for
	 * use), so that we can insert the information into the
	 * correct user-capability. */
	for (i = 0 ; i < st->num_symbs ; i++) {
		if (strcmp(client_fn, st->symbs[i].name) == 0) {
			break;
		}
	}
	if (i == st->num_symbs) {
		printl(PRINT_DEBUG, "Could not find the undefined symbol %s in %s.\n", 
		       server_fn, from_obj->obj);
		exit(-1);
	}
	
	addr = (vaddr_t)get_symb_address(&to_obj->exported, server_stub);
	if (addr == 0) {
		printl(PRINT_DEBUG, "Could not find %s in %s.\n", server_stub, to_obj->obj);
		exit(-1);
	}
	cap.SD_serv_stub = addr;
	addr = (vaddr_t)get_symb_address(&from_obj->exported, client_stub);
	if (addr == 0) {
		printl(PRINT_DEBUG, "could not find %s in %s.\n", client_stub, from_obj->obj);
		exit(-1);
	}
	cap.SD_cli_stub = addr;
	addr = (vaddr_t)get_symb_address(&to_obj->exported, server_fn);
	if (addr == 0) {
		printl(PRINT_DEBUG, "could not find %s in %s.\n", server_fn, to_obj->obj);
		exit(-1);
	}
	cap.ST_serv_entry = addr;
	
	cap.rel_offset = i;
	cap.owner_spd_handle = from_spd->spd_handle;
	cap.dest_spd_handle = to_spd->spd_handle;
	cap.il = 3;
	cap.flags = flags;

	cap.cap_handle = cos_spd_add_cap(cos_fd, &cap);

 	if (cap.cap_handle == 0) {
		printl(PRINT_DEBUG, "Could not add capability # %d to %s (%d) for %s.\n", 
		       cap.rel_offset, from_obj->obj, cap.owner_spd_handle, server_fn);
		exit(-1);
	}
	
	return 0;
}

static struct symb *spd_contains_symb(struct service_symbs *s, char *name) 
{
	int i;
	struct symb_type *symbs = &s->exported; 

	for (i = 0 ; i < symbs->num_symbs ; i++) {
		if (strcmp(name, symbs->symbs[i].name) == 0) {
			return &symbs->symbs[i];
		}
	}
	return NULL;
}

struct cap_ret_info {
	struct symb *csymb, *ssymbfn, *cstub, *sstub;
	struct service_symbs *serv;
	u32_t fault_handler;
};

static int cap_get_info(struct service_symbs *service, struct cap_ret_info *cri, struct symb *symb)
{
	struct symb *exp_symb = symb->exported_symb;
	struct service_symbs *exporter = symb->exporter;
	struct symb *c_stub, *s_stub;
	char tmp[MAX_SYMB_LEN];

	assert(exporter);
	memset(cri, 0, sizeof(struct cap_ret_info));

	if (MAX_SYMB_LEN-1 == snprintf(tmp, MAX_SYMB_LEN-1, "%s%s", symb->name, CAP_CLIENT_STUB_POSTPEND)) {
		printl(PRINT_HIGH, "symbol name %s too long to become client capability\n", symb->name);
		return -1;
	}
	c_stub = spd_contains_symb(service, tmp);
	if (NULL == c_stub) {
		c_stub = spd_contains_symb(service, CAP_CLIENT_STUB_DEFAULT);
		if (NULL == c_stub) {
			printl(PRINT_HIGH, "Could not find a client stub for function %s in service %s.\n",
			       symb->name, service->obj);
			return -1;
		}
	}

	if (MAX_SYMB_LEN-1 == snprintf(tmp, MAX_SYMB_LEN-1, "%s%s", exp_symb->name, CAP_SERVER_STUB_POSTPEND)) {
		printl(PRINT_HIGH, "symbol name %s too long to become server capability\n", exp_symb->name);
		return -1;
	}

	s_stub = spd_contains_symb(exporter, tmp);
	if (NULL == s_stub) {
		printl(PRINT_HIGH, "Could not find server stub (%s) for function %s in service %s to satisfy %s.\n",
		       tmp, exp_symb->name, exporter->obj, service->obj);
		return -1;
	}

//	printf("spd %s: symb %s, exp %s\n", exporter->obj, symb->name, exp_symb->name);

	cri->csymb = symb;
	cri->ssymbfn = exp_symb;
	cri->cstub = c_stub;
	cri->sstub = s_stub;
	cri->serv = exporter;
	cri->fault_handler = (u32_t)fault_handler_num(exp_symb->name);

	return 0;
}

static int create_spd_capabilities(struct service_symbs *service/*, struct spd_info *si*/, int cntl_fd)
{
	int i;
	struct symb_type *undef_symbs = &service->undef;
	struct spd_info *spd = (struct spd_info*)service->extern_info;
	
	assert(!service->is_composite_loaded);
	for (i = 0 ; i < undef_symbs->num_symbs ; i++) {
		struct symb *symb = &undef_symbs->symbs[i];
		struct cap_ret_info cri;

		if (cap_get_info(service, &cri, symb)) return -1;
		assert(!cri.serv->is_composite_loaded);
		if (create_invocation_cap(spd, service, cri.serv->extern_info, cri.serv, cntl_fd, 
					  cri.csymb->name, cri.cstub->name, cri.sstub->name, 
					  cri.ssymbfn->name, 0)) {
			return -1;
		}
	}
	
	return 0;
}

struct spd_info *create_spd(int cos_fd, struct service_symbs *s, 
			    long lowest_addr, long size) 
{
	struct spd_info *spd;
	struct usr_inv_cap *ucap_tbl;
	vaddr_t upcall_addr;
	long *spd_id_addr, *heap_ptr;
	struct cos_component_information *ci;
	int i;

	assert(!s->is_composite_loaded);

	spd = (struct spd_info *)malloc(sizeof(struct spd_info));
	if (NULL == spd) {
		perror("Could not allocate memory for spd\n");
		return NULL;
	}

	ci = (void*)get_symb_address(&s->exported, COMP_INFO);
	if (ci == NULL) {
		printl(PRINT_DEBUG, "Could not find %s in %s.\n", COMP_INFO, s->obj);
		return NULL;
	}
	upcall_addr = ci->cos_upcall_entry;
	spd_id_addr = (long*)&ci->cos_this_spd_id;
	heap_ptr    = (long*)&ci->cos_heap_ptr;
	ucap_tbl    = (struct usr_inv_cap*)ci->cos_user_caps;
	
	for (i = 0 ; i < NUM_ATOMIC_SYMBS ; i++) {
		if (i % 2 == 0) {
			spd->atomic_regions[i] = ci->cos_ras[i/2].start;
		} else {
			spd->atomic_regions[i] = ci->cos_ras[i/2].end;
		}
	}
	
	spd->num_caps = s->undef.num_symbs;
	spd->ucap_tbl = (vaddr_t)ucap_tbl;
	spd->lowest_addr = lowest_addr;
	spd->size = size;
	spd->upcall_entry = upcall_addr;

	spdid_inc++;
	spd->spd_handle = cos_create_spd(cos_fd, spd);
	assert(spdid_inc == spd->spd_handle);
	if (spd->spd_handle < 0) {
		printl(PRINT_DEBUG, "Could not create spd %s\n", s->obj);
		free(spd);
		return NULL;
	}
	printl(PRINT_HIGH, "spd %s, id %d with initialization string \"%s\" @ %x.\n", 
	       s->obj, (unsigned int)spd->spd_handle, s->init_str, (unsigned int)spd->lowest_addr);
	*spd_id_addr = spd->spd_handle;
	printl(PRINT_DEBUG, "\tHeap pointer directed to %x.\n", (unsigned int)s->heap_top);
	*heap_ptr = s->heap_top;

	printl(PRINT_DEBUG, "\tFound ucap_tbl for component %s @ %p.\n", s->obj, ucap_tbl);
	printl(PRINT_DEBUG, "\tFound cos_upcall for component %s @ %p.\n", s->obj, (void*)upcall_addr);
	printl(PRINT_DEBUG, "\tFound spd_id address for component %s @ %p.\n", s->obj, spd_id_addr);
	for (i = 0 ; i < NUM_ATOMIC_SYMBS ; i++) {
		printl(PRINT_DEBUG, "\tFound %s address for component %s @ %x.\n", 
		       ATOMIC_USER_DEF[i], s->obj, (unsigned int)spd->atomic_regions[i]);
	}

	s->extern_info = spd;

	return spd;
}

void make_spd_scheduler(int cntl_fd, struct service_symbs *s, struct service_symbs *p)
{
	vaddr_t sched_page;
	struct spd_info *spd = s->extern_info, *parent = NULL;
	struct cos_component_information *ci;

	if (p) parent = p->extern_info;

	ci = (struct cos_component_information*)get_symb_address(&s->exported, COMP_INFO);
	sched_page = (vaddr_t)ci->cos_sched_data_area;

	printl(PRINT_DEBUG, "Found spd notification page @ %x.  Promoting to scheduler.\n", 
	       (unsigned int) sched_page);

	cos_promote_to_scheduler(cntl_fd, spd->spd_handle, (NULL == parent)? -1 : parent->spd_handle, sched_page);

	return;
}

/* Edge description of components.  Mirrored in mpd_mgr.c */
struct comp_graph {
	int client, server;
};

static int service_get_spdid(struct service_symbs *ss)
{
	return (ss->is_composite_loaded) ? 
		(int)ss->cobj->id :
		((struct spd_info*)ss->extern_info)->spd_handle;
}

static int serialize_spd_graph(struct comp_graph *g, int sz, struct service_symbs *ss)
{
	struct comp_graph *edge;
	int g_frontier = 0;

	while (ss) {
		int i, cid, sid;

		if (ss->is_composite_loaded) {
			ss = ss->next;
			continue;
		}

		assert(ss->extern_info);		
		cid = service_get_spdid(ss);
		for (i = 0 ; i < ss->num_dependencies && 0 != cid ; i++) {
			struct service_symbs *dep = ss->dependencies[i].dep;
			assert(dep);
			
			sid = service_get_spdid(dep);
			if (sid == 0) continue;
			if (g_frontier >= (sz-2)) {
				printl(PRINT_DEBUG, "More edges in component graph than can be serialized into the allocated region: fix cos_loader.c.\n");
				exit(-1);
			}

			edge = &g[g_frontier++];
			edge->client = cid;
			edge->server = sid;
			//printl(PRINT_DEBUG, "serialized edge @ %p: %d->%d.\n", edge, cid, sid);
		}
		
		ss = ss->next;
	}
	edge = &g[g_frontier];
	edge->client = edge->server = 0;

	return 0;
}

int **get_heap_ptr(struct service_symbs *ss)
{
	struct cos_component_information *ci;

	ci = (struct cos_component_information *)get_symb_address(&ss->exported, COMP_INFO);
	if (ci == NULL) {
		printl(PRINT_DEBUG, "Could not find component information struct in %s.\n", ss->obj);
		exit(-1);
	}
	return (int**)&(ci->cos_heap_ptr);
}

/* 
 * The only thing we need to do to the mpd manager is to let it know
 * the topology of the component graph.  Progress the heap pointer a
 * page, and serialize the component graph into that page.
 */
static void make_spd_mpd_mgr(struct service_symbs *mm, struct service_symbs *all)
{
	int **heap_ptr, *heap_ptr_val;
	struct comp_graph *g;

	if (mm->is_composite_loaded) {
		printl(PRINT_HIGH, "Cannot load %s via composite (%s).\n", MPD_MGR, BOOT_COMP);
		return;
	}
	heap_ptr = get_heap_ptr(mm);
	if (heap_ptr == NULL) {
		printl(PRINT_DEBUG, "Could not find heap pointer in %s.\n", mm->obj);
		return;
	}
	heap_ptr_val = *heap_ptr;
	g = mmap((void*)heap_ptr_val, PAGE_SIZE, PROT_WRITE | PROT_READ,
			MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (MAP_FAILED == g){
		perror("Couldn't map the graph into the address space");
		return;
	}
	printl(PRINT_DEBUG, "Found mpd_mgr: remapping heap_ptr from %p to %p, serializing graph.\n",
	       *heap_ptr, heap_ptr_val + PAGE_SIZE/sizeof(heap_ptr_val));
	*heap_ptr = heap_ptr_val + PAGE_SIZE/sizeof(heap_ptr_val);

	serialize_spd_graph(g, PAGE_SIZE/sizeof(struct comp_graph), all);
}

static void make_spd_init_file(struct service_symbs *ic, const char *fname)
{
	int fd = open(fname, O_RDWR);
	struct stat b;
	int real_sz, sz, ret;
	int **heap_ptr, *heap_ptr_val;
	int *start;
	struct cos_component_information *ci;

	if (fd == -1) {
		printl(PRINT_HIGH, "Init file component specified, but file %s not found\n", fname);
		perror("Error");
		exit(-1);
	}
	if (fstat(fd, &b)) {
		printl(PRINT_HIGH, "Init file component specified, but error stating file %s not found\n", fname);
		perror("Error");
		exit(-1);
	}
	real_sz = b.st_size;
	sz = round_up_to_page(real_sz);

	if (ic->is_composite_loaded) {
		printl(PRINT_HIGH, "Cannot load %s via composite (%s).\n", INIT_FILE, BOOT_COMP);
		return;
	}
	heap_ptr = get_heap_ptr(ic);
	if (heap_ptr == NULL) {
		printl(PRINT_HIGH, "Could not find heap pointer in %s.\n", ic->obj);
		return;
	}
	heap_ptr_val = *heap_ptr;
	ci = (void *)get_symb_address(&ic->exported, COMP_INFO);
	if (!ci) {
		printl(PRINT_HIGH, "Could not find component information in %s.\n", ic->obj);
		return;
	}
	ci->cos_poly[0] = (vaddr_t)heap_ptr_val;

	start = mmap((void*)heap_ptr_val, sz, PROT_WRITE | PROT_READ, 
		     MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	ret = read(fd, start, real_sz);
	if (real_sz != ret) {
		printl(PRINT_HIGH, "Reading in init file %s: could not retrieve whole file\n", fname);
		perror("error: ");
		exit(-1);
	}
	if (MAP_FAILED == start){
		printl(PRINT_HIGH, "Couldn't map the init file, %s, into address space", fname);
		perror("error:");
		exit(-1);
	}
	printl(PRINT_HIGH, "Found init file component: remapping heap_ptr from %p to %p, mapping in file.\n",
	       *heap_ptr, heap_ptr_val);
	*heap_ptr = (int*)((char*)heap_ptr_val + sz);
	ci->cos_poly[1] = real_sz;
}

static int make_cobj_symbols(struct service_symbs *s, struct cobj_header *h)
{
	u32_t addr;
	u32_t symb_offset = 0;
	int i;

	struct name_type_map { 
		const char *name; 
		u32_t type;
	};
	struct name_type_map map[] = {
		{.name = COMP_INFO, .type = COBJ_SYMB_COMP_INFO},
		{.name = NULL, .type = 0} 
	};

	/* Create the sumbols */
	printl(PRINT_DEBUG, "%s loaded by Composite -- Symbols:\n", s->obj);
	for (i = 0 ; map[i].name != NULL ; i++) {
		addr = (u32_t)get_symb_address(&s->exported, map[i].name);
		printl(PRINT_DEBUG, "\taddr %x, nsymb %d\n", addr, i);
		if (addr && cobj_symb_init(h, symb_offset++, map[i].type, addr)) {
			printl(PRINT_HIGH, "boot component: couldn't create cobj symb for %s (%d).\n", map[i].name, i);
			return -1;
		}
	}

	return 0;
}

static int make_cobj_caps(struct service_symbs *s, struct cobj_header *h)
{
	int i;
	struct symb_type *undef_symbs = &s->undef;
	
	printl(PRINT_DEBUG, "%s loaded by Composite -- Capabilities:\n", s->obj);
	for (i = 0 ; i < undef_symbs->num_symbs ; i++) {
		u32_t cap_off, dest_id, sfn, cstub, sstub, fault;
		struct symb *symb = &undef_symbs->symbs[i];
		struct cap_ret_info cri;

		if (cap_get_info(s, &cri, symb)) return -1;

		cap_off = i;
		dest_id = service_get_spdid(cri.serv);
		sfn     = cri.ssymbfn->addr;
		cstub   = cri.cstub->addr;
		sstub   = cri.sstub->addr;
		fault   = cri.fault_handler;

		printl(PRINT_DEBUG, "\tcap %d, off %d, sfn %x, cstub %x, sstub %x\n", 
		       i, cap_off, sfn, cstub, sstub);

		if (cobj_cap_init(h, cap_off, cap_off, dest_id, sfn, cstub, sstub, fault)) return -1;

		printl(PRINT_DEBUG, "capability from %s:%d to %s:%d\n", s->obj, s->cobj->id, cri.serv->obj, dest_id);
	}
	
	return 0;
}

static struct service_symbs *find_obj_by_name(struct service_symbs *s, const char *n);

//#define ROUND_UP_TO_PAGE(a) (((vaddr_t)(a)+PAGE_SIZE-1) & ~(PAGE_SIZE-1))
//#deinfe ROUND_UP_TO_CACHELINE(a) (((vaddr_t)(a)+CACHE_LINE-1) & ~(CACHE_LINE-1))

static void make_spd_boot(struct service_symbs *boot, struct service_symbs *all)
{
	int **heap_ptr, *heap_ptr_val, n = 0;
	struct cobj_header *h;
	char *mem;
	u32_t obj_size;
	struct cos_component_information *ci;
	struct service_symbs *first = all;

	/* Assign ids to the booter-loaded components. */
	for (all = first ; NULL != all ; all = all->next) {
		if (!all->is_composite_loaded) continue;

		h = all->cobj;
		assert(h);
		spdid_inc++;
		h->id = spdid_inc;
	}

	/* Setup the capabilities for each of the booter-loaded
	 * components */
	all = first;
	for (all = first ; NULL != all ; all = all->next) {
		if (!all->is_composite_loaded) continue;

		if (make_cobj_caps(all, all->cobj)) {
			printl(PRINT_HIGH, "Could not create capabilities in cobj for %s\n", all->obj);
			exit(-1);
		}
	}

	heap_ptr = get_heap_ptr(boot);
	ci = (void *)get_symb_address(&boot->exported, COMP_INFO);
	ci->cos_poly[0] = (vaddr_t)*heap_ptr;
	for (all = first ; NULL != all ; all = all->next) {
		vaddr_t map_addr;
		int map_sz;

		if (!all->is_composite_loaded) continue;
		n++;

		heap_ptr_val = *heap_ptr;
		assert(all->is_composite_loaded);
		h = all->cobj;
		assert(h);

		obj_size = round_up_to_cacheline(h->size);
		map_addr = round_up_to_page(heap_ptr_val);
		map_sz = (int)obj_size - (int)(map_addr-(vaddr_t)heap_ptr_val);
		if (map_sz > 0) {
			mem = mmap((void*)map_addr, map_sz, PROT_WRITE | PROT_READ,
				   MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
			if (MAP_FAILED == mem) {
				perror("Couldn't map test component into the boot component");
				exit(-1);
			}
		}
		printl(PRINT_HIGH, "boot component: placing %s:%d @ %p, copied from %p:%d\n", 
		       all->obj, service_get_spdid(all), heap_ptr_val, h, obj_size);
		memcpy(heap_ptr_val, h, h->size);
		*heap_ptr = (void*)(((int)heap_ptr_val) + obj_size);
	}
	*heap_ptr = (int*)(round_up_to_page((int)*heap_ptr));
	ci->cos_poly[1] = (vaddr_t)n;
}

//#define INIT_STR_SZ 116
#define INIT_STR_SZ 52

/* struct is 64 bytes, so we can have 64 entries in a page. */
struct component_init_str {
	unsigned int spdid, schedid;
	int startup;
	char init_str[INIT_STR_SZ];
}__attribute__((packed));

static void format_config_info(struct service_symbs *ss, struct component_init_str *data)
{
	int i; 

	for (i = 0 ; ss ; i++, ss = ss->next) {
		char *info;

		info = ss->init_str;
		if (strlen(info) >= INIT_STR_SZ) {
			printl(PRINT_HIGH, "Initialization string %s for component %s is too long (longer than %d)",
			       info, ss->obj, strlen(info));
			exit(-1);
		}

		if (ss->is_composite_loaded) {
			data[i].startup = 0;
			data[i].spdid = ss->cobj->id;
		} else {
			data[i].startup = 1;
			data[i].spdid = ((struct spd_info *)(ss->extern_info))->spd_handle;
		}
		data[i].schedid = ss->scheduler ? 
			((struct spd_info *)(ss->scheduler->extern_info))->spd_handle : 
			0;

		if (0 == strcmp(" ", info)) info = "";
		strcpy(data[i].init_str, info);
	}
	data[i].spdid = 0;
}

static void make_spd_config_comp(struct service_symbs *c, struct service_symbs *all)
{
	int **heap_ptr, *heap_ptr_val;
	struct component_init_str *info;

	heap_ptr = get_heap_ptr(c);
	if (heap_ptr == NULL) {
		printl(PRINT_DEBUG, "Could not find cos_heap_ptr in %s.\n", c->obj);
		return;
	}
	heap_ptr_val = *heap_ptr;
	info = mmap((void*)heap_ptr_val, PAGE_SIZE, PROT_WRITE | PROT_READ,
			MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (MAP_FAILED == info){
		perror("Couldn't map the configuration info into the address space");
		return;
	}
	printl(PRINT_DEBUG, "Found %s: remapping heap_ptr from %p to %p, writing config info.\n",
	       CONFIG_COMP, *heap_ptr, heap_ptr_val + PAGE_SIZE/sizeof(heap_ptr_val));
	*heap_ptr = heap_ptr_val + PAGE_SIZE/sizeof(heap_ptr_val);

	format_config_info(all, info);
}

static struct service_symbs *find_obj_by_name(struct service_symbs *s, const char *n)
{
	while (s) {
		if (strstr(s->obj, n) != NULL) {
			return s;
		}

		s = s->next;
	}

	return NULL;
}

#define MAX_SCHEDULERS 3

static void setup_kernel(struct service_symbs *services)
{
	struct service_symbs *s;
	struct service_symbs *init = NULL;
	struct spd_info *init_spd = NULL;

	struct cos_thread_info thd;
	int cntl_fd, ret;
	int (*fn)(void);
	unsigned long long start, end;
	
	cntl_fd = aed_open_cntl_fd();

	s = services;
	while (s) {
		struct service_symbs *t;
		struct spd_info *t_spd;

		t = s;
		if (!s->is_composite_loaded) {
			if (strstr(s->obj, INIT_COMP) != NULL) {
				init = t;
				t_spd = init_spd = create_spd(cntl_fd, init, 0, 0);
			} else {
				t_spd = create_spd(cntl_fd, t, t->lower_addr, t->size);
			}
			if (!t_spd) {
				fprintf(stderr, "\tCould not find service object.\n");
				exit(-1);
			}
		}
		s = s->next;
	}

	s = services;
	while (s) {
		if (!s->is_composite_loaded) {
			if (create_spd_capabilities(s, cntl_fd)) {
				fprintf(stderr, "\tCould not find all stubs.\n");
				exit(-1);
			}
		} 

		s = s->next;
	}
	printl(PRINT_DEBUG, "\n");

	if ((s = find_obj_by_name(services, ROOT_SCHED)) == NULL) {
		fprintf(stderr, "Could not find root scheduler %s\n", ROOT_SCHED);
		exit(-1);
	}
	make_spd_scheduler(cntl_fd, s, NULL);
	assert(!s->is_composite_loaded);
	thd.sched_handle = ((struct spd_info *)s->extern_info)->spd_handle;

	if ((s = find_obj_by_name(services, BOOT_COMP))) {
		make_spd_boot(s, services);
	} else if ((s = find_obj_by_name(services, BOOT_COMP2))) {
		make_spd_boot(s, services);
	}

	fflush(stdout);
	if ((s = find_obj_by_name(services, MPD_MGR))) {
		make_spd_mpd_mgr(s, services);
	}
	fflush(stdout);

	if ((s = find_obj_by_name(services, INIT_FILE))) {
		make_spd_init_file(s, INIT_FILE_NAME);
	}
	fflush(stdout);

	if ((s = find_obj_by_name(services, CONFIG_COMP)) == NULL) {
		fprintf(stderr, "Could not find the configuration component.\n");
		exit(-1);
	}
	make_spd_config_comp(s, services);

	if ((s = find_obj_by_name(services, INIT_COMP)) == NULL) {
		fprintf(stderr, "Could not find initial component\n");
		exit(-1);
	}
	thd.spd_handle = ((struct spd_info *)s->extern_info)->spd_handle;//spd0->spd_handle;
	cos_create_thd(cntl_fd, &thd);

	printl(PRINT_HIGH, "\nOK, good to go, calling component 0's main\n\n");
	fflush(stdout);

	fn = (int (*)(void))get_symb_address(&s->exported, "spd0_main");

#define ITER 1
#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

	/* This will hopefully avoid hugely annoying fsck runs */
	sync();

	aed_disable_syscalls(cntl_fd);
	rdtscll(start);
	ret = fn();
	rdtscll(end);
	aed_enable_syscalls(cntl_fd);

	printl(PRINT_HIGH, "Invocation takes %lld, ret %x.\n", (end-start)/ITER, ret);
	
	close(cntl_fd);

	return;
}

static inline void print_usage(int argc, char **argv)
{
	char *prog_name = argv[0];
	int i;

	printl(PRINT_HIGH, "Usage: %s <comma separated string of all "
	       "objs:truster1-trustee1|trustee2|...;truster2-...> "
	       "<path to gen_client_stub>\n",
	       prog_name);

	printl(PRINT_HIGH, "\nYou gave:");
	for (i = 0 ; i < argc ; i++) {
		printl(PRINT_HIGH, " %s", argv[i]);
	}
	printl(PRINT_HIGH, "\n");
	
	return;
}

#define STUB_PROG_LEN 128
extern vaddr_t SS_ipc_client_marshal;
extern vaddr_t DS_ipc_client_marshal;

//#define FAULT_SIGNAL
#include <sys/ucontext.h>
#ifdef FAULT_SIGNAL
void segv_handler(int signo, siginfo_t *si, void *context) {
	ucontext_t *uc = context;
	struct sigcontext *sc = (struct sigcontext *)&uc->uc_mcontext;

	printl(PRINT_HIGH, "Segfault: Faulting address %p, ip: %lx\n", si->si_addr, sc->eip);
	exit(-1);
}
#endif
//#define ALRM_SIGNAL
#ifdef ALRM_SIGNAL
void alrm_handler(int signo, siginfo_t *si, void *context) {
	printl(PRINT_HIGH, "Alarm! Time to exit!\n");
	exit(-1);
}
#endif


#include <sched.h>
#include <sys/time.h>
#include <sys/resource.h>
static void call_getrlimit(int id, char *name)
{
	struct rlimit rl;

	if (getrlimit(id, &rl)) {
		perror("getrlimit: "); printl(PRINT_HIGH, "\n");
		exit(-1);
	}		
	printl(PRINT_HIGH, "rlimit for %s is %d:%d (inf %d)\n", 
	       name, (int)rl.rlim_cur, (int)rl.rlim_max, (int)RLIM_INFINITY);
}

static void call_setrlimit(int id, rlim_t c, rlim_t m)
{
	struct rlimit rl;

	rl.rlim_cur = c;
	rl.rlim_max = m;
	if (setrlimit(id, &rl)) {
		perror("getrlimit: "); printl(PRINT_HIGH, "\n");
		exit(-1);
	}		
}

void set_prio(void)
{
	struct sched_param sp;

	call_getrlimit(RLIMIT_CPU, "CPU");
#ifdef RLIMIT_RTTIME
	call_getrlimit(RLIMIT_RTTIME, "RTTIME");
#endif
	call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
	call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
	call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");	
	call_getrlimit(RLIMIT_NICE, "NICE");

	if (sched_getparam(0, &sp) < 0) {
		perror("getparam: ");
		printl(PRINT_HIGH, "\n");
	}
	sp.sched_priority = sched_get_priority_max(SCHED_RR);
	if (sched_setscheduler(0, SCHED_RR, &sp) < 0) {
		perror("setscheduler: "); printl(PRINT_HIGH, "\n");
		exit(-1);
	}
	if (sched_getparam(0, &sp) < 0) {
		perror("getparam: ");
		printl(PRINT_HIGH, "\n");
	}
	assert(sp.sched_priority == sched_get_priority_max(SCHED_RR));

	return;
}

void setup_thread(void)
{
#ifdef FAULT_SIGNAL
	struct sigaction sa;

	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &sa, NULL);
#endif
#ifdef HIGHEST_PRIO
	set_prio();
#endif
#ifdef ALRM_SIGNAL
	//printf("pid %d\n", getpid()); getchar();
	{
		struct sigaction saa;

		saa.sa_sigaction = alrm_handler;
		saa.sa_flags = SA_SIGINFO;
		sigaction(SIGALRM, &saa, NULL);
		alarm(30);
	}
	while (1) ;
#endif

}

/*
 * Format of the input string is as such:
 * 
 * "s1,s2,s3,...,sn:s2-s3|...|sm;s3-si|...|sj"
 *
 * Where the pre-: comma-separated list is simply a list of all
 * involved services.  Post-: is a list of truster (before -) ->
 * trustee (all trustees separated by '|'s) relations separated by
 * ';'s.
 */
int main(int argc, char *argv[])
{
	struct service_symbs *services;
	char *delim = ":";
	char *servs, *dependencies, *stub_gen_prog;
	int ret = -1;

	setup_thread();

	printl(PRINT_DEBUG, "Thread scheduling parameters setup\n");

	if (argc != 3) {
		print_usage(argc, argv);
		goto exit;
	}

	stub_gen_prog = argv[2];

	/* 
	 * NOTE: because strtok is used in prepare_service_symbs, we
	 * cannot use it relating to the command line args before AND
	 * after that invocation
	 */
	if (!strstr(argv[1], delim)) {
	printl(PRINT_HIGH, "No %s separating the component list from the dependencies\n", delim);
		goto exit;
	}

	servs = strtok(argv[1], delim);
	dependencies = strtok(NULL, delim);

	if (!servs) {
		print_usage(argc, argv);
		goto exit;
	}

	bfd_init();

	services = prepare_service_symbs(servs);

	print_objs_symbs(services);

//	printl(PRINT_DEBUG, "Loading at %x:%d.\n", BASE_SERVICE_ADDRESS, DEFAULT_SERVICE_SIZE);

	if (!dependencies) {
		printl(PRINT_HIGH, "No dependencies given, not proceeding.\n");
		goto dealloc_exit;
	}
	
	if (deserialize_dependencies(dependencies, services)) {
		printl(PRINT_HIGH, "Error processing dependencies.\n");
		goto dealloc_exit;
	}

	if (verify_dependency_completeness(services)) {
		printl(PRINT_HIGH, "Unresolved symbols, not linking.\n");
		goto dealloc_exit;
	}

	if (verify_dependency_soundness(services)) {
		printl(PRINT_HIGH, "Services arranged in an invalid configuration, not linking.\n");
		goto dealloc_exit;
	}
	
	gen_stubs_and_link(stub_gen_prog, services);
	if (load_all_services(services)) {
		printl(PRINT_HIGH, "Error loading services, aborting.\n");
		goto dealloc_exit;
	}

//	print_kern_symbs(services);

	setup_kernel(services);

	ret = 0;

 dealloc_exit:
	while (services) {
		struct service_symbs *next = services->next;
		free_service_symbs(services);
		services = next;
	}
	/* FIXME: new goto label to dealloc spds */
 exit:
	return 0;
}

