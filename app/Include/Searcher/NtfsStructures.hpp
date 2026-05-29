#pragma once

#pragma pack(push, 1)

/// @brief Low-level data structures mapping raw NTFS volume and file system layouts.
namespace NTFS
{
	// Boot Sector (Volume Boot Record)
	struct BootSector
	{
		uint8_t jump[ 3 ];                 // 0x00
		char oem_id[ 8 ];                  // 0x03
		uint16_t bytes_per_sector;         // 0x0B
		uint8_t sectors_per_cluster;       // 0x0D
		uint16_t reserved_sectors;         // 0x0E
		uint8_t unused1[ 3 ];              // 0x10
		uint16_t unused2;                  // 0x13
		uint8_t media_descriptor;          // 0x15
		uint16_t unused3;                  // 0x16
		uint16_t sectors_per_track;        // 0x18
		uint16_t number_of_heads;          // 0x1A
		uint32_t hidden_sectors;           // 0x1C
		uint32_t unused4;                  // 0x20
		uint32_t unused5;                  // 0x24
		uint64_t total_sectors;            // 0x28
		uint64_t mft_logical_cluster_number; // 0x30
		uint64_t mft_mirror_logical_cluster_number; // 0x38
		int8_t clusters_per_mft_record;    // 0x40
		uint8_t unused6[ 3 ];              // 0x41
		int8_t clusters_per_index_buffer;  // 0x44
		uint8_t unused7[ 3 ];              // 0x45
		uint64_t volume_serial_number;     // 0x48
		uint32_t checksum;                 // 0x50
		uint8_t bootstrap_code[ 426 ];     // 0x54
		uint16_t end_of_sector_marker;     // 0x1FE
	};

	// MFT Record Header
	struct MftRecordHeader
	{
		uint32_t magic; // "FILE"
		uint16_t update_sequence_offset;
		uint16_t update_sequence_size;
		uint64_t log_sequence_number;
		uint16_t sequence_number;
		uint16_t hard_link_count;
		uint16_t first_attribute_offset;
		uint16_t flags; // 0x01 = In Use, 0x02 = Directory
		uint32_t real_size;
		uint32_t allocated_size;
		uint64_t base_reference;
		uint16_t next_attribute_id;
		uint16_t align;
		uint32_t record_number;
	};

	// Generic Attribute Header
	struct AttributeHeader
	{
		uint32_t type; // 0x10 = Standard Info, 0x30 = File Name, 0x80 = Data
		uint32_t length;
		uint8_t non_resident;
		uint8_t name_length;
		uint16_t name_offset;
		uint16_t flags;
		uint16_t attribute_id;
	};

	// Resident Attribute Header
	struct ResidentAttributeHeader
	{
		AttributeHeader header;
		uint32_t value_length;
		uint16_t value_offset;
		uint16_t flags;
	};

	// Non-Resident Attribute Header
	struct NonResidentAttributeHeader
	{
		AttributeHeader header;
		uint64_t starting_vcn;
		uint64_t last_vcn;
		uint16_t data_runs_offset;
		uint16_t compression_unit_size;
		uint32_t padding;
		uint64_t allocated_size;
		uint64_t real_size;
		uint64_t initialized_size;
	};

	// File Name Attribute (0x30)
	struct FileNameAttribute
	{
		uint64_t parent_directory; // Lower 48 bits = MFT Record, Upper 16 bits = Sequence Number
		uint64_t creation_time;
		uint64_t file_altered_time;
		uint64_t mft_altered_time;
		uint64_t file_accessed_time;
		uint64_t allocated_size;
		uint64_t real_size;
		uint32_t flags;
		uint32_t reparse_value;
		uint8_t name_length;
		uint8_t name_type; // 0 = POSIX, 1 = Win32, 2 = DOS, 3 = Win32/DOS
		wchar_t name[ 1 ]; // Variable length
	};
}

#pragma pack(pop)
