#include<stdio.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>
#include<string.h>
#include<math.h>
#include "storage_mgr.h"

FILE *pageFile;

extern void initStorageManager (void) {
	// Initialising file pointer i.e. storage manager.
	pageFile = NULL;
}

extern RC createPageFile(char *path) {
    FILE *fileDescriptor = fopen(path, "wb+"); // Try to open or create the file in a way that works on all computers.

    if (fileDescriptor == NULL) {
        printError(RC_FILE_NOT_FOUND);
        return RC_FILE_NOT_FOUND; // Use a specific error code to say the file couldn't be found.
    }

    char *buffer = (char *)malloc(PAGE_SIZE * sizeof(char)); // Get some space in memory to hold a page's content.
    if (buffer == NULL) {
        fclose(fileDescriptor); // If getting memory space didn't work, close the file.
        printError(RC_ERROR); // Use a general error code for this problem.
        return RC_ERROR; // Say there was a problem getting memory space.
    }

    memset(buffer, 0, PAGE_SIZE); // Make the page's content empty to start with.

    size_t writtenBytes = fwrite(buffer, sizeof(char), PAGE_SIZE, fileDescriptor);
    if (writtenBytes < PAGE_SIZE) {
        printf("Error writing to file\n"); // Here, you could use a specific error code for writing problems.
        free(buffer);
        fclose(fileDescriptor);
        return RC_WRITE_FAILED; // Use a specific error code to say writing didn't work.
    }

    printf("File created successfully\n");

    fclose(fileDescriptor); // Make sure to close the file when done.
    free(buffer); // Give back the memory space we got for the page.

    return RC_OK; // Say everything worked out.
}


extern RC openPageFile(char *fileName, SM_FileHandle *fHandle) {
    // Try to open the file so we can read from it.
    FILE *fileStream = fopen(fileName, "r");

    // If we can't open the file, let the user know it wasn't found.
    if (fileStream == NULL) {
        printError(RC_FILE_NOT_FOUND);
        return RC_FILE_NOT_FOUND;
    } else {
        // Set the file's name and start at the beginning of the file in our tracking info.
        fHandle->fileName = fileName;
        fHandle->curPagePos = 0;

        // Go to the file's end to figure out how big it is.
        fseek(fileStream, 0, SEEK_END);
        long fileSize = ftell(fileStream); // Find out the file's size.
        fseek(fileStream, 0, SEEK_SET); // Go back to the start of the file.

        // Work out how many pages the file has and update our tracking info.
        fHandle->totalNumPages = fileSize / PAGE_SIZE;

        // All done with the file for now, so close it.
        fclose(fileStream);
        return RC_OK;
    }
}


extern RC closePageFile(SM_FileHandle *fHandle) {
    // Make sure we actually have a file to work with.
    if (fHandle != NULL) {
        // Clean up our record of the file since we're not closing a file, just forgetting it.
        fHandle->fileName = NULL;
        fHandle->totalNumPages = 0;
        fHandle->curPagePos = -1; // Use -1 to show it's not pointing at any page.
        return RC_OK;
    } else {
        // If there's no file info to work with, say it wasn't set up right.
        printError(RC_FILE_HANDLE_NOT_INIT);
        return RC_FILE_HANDLE_NOT_INIT;
    }
}


extern RC destroyPageFile (char *fileName) {
    FILE* f1 = fopen(fileName, "r+"); // Open to check if the file exists and can be read/written.
    if(f1 != NULL){
        fclose(f1); // Make sure to close the file first.
        remove(fileName); // Then go ahead and delete the file.
        THROW(RC_OK, "File successfully removed."); // Indicate the file was deleted successfully.
    } else {
        THROW(RC_FILE_NOT_FOUND, "File does not exist."); // Tell the user the file couldn't be found if it's not there.
    }
}


extern RC readBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {
    // Check if the file info and place to put the page are okay.
    if (fHandle == NULL || memPage == NULL) {
        return RC_ERROR; // Maybe use more specific error codes for these problems.
    }

    // Make sure the page we want to read actually exists.
    if (pageNum < 0 || pageNum >= fHandle->totalNumPages) {
        return RC_READ_NON_EXISTING_PAGE;
    }

    // Try to open the file so we can read from it.
    FILE *pageFile = fopen(fHandle->fileName, "r");
    if (pageFile == NULL) {
        return RC_FILE_NOT_FOUND;
    }

    // Go to the start of the page we're interested in.
    if (fseek(pageFile, pageNum * PAGE_SIZE, SEEK_SET) != 0) {
        fclose(pageFile); // Close the file if moving didn't work.
        return RC_READ_NON_EXISTING_PAGE;
    }

    // Get the page's data into our memory space.
    size_t readBytes = fread(memPage, sizeof(char), PAGE_SIZE, pageFile);
    if (readBytes < PAGE_SIZE) {
        fclose(pageFile); // Close the file if we didn't get the whole page.
        return RC_ERROR; // Think about using a special error code for getting only part of the page.
    }

    // Remember where we are in the file.
    fHandle->curPagePos = ftell(pageFile);

    // Done with the file, so close it.
    fclose(pageFile);

    return RC_OK;
}

extern int getBlockPos(SM_FileHandle *fHandle) {
    // Make sure we actually have a file to look at.
    if (fHandle == NULL) {
        // If we don't, it's a problem. We should say there's an error in some way.
        // Since we need to give back a number, let's use -1 to show something went wrong.
        fprintf(stderr, "Error: File handle is NULL in getBlockPos.\n");
        return -1; // This means there's been a mistake.
    }

    // Give back where we are in the file.
    return fHandle->curPagePos;
}

extern RC readFirstBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    // Check if the file info and memory spot are okay.
    if (fHandle == NULL || memPage == NULL) {
        return RC_ERROR; // Tell them something's wrong with what they gave us.
    }
    
    // Try to open the file to read from it.
    FILE *pageFile = fopen(fHandle->fileName, "r");
    if (pageFile == NULL) {
        return RC_FILE_NOT_FOUND; // Say we couldn't find the file.
    }

    // Try to get the very first page of the file.
    size_t bytesRead = fread(memPage, sizeof(char), PAGE_SIZE, pageFile);
    if (bytesRead < PAGE_SIZE) {
        // If we didn't get a full page, figure out if it's the end of the file or another error.
        if (feof(pageFile)) {
            fclose(pageFile); // Close the file since we're done trying.
            return RC_READ_NON_EXISTING_PAGE; // Maybe use a better error for "there's no data."
        } else {
            fclose(pageFile); // Make sure to close the file either way.
            return RC_ERROR; // General error for when reading didn't work out.
        }
    }

    // Remember we just looked at the first page.
    fHandle->curPagePos = 0; // Starting spot, so it's 0.

    // All done with the file, so close it.
    fclose(pageFile);

    return RC_OK; // Everything went fine.
}


extern RC readPreviousBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    // Make sure we have everything we need to start.
    if (fHandle == NULL || memPage == NULL) {
        return RC_ERROR; // Tell them something's wrong with the inputs.
    }

    // Figure out where we are in the file.
    int currentPageNumber = fHandle->curPagePos / PAGE_SIZE;

    // Check if we're at the start or even before any content.
    if (currentPageNumber <= 1) {
        printf("\n First block or before: Previous block not present.\n");
        return RC_READ_NON_EXISTING_PAGE; // Say there's no previous page to read.
    } else {
        // Find where the page before the current one starts.
        int startPosition = (PAGE_SIZE * (currentPageNumber - 2));

        // Try to open the file to read from it.
        FILE *pageFile = fopen(fHandle->fileName, "r");
        if (pageFile == NULL) {
            return RC_FILE_NOT_FOUND; // Can't find the file.
        }

        // Go to the beginning of the page before the current one.
        if (fseek(pageFile, startPosition, SEEK_SET) != 0) {
            fclose(pageFile); // Make sure to close the file if this doesn't work.
            return RC_READ_NON_EXISTING_PAGE; // Say we couldn't get to the previous page.
        }

        // Try to get the content of the previous page.
        if (fread(memPage, sizeof(char), PAGE_SIZE, pageFile) < PAGE_SIZE) {
            // If we didn't get a full page, something went wrong.
            fclose(pageFile); // Close the file anyway.
            return RC_ERROR; // General error for reading problems.
        }

        // Note that we've moved back one page.
        fHandle->curPagePos = startPosition + PAGE_SIZE; // We're now at the end of the page we just read.

        // Done with the file, so close it.
        fclose(pageFile);
        return RC_OK; // Everything worked out.
    }
}


extern RC readCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    // Make sure the file handle and memory page are not missing.
    if (fHandle == NULL || memPage == NULL) {
        return RC_ERROR; // Tell them something's wrong because we got bad inputs.
    }

    // Figure out which page we're currently looking at.
    int currentPageNumber = fHandle->curPagePos / PAGE_SIZE;

    // Find out where this page starts in the file.
    int startPosition = PAGE_SIZE * currentPageNumber;
    
    // Try to open the file so we can read from it.
    FILE *pageFile = fopen(fHandle->fileName, "r");
    if (pageFile == NULL) {
        return RC_FILE_NOT_FOUND; // Say we couldn't find the file.
    }

    // Go to where the page we want to read starts.
    if (fseek(pageFile, startPosition, SEEK_SET) != 0) {
        fclose(pageFile); // Close the file if we couldn't get there.
        return RC_READ_NON_EXISTING_PAGE;
    }

    // Now read that page into the memory space we were given.
    size_t readBytes = fread(memPage, sizeof(char), PAGE_SIZE, pageFile);
    if (readBytes < PAGE_SIZE) {
        // If we didn't get the whole page, something went wrong.
        fclose(pageFile); // Close the file anyway.
        if (feof(pageFile)) {
            return RC_READ_NON_EXISTING_PAGE; // End of the file means there's no more to read.
        }
        return RC_ERROR; // Something else went wrong if it's not the end of the file.
    }

    // After reading, remember we're now at the end of this page.
    fHandle->curPagePos = startPosition + PAGE_SIZE;

    // All done, so close the file.
    fclose(pageFile);

    return RC_OK; // Everything went just fine.
}


extern RC readNextBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    // Make sure neither the file handle nor the memory page is missing.
    if (fHandle == NULL || memPage == NULL) {
        return RC_ERROR; // Say there's a problem because we didn't get what we needed.
    }

    // Figure out the number of the page we're currently on.
    int currentPageNumber = fHandle->curPagePos / PAGE_SIZE;

    // Make sure we're not trying to read a page that doesn't exist.
    if (currentPageNumber >= fHandle->totalNumPages - 1) {
        printf("\n Last block: There's no next block to read.\n");
        return RC_READ_NON_EXISTING_PAGE; 
    } else {
        // Find the spot where the next page starts.
        int startPosition = PAGE_SIZE * (currentPageNumber + 1); // This is where the next page begins.

        // Try opening the file to read from it.
        FILE *pageFile = fopen(fHandle->fileName, "r");
        if (pageFile == NULL) {
            return RC_FILE_NOT_FOUND; // Tell them we couldn't find the file.
        }

        // Go to the beginning of the next page.
        if (fseek(pageFile, startPosition, SEEK_SET) != 0) {
            fclose(pageFile); // Make sure to close the file if moving there didn't work.
            return RC_READ_NON_EXISTING_PAGE;
        }

        // Read the page into the provided memory space.
        size_t readBytes = fread(memPage, sizeof(char), PAGE_SIZE, pageFile);
        if (readBytes < PAGE_SIZE) {
            // If we got less than a whole page, something's up.
            fclose(pageFile); // Always close the file, even if there was a problem.
            if (feof(pageFile)) {
                return RC_READ_NON_EXISTING_PAGE; // We've hit the end of the file, so there's nothing more to read.
            }
            return RC_ERROR; // If it's not the end of the file, it's some other read error.
        }

        // Remember where we are now, which is at the beginning of the next page we just read.
        fHandle->curPagePos = startPosition;

        // We're done here, so close the file.
        fclose(pageFile);

        return RC_OK; // Everything worked out fine.
    }
}


extern RC readLastBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    // Make sure the file handle and memory page are valid.
    if (fHandle == NULL || memPage == NULL) {
        return RC_ERROR; // If something is missing, report an error.
    }

    // Check if there are any pages in the file.
    if (fHandle->totalNumPages <= 0) {
        return RC_READ_NON_EXISTING_PAGE; // If there are no pages, there's nothing to read.
    }

    // Find where the last page starts.
    int startPosition = (fHandle->totalNumPages - 1) * PAGE_SIZE;

    // Try to open the file to read from it.
    FILE *pageFile = fopen(fHandle->fileName, "r");
    if (pageFile == NULL) {
        return RC_FILE_NOT_FOUND; // If the file doesn't open, it might not exist.
    }

    // Move to the beginning of the last page.
    if (fseek(pageFile, startPosition, SEEK_SET) != 0) {
        fclose(pageFile); // If we can't move there, close the file and report an error.
        return RC_READ_NON_EXISTING_PAGE;
    }

    // Now, read the last page into the provided memory space.
    size_t readBytes = fread(memPage, sizeof(char), PAGE_SIZE, pageFile);
    if (readBytes < PAGE_SIZE && !feof(pageFile)) {
        // If we didn't get a full page and it's not because we're at the end, it's an error.
        fclose(pageFile); // Close the file before leaving.
        return RC_ERROR; // Tell them something went wrong during reading.
    }

    // Remember where we just read from, marking the start of the last page.
    fHandle->curPagePos = startPosition;

    // We're done with the file, so close it.
    fclose(pageFile);

    return RC_OK; // Everything went as expected.
}


extern RC writeBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {
    // Check if the page number is valid (not too big or small).
    if (pageNum > fHandle->totalNumPages || pageNum < 0)
        return RC_WRITE_FAILED;
    
    // Try to open the file so we can read and write to it.
    FILE *fileHandle = fopen(fHandle->fileName, "r+");
    
    // If the file didn't open, let the user know.
    if(fileHandle == NULL)
        return RC_FILE_NOT_FOUND;

    int offset = pageNum * PAGE_SIZE; // Figure out where to start writing in the file.

    // Prepare to count how much we write.
    int bytesWritten = 0;

    if(pageNum == 0) { 
        // Move to the right spot in the file.
        fseek(fileHandle, offset, SEEK_SET);	
        while (bytesWritten < PAGE_SIZE) {
            if(feof(fileHandle))
                appendEmptyBlock(fHandle); // If we hit the end of the file, add a new empty page.
            
            fputc(memPage[bytesWritten], fileHandle); // Write one character at a time to the file.
            bytesWritten++;
        }

        // Keep track of where we are in the file after writing.
        fHandle->curPagePos = ftell(fileHandle); 

        // Done writing, so close the file to make sure all changes are saved.
        fclose(fileHandle);	
    } else {	
        // Before we finish, remember the file position where we wrote.
        fHandle->curPagePos = offset;
        writeCurrentBlock(fHandle, memPage);
        fclose(fileHandle); // Now we close the file.
    }
    return RC_OK;
}


extern RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    // Try to open the file so we can change it.
    FILE *fileHandle = fopen(fHandle->fileName, "r+");

    // If the file didn't open, tell the user.
    if(fileHandle == NULL)
        return RC_FILE_NOT_FOUND;
    
    // Make sure there's enough room in the file before we start writing.
    appendEmptyBlock(fHandle);

    // Move to the right place in the file where we want to start writing.
    fseek(fileHandle, fHandle->curPagePos, SEEK_SET);
    
    // Figure out how much of memPage we're going to write to make sure we don't write too much.
    size_t contentLength = strlen(memPage);
    // Don't write more than the max page size.
    contentLength = (contentLength > PAGE_SIZE) ? PAGE_SIZE : contentLength;

    // Now write the actual data from memPage to the file.
    fwrite(memPage, sizeof(char), contentLength, fileHandle);
    
    // Remember where we ended up in the file after writing.
    fHandle->curPagePos = ftell(fileHandle);

    // All done writing, so close the file to save everything properly.
    fclose(fileHandle);

    return RC_OK;
}


extern RC appendEmptyBlock (SM_FileHandle *fHandle) {
    // Make a new empty page.
    SM_PageHandle emptyPage = (SM_PageHandle)calloc(PAGE_SIZE, sizeof(char));
    
    // If making the page didn't work, stop and say there was a problem.
    if (!emptyPage) {
        return RC_WRITE_FAILED; // This means we couldn't write because of some error.
    }

    // Try to open the file so we can add to the end of it.
    FILE *file = fopen(fHandle->fileName, "a");
    
    // If we can't open the file, clean up and say the file wasn't found.
    if(file == NULL) {
        free(emptyPage); // Clean up the empty page we made earlier.
        return RC_FILE_NOT_FOUND;
    }
    
    // Add the new empty page to the end of the file.
    size_t written = fwrite(emptyPage, sizeof(char), PAGE_SIZE, file);
    // If we didn't add the whole page, there was a problem.
    if(written < PAGE_SIZE) {
        fclose(file); // Close the file to keep things neat.
        free(emptyPage); // Clean up the empty page.
        return RC_WRITE_FAILED; // Say that we couldn't write properly.
    }
    
    // We're done with the file, so close it.
    fclose(file);

    // We don't need the empty page in memory anymore, so get rid of it.
    free(emptyPage);
    
    // We added a page, so we need to remember that by increasing the total page count.
    fHandle->totalNumPages++;
    return RC_OK;
}


extern RC ensureCapacity (int requiredPages, SM_FileHandle *handle) {
    // Try to open the file so we can read and write to it, without removing anything that's already there.
    FILE *fileAccess = fopen(handle->fileName, "r+b");

    // If we can't open the file that way, try another way that lets us add to it and make sure it's there.
    if (fileAccess == NULL) {
        fileAccess = fopen(handle->fileName, "a+");
        if (fileAccess == NULL) {
            return RC_FILE_NOT_FOUND; // If we still can't open the file, say it wasn't found.
        }
    }

    // Keep adding empty pages to the file until it's as big as we need it to be.
    while (requiredPages > handle->totalNumPages) {
        RC appendStatus = appendEmptyBlock(handle); // Try to add an empty page.
        if (appendStatus != RC_OK) {
            fclose(fileAccess); // If there's a problem, close the file before stopping.
            return appendStatus; // Say what the problem was.
        }
    }

    // Once the file is big enough, close it.
    fclose(fileAccess);
    return RC_OK; // Say everything went okay.
}

