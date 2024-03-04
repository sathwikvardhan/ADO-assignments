CS-525 Assignment No.2 Buffer manager

Team Members
      •Jagadeesh Kumar Malladi					A20544038
      •Sathwik Vardhan Reddy Pothireddy	        A20528418
      •Srija Jyothula							A20558248
      •Sneha Poojitha Gade						A20558333
	


RUNNING THE SCRIPT

1. make sure youre using the correct directory.
2. use "make" command.
3. run "make test1"
4. run "make run_test1"
5. run "make test2"
6. run "make run_test2"

Included files:

	Makefile
	README.txt
	buffer_mgr.c
	buffer_mgr.h
	buffer_mgr_stat.c
	buffer_mgr_stat.h
	dberror.c
	dberror.h
	dt.h
	storage_mgr.c
	storage_mgr.h
	test_assign2_1.c
	test_assign2_2.c
	test_helper.h

> BUFFER POOL FUNCTIONS

The buffer pool-related functions are used to create a buffer pool for an existing page file on disk. The buffer pool is created in memory while the page file is present on disk. We make use of Storage Manager (Assignment 1) to perform operations on page files on disk.

--> initBufferPool(...)  
This function initializes a buffer pool with specified parameters for caching pages from a file using a chosen replacement strategy.

--> shutdownBufferPool(...) 
This function deallocates the buffer pool resources, flushing modified pages to disk and handling errors if pages are in use.

--> forceFlushPool(...) 
This function writes modified, unused pages (dirtyBit = 1 and fixCount = 0) to the disk.


> PAGE MANAGEMENT FUNCTIONS
The page management-related functions are used to load pages from the disk into the buffer pool (pin pages), remove a page frame from the buffer pool (unpin page), mark the page as dirty, and force a page frame to be written to the disk.

--> pinPage(...) 
This function pins the page pageNum, using replacement strategies when needed and writing the contents of a replaced dirty page to the disk.

--> unpinPage(...)  
 This function unpins the specified page by decrementing its fixCount, indicating the end of client usage.

--> makeDirty(...) 
 This function sets the dirty bit of the specified page frame to 1 by iteratively searching for the page frame using pageNum within the buffer pool.

--> forcePage(....) 
This function writes the specified page frame's content to the disk file after locating it by pageNum in the buffer pool, setting the dirty bit to 0 afterward.


> STATISTICS FUNCTIONS
The statistical functions are utilized to collect data regarding the buffer pool, offering diverse statistical insights into its operations.

--> getFrameContents(...)
This function retrieves an array of page numbers corresponding to each page frame in the buffer pool, with each element representing the page number of the respective frame.

--> getDirtyFlags(...)
This function generates an array of boolean values, where each element represents whether the page stored in the respective page frame within the buffer pool is dirty or not.

--> getFixCounts(...) 
This function provides an integer array representing the fixCount values of page frames in the buffer pool, with each element denoting the fixCount of the respective page stored in the frame.

--> getNumReadIO(...)
This function returns the total count of input/output reads executed by the buffer pool, specifically indicating the number of pages read from the disk, tracked using the rearIndex variable.

--> getNumWriteIO(...)
This function returns the total count of I/O writes conducted by the buffer pool, reflecting the number of pages written to the disk, with the writeCount variable tracking these operations from initialization, incrementing upon each page frame written to disk.



> PAGE REPLACEMENT ALGORITHM FUNCTION

The page replacement strategy functions implement FIFO, LRU, LFU, and CLOCK algorithms which are used while pinning a page. When the buffer pool reaches its capacity and a new page needs to be pinned, an existing page must be replaced. The selection of the page to be replaced from the buffer pool is determined by page replacement strategies.

--> FIFO(...)
For the FIFO page replacement strategy, pages are replaced in the order they were initially loaded into the buffer pool, functioning akin to a queue where the earliest loaded page is replaced first when the buffer pool reaches capacity, followed by writing the page frame's content to the disk before inserting the new page at the same location.

--> LFU(...)
LFU selects the least accessed page frame in the buffer pool based on the refNum field, representing the count of page frame accesses by the client, facilitating efficient page replacement by identifying the position of the least frequently used page frame with the lfuPointer variable for subsequent replacements, thereby minimizing iterations.

--> LRU(...)
LRU removes the least recently used page frame from the buffer pool based on the hitNum field, representing the count of page frame accesses and pins by the client, with a global variable "hit" facilitating this operation, ensuring the replacement of the page frame with the lowest hitNum value, followed by writing its content to disk and inserting the new page.

--> CLOCK(...)
The CLOCK algorithm tracks the most recently added page frame in the buffer pool using a clock pointer, which iterates through the page frames. When replacement is needed, if the page at the clockPointer position has hitNum ≠ 1, indicating it was not the last added page, it is replaced; otherwise, hitNum is set to 0, clockPointer is incremented, and the process continues until a replacement position is found, preventing infinite loops by resetting hitNum to 0.    
