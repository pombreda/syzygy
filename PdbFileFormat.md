# PDB File Format #

The purpose of this document is to describe the content of the [PDB File Format](http://msdn.microsoft.com/en-us/library/yd4f8bd1.aspx). The structure of the PDB enclosure (which is a Multi-Stream File or MSF) will not be described in detail in this document, which will focus on the contents of the streams.

## Streams with fixed index. ##

### Stream #0 : Root directory ###

This stream has information on where to find the other streams in the PDB. When present, it seems to be a copy of the previous version of the MSF directory.

### Stream #1 : PDB headers ###

Contains the PDB header and the list of named streams.

### Stream #2 : TPI (Type info) ###

[This article](http://moyix.blogspot.ca/2007/10/types-stream.html) discusses the contents of this stream. The document "Microsoft Symbol and Type Information" (see references) is really useful for understanding the contents of this stream.

### Stream #3 : DBI (Debug Info) ###

Structure :

| **Block** | **Length** | **Details** |
|:----------|:-----------|:------------|
| DBI Header | pos(sizeof(DbiHeader)) | [PdbFileFormat#DbiHeader](PdbFileFormat#DbiHeader.md) |
| Module informations | DbiHeader.gp\_modi\_size | Set of [PdbFileFormat#DbiModuleInfo](PdbFileFormat#DbiModuleInfo.md)  |
| Section contribs | DbiHeader.section\_contribution\_size | Signature (32 bits) followed by a set of [PdbFileFormat#SectionContrib](PdbFileFormat#SectionContrib.md) |
| Section map | DbiHeader.section\_map\_size | Seems to contain a table indicating the offset to apply to each address in the different section to get the corresponding RVA. <br> The structure of this substream is : <br> - Number of sections (including the reloc section). It is on 2 bytes and it is repeated twice (if there is 10 sections you'll read 0x0A000A00) <br> - Set of <a href='PdbFileFormat#DbiSectionMapItem.md'>PdbFileFormat#DbiSectionMapItem</a> structures, one for each section. The structure for the reloc section seems to have the number 0 and its field are not valid (length = 0xFFFFFFFF).<br>
<tr><td> File info </td><td> DbiHeader.file_info_size </td><td> This substream contains a list of the files used to generate each obj, the structure of it is :<br> Header | File-blocks table | Offset table | Name table.<br> - The header contain the size of the File-blocks table (on 16 bits) followed by the size of the offset table (16 bits). You have to multiply these size by 4 to obtain the size in bytes.<br> - The file block-tables is divided in 2 parts. The first half contains the starting value of each blocks (16 bits by value) and the second one contains the length of these blocks. These value refers to the offset table. There seems that there's always a last block with a starting value equal to the length of the offset table and a length of 0 at the end of this table.<br> - The offset table contains offset to the beginning of file names in the name table. These offset are relative to the beginning of the name table.<br> The example here DbiFileInfoSample illustrates this information. </td></tr>
<tr><td> TS map </td><td> DbiHeader.ts_map_size </td><td> Size = 0 on testDll, we can ignore it.</td></tr>
<tr><td> EC info </td><td> DbiHeader.ec_info_size </td><td> It's important to note that this field appear after the dbg_header_size field in the header of this stream but the EC info are located before the DbgHeader. <br> The structure of this substream is :<br> - Signature (32 bits : 0xFEEFFEEF)<br> - 32 bits field (value = 1, seems to be a version or an age) <br> - Offset to the end of the name block (32 bits) <br> - Filename block + number of entries in the name table (32 bits) <br> - name table (set of offset relative to the beginning of the filename block) <br> - value that seems to be the number of filenames in the the filename block (32 bits) but it's not always the case (I've observed a difference of +/- 1 in some cases). </td></tr>
<tr><td> DbiDbgHeader </td><td> sizeof(DbiDbgHeader) </td><td> <a href='PdbFileFormat#DbiDbgHeader.md'>PdbFileFormat#DbiDbgHeader</a> </td></tr></tbody></table>

<h4>DbiHeader</h4>
<pre><code>struct DbiHeader {<br>
  int32 signature;<br>
  uint32 version;<br>
  uint32 age;<br>
  int16 global_symbol_info_stream;<br>
  uint16 pdb_dll_version;<br>
  int16 public_symbol_info_stream;<br>
  uint16 pdb_dll_build_major;<br>
  int16 symbol_record_stream;<br>
  uint16 pdb_dll_build_minor;<br>
  uint32 gp_modi_size;<br>
  uint32 section_contribution_size;<br>
  uint32 section_map_size;<br>
  uint32 file_info_size;<br>
  uint32 ts_map_size;<br>
  uint32 mfc_index;<br>
  uint32 dbg_header_size;<br>
  uint32 ec_info_size;<br>
  uint16 flags;<br>
  uint16 machine;<br>
  uint32 reserved;<br>
};<br>
</code></pre>

<h4>DbiModuleInfo</h4>
<pre><code>struct DbiModuleInfo {<br>
  uint32 opened;<br>
  SectionContrib section;<br>
  uint16 flags;<br>
  int16 stream;<br>
  uint32 symbol_bytes;<br>
  uint32 old_lines_bytes;<br>
  uint32 lines_bytes;<br>
  int16 num_files;<br>
  uint16 padding;<br>
  uint32 offsets;<br>
  uint32 num_source;<br>
  uint32 num_compiler;<br>
  std::string module_name;<br>
  std::string object_name;<br>
};<br>
</code></pre>

<h4>SectionContrib</h4>
<pre><code>struct SectionContrib {<br>
  int16 section;<br>
  int16 pad1;<br>
  int32 offset;<br>
  int32 size;<br>
  uint32 flags;<br>
  int16 module;<br>
  int16 pad2;<br>
  uint32 data_crc;<br>
  uint32 reloc_crc;<br>
};<br>
</code></pre>

<h4>DbiDbgHeader</h4>
<pre><code>struct DbiDbgHeader {<br>
  int16 fpo;<br>
  int16 exception;<br>
  int16 fixup;<br>
  int16 omap_to_src;<br>
  int16 omap_from_src;<br>
  int16 section_header;<br>
  int16 token_rid_map;<br>
  int16 x_data;<br>
  int16 p_data;<br>
  int16 new_fpo;<br>
  int16 section_header_origin;<br>
};<br>
</code></pre>

<h4>DbiSectionMapItem</h4>
<pre><code>struct DbiSectionMapItem {<br>
  uint8 flags;<br>
  uint8 section_type;<br>
  uint8 unknown_data_1[4]; // Don't know that's in this field, it's always 0x00000000 or 0xFFFFFFFF.<br>
  uint16 section_number; // Value = 0 for the reloc section.<br>
  uint8 unknown_data_2[4]; // Same as unknown_data_1.<br>
  uint32 rva_offset; // Value added to the address offset when calculating the RVA.<br>
  uint32 section_length;<br>
};<br>
</code></pre>

<h4>DbiFileInfoSample</h4>
<pre><code>Header : <br>
03 00 05 00 // The File-blocks table have a length of 3*32bits and the offset table have a length of 5*32 bits.<br>
<br>
File-blocks table : 3*2*16 bits : <br>
00 00 // Block 1 : Starting offset<br>
03 00 // Block 2 : Starting offset<br>
06 00 // Block 3 : Starting offset<br>
03 00 // Block 1 : Length<br>
02 00 // Block 2 : Length<br>
00 00 // Block 3 : Length<br>
<br>
Offset table : 5*32 bits :<br>
(0x00000000) ./src_A.cc    // Block 1 start at 0 and have a length of 3, so it is composed by src_A.cc, header_1.h and header_2.h.<br>
(0x5B000000) ./header_1.h<br>
(0xB8000000) ./header_2.h<br>
(0x16010000) ./src_B.cc    // Block 2 start at 3 and have a length of 2, so it is composed by src_B.cc and header_1.h.<br>
(0x5B000000) ./header_1.h<br>
</code></pre>

<h2>Streams with variable numbers.</h2>

<h3>Names</h3>

<br>This stream ID can be found in the header of the DBI stream.<br>
<br>This stream contains the name to all the source files used to build the PE file matching the PDB.<br>
<br> The structure of this stream is :<br>
<br> - Signature (32 bits : 0xFEEFFEEF)<br>
<br> - 32 bits field (value = 1, seems to be a version or an age)<br>
<br> - Offset to the end of the name block (32 bits)<br>
<br> - Filename block + number of entries in the name table (32 bits)<br>
<br> - name table (set of offset relative to the beginning of the filename block)<br>
<br> - value that seems to be the number of filenames in the the filename block (32 bits) but it's not always the case (We've observed a difference of +/- 1 in some cases).<br>
<br>
<h3>Globals</h3>
<p>This stream ID can be found in the header of the DBI stream.<br>
<br>
<h3>Public</h3>
<p>The public stream contains information about public symbols. Its ID can be found in the header of the DBI stream. The structure of the stream is described below.<br>
<br>
<h4>Public stream header</h4>

<pre><code>struct PublicStreamHeader {<br>
  // The offset of the sorted table of public symbols, in bytes and relative<br>
  // to the |unknown| field of this header.<br>
  uint32 sorted_symbols_offset;<br>
<br>
  // The size of the sorted table of public symbols, in bytes.<br>
  // This is equal to 4 times the number of public symbols.<br>
  uint32 sorted_symbols_size;<br>
<br>
  // These fields are always equal to zero.<br>
  uint32 zero_0;<br>
  uint32 zero_1;<br>
  uint32 zero_2;<br>
  uint32 zero_3;<br>
<br>
  // Padding field, which can have any value.<br>
  uint32 padding;<br>
<br>
  // An unknown field that is always equal to -1.<br>
  uint32 unknown;<br>
<br>
  // The signature of the stream, which is equal to |kPublicStreamSignature|.<br>
  uint32 signature;<br>
<br>
  // The size of the table of public symbol offsets.<br>
  // This is equal to 8 times the number of public symbols.<br>
  uint32 offset_table_size;<br>
<br>
  // The size of the hash table of public symbols, in bytes. This includes<br>
  // a 512-byte bitset with a 1 in used buckets followed by an array identifying<br>
  // a representative of each bucket.<br>
  uint32 hash_table_size;<br>
};<br>
</code></pre>

<h4>Public symbol offsets</h4>

For each public symbol, in the same order as in the symbol record stream:<br>
<br>
- uint32: The offset of the symbol in the symbol record stream, incremented by one.<br>
<br>- uint32: The value 0x1.<br>
<br>
<h4>Public symbol hash table</h4>

The public stream then contains the representation of an hash table in which keys are symbol names. This part is omitted when the PDB doesn't contain public symbols.<br>
<br>
The hash table representation starts with a 512-byte bit set in which bits corresponding to non-empty buckets are set to one. The bucket corresponding to a symbol name can be computed using the HashString() function of <a href='https://code.google.com/p/sawbuck/source/browse/trunk/syzygy/pdb/pdb_util.cc'>pdb_util.cc</a>.<br>
<br>
After the bit set, we find an uint32 with value 0x0.<br>
<br>
Then, a representative of each bucket is listed using an uint32 that is the index of a public symbol in the "Public symbol offsets" table multiplied by 12.<br>
<br>
<br>
<h4>Sorted table of symbols</h4>

Finally, for each public symbol, ordered address in the image:<br>
<br>
- uint32: Index of the symbol in the symbol record stream.<br>
<br>
<h3>Modules</h3>
<p>Those stream IDs can be found in the DBI stream.<br>
<br>The first field encountered in this stream is a 4 bytes value indicating it's type, the expected value is C13 (4).<br>
<br>After this we have a symbol table. The size of this table is located in the information that we got for this stream in the DBI stream. This is the same type of table as the one we find in the symbol record stream.<br>
<br>The line information is arranged as a back-to-back run of {type, len} prefixed chunks. The types are DEBUG_S_FILECHKSMS and DEBUG_S_LINES. The first of these provides file names and a file content checksum, where each record is identified by its index into its chunk (excluding type and len). The other one consist of a bunch of CV_SourceFile structures.<br>
<br>
<h3>Section header</h3>
<p>Not encountered yet.<br>
<br>
<h3>Section header origin</h3>
<p>This stream ID can be found in the Dbg header of the DBI stream.<br>
<br>Not encountered yet.<br>
<br>
<h3>FPO (Frame pointer omission)</h3>
<p>This stream ID can be found in the Dbg header of the DBI stream.<br>
<br>
<h3>New FPO</h3>
<p>This stream ID can be found in the Dbg header of the DBI stream.<br>
<br>
<h3>Exception</h3>
<p>This stream ID can be found in the Dbg header of the DBI stream.<br>
<br>Not encountered yet.<br>
<br>
<h3>Fixup</h3>
<p>This stream ID can be found in the Dbg header of the DBI stream.<br>
<br>
<h3>Omap-to-src</h3>
<p>This stream ID can be found in the Dbg header of the DBI stream.<br>
<br>Not encountered yet.<br>
<br>
<h3>Omap-from-src</h3>
<p>This stream ID can be found in the Dbg header of the DBI stream.<br>
<br>Not encountered yet.<br>
<br>
<h3>Token rid map</h3>
<p>This stream ID can be found in the Dbg header of the DBI stream.<br>
<br>Not encountered yet.<br>
<br>
<h3>x-data</h3>
<p>This stream ID can be found in the Dbg header of the DBI stream.<br>
<br>Not encountered yet.<br>
<br>
<h3>p-data</h3>
<p>This stream ID can be found in the Dbg header of the DBI stream.<br>
<br>Not encountered yet..<br>
<br>
<h3>Type info hash</h3>
<p>This stream ID can be found in the header of the type info stream.<br>
<br>
<h3>Symbol record</h3>
<p>This stream ID can be found in the header of the DBI stream..<br>
<br>This stream contains a set of symbol record. The structure of each block is pretty simple :<br>
<br>- Symbol record length (2 bytes), the length field is not included in the length.<br>
<br>- Symbol record type ID (2 bytes).<br>
<br>- Symbol record data (length - 2 bytes).<br>
<br>
<br>The data content for each different kind of symbol can be match to a structure contained in the cvinfo header file. The different symbol types ID are also enumerated on this file. The document "Microsoft Symbol and Type Information" (see References) is useful to understand the content of this stream.<br>
<br>
<h2>References</h2>

<ul><li><a href='http://www.openwatcom.org/ftp/devel/docs/CodeView.pdf'>Microsoft Symbol and Type Information</a>.<br>
</li><li><a href='http://www.codeproject.com/Articles/37456/How-To-Inspect-the-Content-of-a-Program-Database-P'>How to Inspect the Content of a Program Database (PDB) File</a>.