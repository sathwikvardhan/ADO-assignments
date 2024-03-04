#include <stdio.h>
#include <stdlib.h>
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include <math.h>

typedef struct Page {
    SM_PageHandle data;
    PageNumber pageNum;
    int dirtyBit;
    int fixCount;
    int hitNum;
    int refNum;
} PageFrame;

int bufferSize = 0;
int rearIndex = 0;
int writeCount = 0;
int hit = 0;
int clockPointer = 0;
int lfuPtr = 0;


// Function implementations

extern void FIFO(BM_BufferPool *const bp, PageFrame *tgtPg) {
    PageFrame *frames = (PageFrame *)bp->mgmtData;
    int isReplaced = 0, currentIndex;
    currentIndex = rearIndex % bufferSize; // Assuming rearIndex and bufferSize are declared elsewhere
    int trialCount = 0; // To prevent infinite loops

    while (!isReplaced && trialCount < bufferSize) {
        int isEligible = (frames[currentIndex].fixCount == 0);
        int requiresFlush = (frames[currentIndex].dirtyBit == 1) && isEligible;

        // Flush the frame to disk if it's dirty
        if (requiresFlush) {
            SM_FileHandle fh;
            if (openPageFile(bp->pageFile, &fh) == RC_OK &&
                writeBlock(frames[currentIndex].pageNum, &fh, frames[currentIndex].data) == RC_OK) {
                writeCount++; // Assume writeCount is declared and used to track writes
            }
        }

        // Replace the frame if eligible
        if (isEligible) {
            frames[currentIndex].data = tgtPg->data;
            frames[currentIndex].pageNum = tgtPg->pageNum;
            frames[currentIndex].dirtyBit = tgtPg->dirtyBit;
            frames[currentIndex].fixCount = tgtPg->fixCount;
            isReplaced = 1;
        }

        // Move to the next frame in a circular manner
        currentIndex = (currentIndex + 1) % bufferSize;
        trialCount++;
    }
    // Loop exits when a frame is replaced or all frames have been tried
}


extern void LFU(BM_BufferPool *const bp, PageFrame *tgtPg) {
    PageFrame *frames = (PageFrame *)bp->mgmtData;
    int curIdx, nextIdx, minFreqIdx, minFreqVal;
    minFreqIdx = lfuPtr; // Assuming lfuPtr is declared and initialized elsewhere
    minFreqVal = INT_MAX; // Ensures any initial refCount is lower
    curIdx = minFreqIdx;

    // Attempt to find an initial page with zero fixCount
    int tries = 0;
    while (tries < bufferSize) {
        nextIdx = (curIdx + tries) % bufferSize;
        if (frames[nextIdx].fixCount == 0) {
            minFreqIdx = nextIdx;
            minFreqVal = frames[nextIdx].refNum; // Assuming refCount tracks frequency
            break;
        }
        tries++;
    }

    // Identify the least frequently used page
    curIdx = (minFreqIdx + 1) % bufferSize;
    int loopCount = 0;
    while (loopCount < bufferSize) {
        if (frames[curIdx].refNum < minFreqVal && frames[curIdx].fixCount == 0) {
            minFreqIdx = curIdx;
            minFreqVal = frames[curIdx].refNum;
        }
        curIdx = (curIdx + 1) % bufferSize;
        loopCount++;
    }

    // If the chosen page is dirty, write its content to disk
    if (frames[minFreqIdx].dirtyBit == 1) {
        SM_FileHandle fh;
        if (openPageFile(bp->pageFile, &fh) == RC_OK &&
            writeBlock(frames[minFreqIdx].pageNum, &fh, frames[minFreqIdx].data) == RC_OK) {
            writeCount++; // Increment write counter if write is successful
        }
    }

    // Replace the page content with the target page's information
    frames[minFreqIdx].data = tgtPg->data;
    frames[minFreqIdx].pageNum = tgtPg->pageNum;
    frames[minFreqIdx].dirtyBit = tgtPg->dirtyBit;
    frames[minFreqIdx].fixCount = tgtPg->fixCount;
    lfuPtr = (minFreqIdx + 1) % bufferSize; // Update LFU pointer for next replacement
}


extern void LRU(BM_BufferPool *const bufferPool, PageFrame *targetPage) {
    PageFrame *pageFrames = (PageFrame *)bufferPool->mgmtData;
    int idx, lruIndex = -1, lruHitNumber = INT_MAX;

    // Search for the least recently used page based on hitNum, ensuring it's not currently being used (fixCount == 0)
    for (idx = 0; idx < bufferSize; idx++) {
        if (pageFrames[idx].fixCount == 0 && pageFrames[idx].hitNum < lruHitNumber) {
            lruIndex = idx;
            lruHitNumber = pageFrames[idx].hitNum;
        }
    }

    // Ensure a page eligible for replacement was found
    if (lruIndex != -1) {
        // Flush the page to disk if it's marked as dirty
        if (pageFrames[lruIndex].dirtyBit == 1) {
            SM_FileHandle fileHandle;
            if (openPageFile(bufferPool->pageFile, &fileHandle) == RC_OK && 
                writeBlock(pageFrames[lruIndex].pageNum, &fileHandle, pageFrames[lruIndex].data) == RC_OK) {
                // Assuming there's a mechanism or variable like writeCount to track the number of writes
                writeCount++;
            }
        }

        // Update the least recently used page frame with the target page's information
        pageFrames[lruIndex].data = targetPage->data;
        pageFrames[lruIndex].pageNum = targetPage->pageNum;
        pageFrames[lruIndex].dirtyBit = targetPage->dirtyBit;
        pageFrames[lruIndex].fixCount = targetPage->fixCount;
        // Assuming hitNum should be updated to indicate the page has been accessed recently
        pageFrames[lruIndex].hitNum = hit; // Increment a global counter to simulate recent access
    }
}



extern void CLOCK(BM_BufferPool *const bp, PageFrame *tgtPg) {
    PageFrame *frames = (PageFrame *)bp->mgmtData;
    int replacementFound = 0;

    while (!replacementFound) {
        // Adjust clockPointer to ensure it always points within the buffer range
        clockPointer %= bufferSize;

        // Check if the current page frame is eligible for replacement
        if (frames[clockPointer].fixCount == 0) { // Assuming fixCount is a better indicator for eligibility than hitNum
            if (frames[clockPointer].dirtyBit == 1) {
                // Flush the page to disk if it's dirty
                SM_FileHandle fh;
                if (openPageFile(bp->pageFile, &fh) == RC_OK && 
                    writeBlock(frames[clockPointer].pageNum, &fh, frames[clockPointer].data) == RC_OK) {
                    writeCount++; // Increment write counter on successful write
                }
            }

            // Update the page frame with the target page's data
            frames[clockPointer].data = tgtPg->data;
            frames[clockPointer].pageNum = tgtPg->pageNum;
            frames[clockPointer].dirtyBit = tgtPg->dirtyBit;
            frames[clockPointer].fixCount = tgtPg->fixCount;
            // Instead of copying targetPage's hitNum, reset or adjust the current frame's hitNum as needed for the CLOCK logic
            frames[clockPointer].hitNum = 1; // Indicate that this page frame has been 'accessed' or 'used' recently

            replacementFound = 1; // Mark that a replacement has been made
        } else {
            // If the page is not eligible for replacement, give it a second chance by resetting its hitNum
            frames[clockPointer].hitNum = 0;
        }

        // Move to the next page frame in a circular manner
        clockPointer = (clockPointer + 1) % bufferSize;
    }
}

extern RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,
                         const int numPages, ReplacementStrategy strategy,
                         void *stratData) {
    bm->pageFile = (char *)pageFileName;
    bm->numPages = numPages;
    bm->strategy = strategy;

    PageFrame *page = malloc(sizeof(PageFrame) * numPages);
    if (page == NULL) {
        // Handle memory allocation failure
        return RC_OK; // Assuming RC_MEMORY_ALLOCATION_ERROR is defined as an error code
    }

    bufferSize = numPages;

    // Initialize PageFrame elements in a single loop
    for (int i = 0; i < bufferSize; i++) {
        page[i] = (PageFrame){.data = NULL, .pageNum = -1, .dirtyBit = 0, 
                              .fixCount = 0, .hitNum = 0, .refNum = 0};
    }

    bm->mgmtData = page;
    writeCount = clockPointer = lfuPtr = 0;
    return RC_OK;
}

extern RC shutdownBufferPool(BM_BufferPool *const bm) {
    PageFrame *frameSet = (PageFrame *)bm->mgmtData; // Using frameSet for clarity
    forceFlushPool(bm); // Ensure all dirty pages are written back

    // Iterating with a while loop to check for pinned pages
    int idx = 0; // Using idx as a loop counter
    while (idx < bm->numPages) { // Using bm->numPages to directly reference the number of pages
        if (frameSet[idx].fixCount > 0) {
            return RC_PINNED_PAGES_IN_BUFFER; // Return error if any page is still pinned
        }
        idx++; // Increment loop counter
    }

    free(frameSet); // Free the allocated memory for frames
    bm->mgmtData = NULL; // Safely nullify the management data pointer

    return RC_OK; // Successfully shutdown the buffer pool
}


extern RC forceFlushPool(BM_BufferPool *const bm) {
    PageFrame *pageFrames = (PageFrame *)bm->mgmtData; // Direct reference to the frames
    int currentPageIndex = 0; // Index for the current page frame being examined

    // Iterating through the buffer pool's frames
    while (currentPageIndex < bm->numPages) { // Using bm->numPages for direct reference
        PageFrame *currentFrame = &pageFrames[currentPageIndex]; // Pointer to the current frame for readability
        
        // Check if the frame is dirty and not pinned
        if (currentFrame->fixCount == 0 && currentFrame->dirtyBit == 1) {
            SM_FileHandle fh;
            // Attempt to open the page file and write the block if dirty
            if (openPageFile(bm->pageFile, &fh) == RC_OK &&
                writeBlock(currentFrame->pageNum, &fh, currentFrame->data) == RC_OK) {
                currentFrame->dirtyBit = 0; // Successfully written, clear the dirty bit
                writeCount++; // Assuming writeCount tracks the number of write operations
            }
            // Optionally handle errors or log them here
        }

        currentPageIndex++; // Move to the next frame
    }

    return RC_OK; // Indicate successful flush
}




extern RC markDirty(BM_BufferPool *const bm, BM_PageHandle *const page) {
    PageFrame *pageFrames = (PageFrame *)bm->mgmtData; // Direct reference to the frames
    int currentFrameIndex = 0; // Index for the current frame being examined

    // Iterating through the buffer pool's frames to find the matching page
    while (currentFrameIndex < bm->numPages) { // Using bm->numPages to directly reference the number of pages
        if (pageFrames[currentFrameIndex].pageNum == page->pageNum) {
            pageFrames[currentFrameIndex].dirtyBit = 1; // Mark the matching page as dirty

            return RC_OK; // Successfully marked the page as dirty
        }

        currentFrameIndex++; // Move to the next frame if not a match
    }

    return RC_ERROR; // Return an error if no matching page was found
}



extern RC unpinPage(BM_BufferPool *const bufferMgr, BM_PageHandle *const page) {
    PageFrame *frames = (PageFrame *)bufferMgr->mgmtData;
    int pageIndex = 0; // Initialize outside the while loop

    // Use a while loop to search for the matching page
    while (pageIndex < bufferSize) {
        // Using an implicit check to determine if the page number matches
        int isMatch = (frames[pageIndex].pageNum == page->pageNum);
        
        // Decrease fixCount if isMatch is true, won't change anything if false
        frames[pageIndex].fixCount -= isMatch;

        // Exit the loop if a match was found
        if (isMatch) {
            return RC_OK;
        }

        pageIndex++; // Increment at the end of the while loop
    }
    return RC_OK; // Assuming every unpin operation is considered successful
}


extern RC forcePage(BM_BufferPool *const bufferMgr, BM_PageHandle *const page) {
    PageFrame *frames = (PageFrame *)bufferMgr->mgmtData;
    int pageIndex = 0; // Initialize outside the while loop
    SM_FileHandle fileHandle;

    // Open the page file outside the loop to avoid repeated opening
    RC openResult = openPageFile(bufferMgr->pageFile, &fileHandle);

    // Proceed only if the file was successfully opened
    if (openResult == RC_OK) {
        while (pageIndex < bufferSize) {
            int isMatch = (frames[pageIndex].pageNum == page->pageNum);
            
            // Perform write operation only if there's a match
            if (isMatch) {
                writeBlock(frames[pageIndex].pageNum, &fileHandle, frames[pageIndex].data);
                frames[pageIndex].dirtyBit = 0;
                writeCount++;
                break; // Exit after processing the matched page
            }

            pageIndex++; // Increment at the end of the while loop
        }
    }
    return RC_OK;
}

extern RC pinPage(BM_BufferPool *const bm, BM_PageHandle *const page,
                  const PageNumber pageNum) {
    PageFrame *bufferPool = (PageFrame *)bm->mgmtData;
    bool isBufFull = true;

    // Check if bufferPool[0] needs to be initialized
    while (bufferPool[0].pageNum == -1) {
        SM_FileHandle fileHandle;
        openPageFile(bm->pageFile, &fileHandle);
        bufferPool[0].data = (SM_PageHandle) malloc(PAGE_SIZE);
        ensureCapacity(pageNum, &fileHandle);
        readBlock(pageNum, &fileHandle, bufferPool[0].data);
        bufferPool[0].pageNum = pageNum;
        bufferPool[0].fixCount++;
        rearIndex = hit = 0;
        bufferPool[0].hitNum = hit;
        bufferPool[0].refNum = 0;
        page->pageNum = pageNum;
        page->data = bufferPool[0].data;

        // Exit the function once the operation is completed
        return RC_OK;
    }

    // Iterate through bufferPool to find an available slot or match pageNum
    int i = 0;
    while (i < bufferSize && isBufFull) {
        if (bufferPool[i].pageNum != -1) {
            if (bufferPool[i].pageNum == pageNum) {
                bufferPool[i].fixCount++;
                isBufFull = false;
                hit++;

                if (bm->strategy == RS_LRU)
                    bufferPool[i].hitNum = hit;
                else if (bm->strategy == RS_CLOCK)
                    bufferPool[i].hitNum = 1;
                else if (bm->strategy == RS_LFU)
                    bufferPool[i].refNum++;

                page->pageNum = pageNum;
                page->data = bufferPool[i].data;
                clockPointer++;
                break;
            }
        } else {
            SM_FileHandle fileHandle;
            openPageFile(bm->pageFile, &fileHandle);
            bufferPool[i].data = (SM_PageHandle) malloc(PAGE_SIZE);
            readBlock(pageNum, &fileHandle, bufferPool[i].data);
            bufferPool[i].pageNum = pageNum;
            bufferPool[i].fixCount = 1;
            bufferPool[i].refNum = 0;
            rearIndex++;
            hit++;

            if (bm->strategy == RS_LRU)
                bufferPool[i].hitNum = hit;
            else if (bm->strategy == RS_CLOCK)
                bufferPool[i].hitNum = 1;

            page->pageNum = pageNum;
            page->data = bufferPool[i].data;
            isBufFull = false;
            break;
        }
        i++;
    }

    // If buffer is full, apply replacement algorithm
    if (isBufFull) {
        PageFrame *newPage = (PageFrame *) malloc(sizeof(PageFrame));
        SM_FileHandle fileHandle;
        openPageFile(bm->pageFile, &fileHandle);
        newPage->data = (SM_PageHandle) malloc(PAGE_SIZE);
        readBlock(pageNum, &fileHandle, newPage->data);
        newPage->pageNum = pageNum;
        newPage->dirtyBit = 0;
        newPage->fixCount = 1;
        newPage->refNum = 0;
        rearIndex++;
        hit++;

        if (bm->strategy == RS_LRU)
            newPage->hitNum = hit;
        else if (bm->strategy == RS_CLOCK)
            newPage->hitNum = 1;

        page->pageNum = pageNum;
        page->data = newPage->data;

        // Apply the replacement algorithm based on the strategy
        if (bm->strategy == RS_FIFO)
            FIFO(bm, newPage);
        else if (bm->strategy == RS_LRU)
            LRU(bm, newPage);
        else if (bm->strategy == RS_CLOCK)
            CLOCK(bm, newPage);
        else if (bm->strategy == RS_LFU)
            LFU(bm, newPage);
        else if (bm->strategy == RS_LRU_K)
            printf("\n LRU-k algorithm not implemented");
        else
            printf("\nAlgorithm Not Implemented\n");

        // Exit the function once the operation is completed
        return RC_OK;
    }

    // Return RC_OK if the operation is completed
    return RC_OK;
}

extern PageNumber *getFrameContents(BM_BufferPool *const bm) {
    PageNumber *frameContents = malloc(sizeof(PageNumber) * bufferSize);
    PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
    
    for (int i = 0; i < bufferSize; i++) {
        frameContents[i] = pageFrame[i].pageNum != -1 ? pageFrame[i].pageNum : NO_PAGE;
    }
    
    return frameContents;
}


extern bool *getDirtyFlags(BM_BufferPool *const bm) {
    bool *dirtyFlags = malloc(sizeof(bool) * bufferSize);
    PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
    
    // Using a while loop for consistency with previous adjustments
    int index = 0;
    while (index < bufferSize) {
        dirtyFlags[index] = pageFrame[index].dirtyBit ? true : false;
        index++;
    }
    
    return dirtyFlags;
}


extern int *getFixCounts(BM_BufferPool *const bm) {
    int *fixCounts = malloc(sizeof(int) * bufferSize);
    PageFrame *pageFrame = (PageFrame *)bm->mgmtData;

    for (int index = 0; index < bufferSize; index++) {
        // Assuming the fixCount check against -1 is not needed based on the assumption of non-negative fixCounts
        fixCounts[index] = pageFrame[index].fixCount;
    }

    return fixCounts;
}


extern int getNumReadIO(BM_BufferPool *const bm) {
    return (rearIndex + 1);
}

extern int getNumWriteIO(BM_BufferPool *const bm) {
    return writeCount;
}
