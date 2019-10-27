# SmartWaterServer-hackathon
This server stores measurements form the ttn backbone and provides them through a REST api. 

## Storage
The data is stored in a custom block based file format. Every block (or chunk) is 4096 bytes. The first block is an index block.
It stores the beginning blocks of tables, as well as a pointer to the next index block. A table is stored as a chain of data blocks.
A data block can store arbitrary data. It also contains a pointer to the next data block. The format also supports journaling, by
using the second and third chunk in the file as a journal. The first of these chunks contains the id of a dirty chunk.
This id is 1 if no chunk is dirty (the block itself is block 1). The second journal chunk contains a copy of the dirty chunk from
before writing. When loading, if a chunk i smarked as dirty the saved copy of the chunk is copied from chunk 3 to the dirty chunk
and the journal cleared.
