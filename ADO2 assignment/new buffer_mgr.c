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
} PFrame;

int bufferSize = 0;
int rearIndex = 0;
int writeCount = 0;
int hit = 0;
int clockPointer = 0;
int lfuPointer = 0;

// Function prototypes
extern void FIFO(BM_BufferPool *const bp, PFrame *newPage);
extern void LFU(BM_BufferPool *const bp, PFrame *newPage);
extern void LRU(BM_BufferPool *const bp, PFrame *newPage);
extern void CLOCK(BM_BufferPool *const bp, PFrame *newPage);

extern RC initBufferPool(BM_BufferPool *const bp, const char *const fileName,
                         const int pageCount, ReplacementStrategy strategy,
                         void *stratData);

extern RC shutdownBufferPool(BM_BufferPool *const bp);
extern RC forceFlushPool(BM_BufferPool *const bp);
extern RC markDirty(BM_BufferPool *const bp, BM_PageHandle *const page);
extern RC unpinPage(BM_BufferPool *const bp, BM_PageHandle *const page);
extern RC forcePage(BM_BufferPool *const bp, BM_PageHandle *const page);
extern RC pinPage(BM_BufferPool *const bp, BM_PageHandle *const page,
                   const PageNumber pageNum);

extern PageNumber *getFrameContents(BM_BufferPool *const bp);
extern bool *getDirtyFlags(BM_BufferPool *const bp);
extern int *getFixCounts(BM_BufferPool *const bp);
extern int getNumReadIO(BM_BufferPool *const bp);
extern int getNumWriteIO(BM_BufferPool *const bp);

// Function implementations

extern void FIFO(BM_BufferPool *const bp, PFrame *newPage) {
    PFrame *frameList = (PFrame *)bp->mgmtData;

    // Check if there are any unpinned frames
    int unpinnedIndex = -1;
    for (int i = 0; i < bufferSize; i++) {
        if (frameList[i].fixCount == 0) {
            unpinnedIndex = i;
            break;
        }
    }

    if (unpinnedIndex != -1) {
        // Found an unpinned frame, use it
        if (frameList[unpinnedIndex].dirtyBit == 1) {
            SM_FileHandle fileHandle;
            openPageFile(bp->pageFile, &fileHandle);
            writeBlock(frameList[unpinnedIndex].pageNum, &fileHandle, frameList[unpinnedIndex].data);
            writeCount++;
        }

        frameList[unpinnedIndex].data = newPage->data;
        frameList[unpinnedIndex].pageNum = newPage->pageNum;
        frameList[unpinnedIndex].dirtyBit = newPage->dirtyBit;
        frameList[unpinnedIndex].fixCount = newPage->fixCount;
    } else {
        // All frames are pinned, replace the oldest (first) unpinned frame
        extern void FIFO(BM_BufferPool *const bp, PFrame *newPage) 
    
    PFrame *frameList = (PFrame *)bp->mgmtData;
    int index, front;

    front = rearIndex % bufferSize;

    while (frameList[front].fixCount != 0) {
        front++;
        front = (front % bufferSize == 0) ? 0 : front;
    }

    if (frameList[front].dirtyBit == 1) {
        SM_FileHandle fileHandle;
        openPageFile(bp->pageFile, &fileHandle);
        writeBlock(frameList[front].pageNum, &fileHandle, frameList[front].data);
        writeCount++;
    }

    frameList[front].data = newPage->data;
    frameList[front].pageNum = newPage->pageNum;
    frameList[front].dirtyBit = newPage->dirtyBit;
    frameList[front].fixCount = newPage->fixCount;
    rearIndex = (rearIndex + 1) % bufferSize;
}

    }
}


 extern void LFU(BM_BufferPool *const bp, PFrame *newPage) {
    PFrame *frameList = (PFrame *)bp->mgmtData;
    int index, leastFreqIndex, leastFreq;

    // Find the first unpinned frame
    for (index = 0; index < bufferSize; index++) {
        if (frameList[index].fixCount == 0) {
            leastFreqIndex = index;
            leastFreq = frameList[index].frequency;
            break;
        }
    }

    // Find the frame with the least frequency
    for (index = leastFreqIndex + 1; index < bufferSize; index++) {
        int leastFrequentFrameIndex = -1;
int leastFrequency = INT_MAX; // Initialize with the maximum possible value
PFrame *frames = frameList; // Assuming frameList is an array of PFrame

// Find the least frequent frame
for (int i = 0; i < bufferSize; i++) {
    if (frames[i].fixCount == 0 && frames[i].frequency < leastFrequency) {
        leastFrequentFrameIndex

 = i;
        leastFrequency = frames[i].frequency;
    }
}

// If the chosen frame is dirty, write it to disk
if (leastFrequentFrameIndex != -1 && frames[leastFrequentFrameIndex].isDirty) {
    SM_FileHandle fileHandle;
    openPageFile(bp->pageFile, &fileHandle);
    writeBlock(frames[leastFrequentFrameIndex].pageNum, &fileHandle, frames[leastFrequentFrameIndex].data);
    bp->numWriteIO++; // Assuming numWriteIO tracks write operations
}

// Update the chosen frame with the new page
if (leastFrequentFrameIndex != -1) {
    frames[leastFrequentFrameIndex].data = newPage->data;
    frames[leastFrequentFrameIndex].pageNum = newPage->pageNum;
    frames[leastFrequentFrameIndex].isDirty = newPage->isDirty;
}
    frameList[leastFreqIndex].fixCount = newPage->fixCount;
    frameList[leastFreqIndex].freq++;  // Increment the frequency
}


extern void LRU(BM_BufferPool *const bp, PFrame *newPage) {
    PFrame *frameList = (PFrame *)bp->mgmtData;
    int index, leastRecentIndex, leastRecentAccess;

    // Find the first unpinned frame
    for (index = 0; index < bufferSize; index++) {
        if (frameList[index].fixCount == 0) {
            leastRecentIndex = index;
            leastRecentAccess = frameList[index].lastAccess;
            break;
        }
    }

    // Find the frame with the least recent access time
    for (index = leastRecentIndex + 1; index < bufferSize; index++) {
        if (frameList[index].fixCount == 0 && frameList[index].lastAccess < leastRecentAccess) {
            leastRecentIndex = index;
            leastRecentAccess = frameList[index].lastAccess;
        }
    }

    // If the chosen frame is dirty, write it to disk
    if (frameList[leastRecentIndex].dirtyBit == 1) {
        SM_FileHandle fileHandle;
        openPageFile(bp->pageFile, &fileHandle);
        writeBlock(frameList[leastRecentIndex].pageNum, &fileHandle, frameList[leastRecentIndex].data);
        writeCount++;
    }

    // Update the chosen frame with the new page and update the access time
    frameList[leastRecentIndex].data = newPage->data;
    frameList[leastRecentIndex].pageNum = newPage->pageNum;
    frameList[leastRecentIndex].dirtyBit = newPage->dirtyBit;
    frameList[leastRecentIndex].fixCount = newPage->fixCount;
    frameList[leastRecentIndex].lastAccess = getCurrentTime();  // Update the access time to the current time
}

extern void CLOCK(BM_BufferPool *const bp, PFrame *newPage) {
    PFrame *frameList = (PFrame *)bp->mgmtData;
    
    // Iterate through frames using CLOCK algorithm
    while (1) {
        clockHand = (clockHand % bufferSize == 0) ? 0 : clockHand;

        if (frameList[clockHand].accessCount == 0) {
            // If the frame has not been recently accessed
            if (frameList[clockHand].isDirty) {
                // If the frame is dirty, write it to disk
                SM_FileHandle fileHandle;
                openPageFile(bp->pageFile, &fileHandle);
                writeBlock(frameList[clockHand].pageNum, &fileHandle, frameList[clockHand].data);
                bp->numWriteIO++;
            }

            // Update the frame with the new page and set its properties
            frameList[clockHand].data = newPage->data;
            frameList[clockHand].pageNum = newPage->pageNum;
            frameList[clockHand].isDirty = newPage->isDirty;
            frameList[clockHand].fixCount = newPage->fixCount;
            frameList[clockHand].accessCount = newPage->accessCount;
            clockHand++;
            break;
        } else {
            // If the frame has been recently accessed, reset its accessCount
            frameList[clockHand++].accessCount = 0;
        }
    }
}


extern RC initBufferPool(BM_BufferPool *const bp, const char *const fileName,
                         const int pageCount, ReplacementStrategy strategy,
                         void *stratData) {
    bp->pageFile = (char *)fileName;
    bp->numPages = pageCount;
    bp->strategy = strategy;

    PFrame *frames = malloc(sizeof(PFrame) * pageCount);

    bufferSize = pageCount;
    return RC_OK;
}

    bp->mgmtData = frames;

    // Initialize each frame using memset
    memset(frames, 0, sizeof(PFrame) * pageCount);

    writeCount = clockPointer = lfuPointer = 0;
    return RC_OK;
}

extern RC shutdownBufferPool(BM_BufferPool *const bp) {
    PFrame *frames = (PFrame *)bp->mgmtData;

    // Force flush to write dirty pages to disk
    forceFlushPool(bp);

    int index = 0;

    // Check if there are any pinned pages in the buffer using a while loop
    while (index < bufferSize && frames[index].fixCount == 0) {
        index++;
    }

    // If there are pinned pages, cannot shut down
    if (index < bufferSize) {
        return RC_PINNED_PAGES_IN_BUFFER;
    }

    // No pinned pages, free memory and shut down
    free(frames);
    bp->mgmtData = NULL;
    return RC_OK;
}




extern RC forceFlushPool(BM_BufferPool *const bp) {
    PFrame *frames = (PFrame *)bp->mgmtData;
    int index = 0;

    // Iterate through frames and flush dirty pages
    while (index < bufferSize) {
        if (frames[index].fixCount == 0 && frames[index].dirtyBit == 1) {
            // Open the page file and write the block to disk
            SM_FileHandle fileHandle;
            openPageFile(bp->pageFile, &fileHandle);
            writeBlock(frames[index].pageNum, &fileHandle, frames[index].data);

            // Update dirty bit and increment write count
            frames[index].dirtyBit = 0;
            writeCount++;
        }
        index++;
    }

    return RC_OK;
}


extern RC markDirty(BM_BufferPool *const bp, BM_PageHandle *const page) {
    PFrame *frames = (PFrame *)bp->mgmtData;
    int index = 0;

    // Search for the page with the specified page number
    while (index < bufferSize) {
        if (frames[index].pageNum == page->pageNum) {
            // Mark the page as dirty
            frames[index].dirtyBit = 1;
            return RC_OK;
        }
        index++;
    }

    // Page not found, return an error
    return RC_ERROR;
}

extern RC unpinPage(BM_BufferPool *const bp, BM_PageHandle *const page) {
    PFrame *frames = (PFrame *)bp->mgmtData;
    int index = 0;


    // Search for the page with the specified page number
    while (index < bufferSize) {
        if (frames[index].pageNum == page->pageNum) {
            // Decrease the fix count
            frames[index].fixCount--;
            break;
        }
        index++;
    }

    // Return OK status
    return RC_OK;
}



extern RC forcePage(BM_BufferPool *const bp, BM_PageHandle *const page) {
    PFrame *frames = (PFrame *)bp->mgmtData;
    int index = 0;

    // Search for the page with the specified page number
    while (index < bufferSize) {
        if (frames[index].pageNum == page->pageNum) {
            // Write the page to disk if it's dirty
            SM_FileHandle fileHandle;
            openPageFile(bp->pageFile, &fileHandle);
            writeBlock(frames[index].pageNum, &fileHandle, frames[index].data);
            frames[index].dirtyBit = 0;
            writeCount++;
        }
        index++;
    }

    // Return OK status
    return RC_OK;
}


extern RC pinPage(BM_BufferPool *const bp, BM_PageHandle *const page, const PageNumber pageNum) {
    PFrame *frames = (PFrame *)bp->mgmtData;

    SM_FileHandle fileHandle;
    openPageFile(bp->pageFile, &fileHandle);

    // Check if the page is already in the buffer
    int index;
    for (index = 0; index < bufferSize; index++) {
        if (frames[index].pageNum == pageNum) {
            frames[index].fixCount++;
            updateHitInfo(bp, &frames[index]);
            page->pageNum = pageNum;
            page->data = frames[index].data;
            movePageToFront(frames, index);
            return RC_OK;
        }
    }

    // If the buffer is not full, find an empty frame
    for (index = 0; index < bufferSize; index++) {
        if (frames[index].pageNum == -1) {
            readPageIntoFrame(bp, pageNum, &fileHandle, &frames[index]);
            page->pageNum = pageNum;
            page->data = frames[index].data;
            frames[index].fixCount = 1;
            frames[index].refNum = 0;
            rearIndex = (rearIndex + 1) % bufferSize;
            hit++;
            updateHitInfo(bp, &frames[index]);
            return RC_OK;
        }
    }

    // If the buffer is full, apply replacement strategy
    PFrame *newPage = (PFrame *)malloc(sizeof(PFrame));
    readPageIntoFrame(bp, pageNum, &fileHandle, newPage);
    newPage->fixCount = 1;
    newPage->refNum = 0;
    rearIndex = (rearIndex + 1) % bufferSize;
    hit++;
    updateHitInfo(bp, newPage);
    page->pageNum = pageNum;
    page->data = newPage->data;

    applyReplacementStrategy(bp, newPage);

    return RC_OK;
}

// Helper functions

void readPageIntoFrame(BM_BufferPool *const bp, const PageNumber pageNum, SM_FileHandle *fileHandle, PFrame *frame) {
    frame->data = (SM_PageHandle)malloc(PAGE_SIZE);
    ensureCapacity(pageNum, fileHandle);
    readBlock(pageNum, fileHandle, frame->data);
    frame->pageNum = pageNum;
}

void updateHitInfo(BM_BufferPool *const bp, PFrame *frame) {
    if (bp->strategy == RS_LRU)
        frame->hitNum = ++hit;
    else if (bp->strategy == RS_CLOCK)
        frame->hitNum = 1;
}

void movePageToFront(PFrame *frames, int index) {
    // Move the page to the front to simulate LRU
    PFrame temp = frames[index];
    for (int i = index; i > 0; i--)
        frames[i] = frames[i - 1];
    frames[0] = temp;
}

void applyReplacementStrategy(BM_BufferPool *const bp, PFrame *newPage) {
    switch (bp->strategy) {
        case RS_FIFO:
            FIFO(bp, newPage);
            break;

        case RS_LRU:
            LRU(bp, newPage);
            break;

        case RS_CLOCK:
            CLOCK(bp, newPage);
            break;

        case RS_LFU:
            LFU(bp, newPage);
            break;

        case RS_LRU_K:
            printf("\n LRU-k algorithm not implemented");
            break;

        default:
            printf("\nAlgorithm Not Implemented\n");
            break;
    }
}


extern PageNumber *getFrameContents(BM_BufferPool *const bp) {
    PFrame *frames = (PFrame *)bp->mgmtData;

    // Allocate memory for frame contents array
    PageNumber *frameContents = malloc(sizeof(PageNumber) * bufferSize);

    // Check if memory allocation is successful
    if (frameContents == NULL) {
        // Handle memory allocation failure, e.g., return an error code
        return NULL;
    }

    // Iterate through frames to retrieve page numbers
    for (int index = 0; index < bufferSize; index++) {
        // Store the page number in the array, or NO_PAGE if the frame is not used
        frameContents[index] = (frames[index].pageNum != -1) ? frames[index].pageNum : NO_PAGE;
    }

    return frameContents;
}


extern bool *getDirtyFlags(BM_BufferPool *const bp) {
    PFrame *frames = (PFrame *)bp->mgmtData;

    // Allocate memory for dirty flags array
    bool *dirtyFlags = malloc(sizeof(bool

) * bufferSize);

    // Check if memory allocation is successful
    if (dirtyFlags == NULL) {
        // Handle memory allocation failure, e.g., return an error code
        return NULL;
    }

    // Iterate through frames to retrieve dirty flags
    for (int index = 0; index < bufferSize; index++) {
        // Store the dirty flag in the array
        dirtyFlags[index] = (frames[index].dirtyBit == 1) ? true : false;
    }

    return dirtyFlags;
}


extern int *getFixCounts(BM_BufferPool *const bp) {
    PFrame *frames = (PFrame *)bp->mgmtData;

    // Allocate memory for fix counts array
    int *fixCounts = malloc(sizeof(int) * bufferSize);

    // Check if memory allocation is successful
    if (fixCounts == NULL) {
        // Handle memory allocation failure, e.g., return an error code
        return NULL;
    }

    // Iterate through frames to retrieve fix counts
    for (int index = 0; index < bufferSize; index++) {
        // Store the fix count in the array
        fixCounts[index] = frames[index].fixCount;
    }

    return fixCounts;
}

extern int getNumReadIO(BM_BufferPool *const bp) {
    return bp->numReadIO;
}

extern int getNumWriteIO(BM_BufferPool *const bp) {
    return bp->numWriteIO;

}