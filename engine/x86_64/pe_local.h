#define PE_DOS_SIGNATURE				0x5A4D
#define PE_NT_SIGNATURE					0x00004550

#define PE_NT_OPTIONAL_HDR64_MAGIC		0x20b
#define PE_NT_OPTIONAL_HDR_MAGIC		PE_NT_OPTIONAL_HDR64_MAGIC

#define PE_FILE_RELOCS_STRIPPED			1

#define PE_DIRECTORY_ENTRY_EXPORT		0
#define PE_DIRECTORY_ENTRY_BASERELOC	5

#define PE_REL_BASED_ABSOLUTE			0
#define PE_REL_BASED_HIGHLOW			3
#define PE_REL_BASED_DIR64				10

#define PE_SCN_CNT_CODE					0x00000020
#define PE_SCN_MEM_EXECUTE				0x20000000
#define PE_SCN_MEM_READ					0x40000000
#define PE_SCN_MEM_WRITE				0x80000000

typedef struct {
	uint32_t				Characteristics;
	uint32_t				TimeDateStamp;
	uint16_t				MajorVersion;
	uint16_t				MinorVersion;
	uint32_t				Name;
	uint32_t				Base;
	uint32_t				NumberOfFunctions;
	uint32_t				NumberOfNames;
	uint32_t				AddressOfFunctions;
	uint32_t				AddressOfNames;
	uint32_t				AddressOfNameOrdinals;
} PE_EXPORT_DIRECTORY;

typedef struct {
	uint16_t				e_magic;           
	uint16_t				e_cblp;            
	uint16_t				e_cp;              
	uint16_t				e_crlc;            
	uint16_t				e_cparhdr;         
	uint16_t				e_minalloc;        
	uint16_t				e_maxalloc;        
	uint16_t				e_ss;              
	uint16_t				e_sp;              
	uint16_t				e_csum;            
	uint16_t				e_ip;              
	uint16_t				e_cs;              
	uint16_t				e_lfarlc;          
	uint16_t				e_ovno;            
	uint16_t				e_res[4];          
	uint16_t				e_oemid;           
	uint16_t				e_oeminfo;         
	uint16_t				e_res2[10];        
	int32_t 				e_lfanew;           
} PE_DOS_HEADER;

typedef struct {
	union {
		uint32_t			Characteristics;      
		uint32_t			OriginalFirstThunk;   
#if defined( _MSC_VER )
	} _;
#else
	};
#endif
	uint32_t				TimeDateStamp;                                                    
	uint32_t				ForwarderChain;           
	uint32_t				Name;
	uint32_t				FirstThunk;               
} PE_IMPORT_DESCRIPTOR;

typedef struct {
	uint32_t				VirtualAddress;
	uint32_t				SizeOfBlock;
	uint16_t				Fixups[0];
} PE_BASE_RELOCATION;

typedef struct {
	uint8_t					Name[8];
	uint32_t				VirtualSize;
	uint32_t				VirtualAddress;
	uint32_t				SizeOfRawData;
	uint32_t				PointerToRawData;
	uint32_t				PointerToRelocations;
	uint32_t				PointerToLinenumbers;
	uint16_t				NumberOfRelocations;
	uint16_t				NumberOfLinenumbers;
	uint32_t				Characteristics;
} PE_SECTION_HEADER;

typedef struct {
	uint16_t				Machine;
	uint16_t				NumberOfSections;
	uint32_t				TimeDateStamp;
	uint32_t				PointerToSymbolTable;
	uint32_t				NumberOfSymbols;
	uint16_t				SizeOfOptionalHeader;
	uint16_t				Characteristics;
} PE_FILE_HEADER;

typedef struct {
	uint32_t				VirtualAddress;
	uint32_t				Size;
} PE_DATA_DIRECTORY;

typedef struct {
	uint16_t				Magic;
	uint8_t					MajorLinkerVersion;
	uint8_t					MinorLinkerVersion;
	uint32_t				SizeOfCode;
	uint32_t				SizeOfInitializedData;
	uint32_t				SizeOfUninitializedData;
	uint32_t				AddressOfEntryPoint;
	uint32_t				BaseOfCode;
	uintptr_t				ImageBase;
	uint32_t				SectionAlignment;
	uint32_t				FileAlignment;
	uint16_t				MajorOperatingSystemVersion;
	uint16_t				MinorOperatingSystemVersion;
	uint16_t				MajorImageVersion;
	uint16_t				MinorImageVersion;
	uint16_t				MajorSubsystemVersion;
	uint16_t				MinorSubsystemVersion;
	uint32_t				Win32VersionValue;
	uint32_t				SizeOfImage;
	uint32_t				SizeOfHeaders;
	uint32_t				CheckSum;
	uint16_t				Subsystem;
	uint16_t				DllCharacteristics;
	uintptr_t				SizeOfStackReserve;
	uintptr_t				SizeOfStackCommit;
	uintptr_t				SizeOfHeapReserve;
	uintptr_t				SizeOfHeapCommit;
	uint32_t				LoaderFlags;
	uint32_t				NumberOfRvaAndSizes;
	PE_DATA_DIRECTORY		DataDirectory[16];
} PE_OPTIONAL_HEADER64;

typedef struct {
	uint32_t					Signature;
	PE_FILE_HEADER				FileHeader;
	PE_OPTIONAL_HEADER64		OptionalHeader;
} PE_NT_HEADERS64;

typedef PE_NT_HEADERS64			PE_NT_HEADERS;
typedef PE_OPTIONAL_HEADER64	PE_OPTIONAL_HEADER;
