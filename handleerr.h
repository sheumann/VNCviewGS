/***************************************************************
* HandleError.h - header file for error handling routines
***************************************************************/

/***************************************************************
* GetString - Get a string from the resource fork
* Parameters:
*    resourceID - resource ID of the rCString resource
* Returns: pointer to the string; NULL for an error
* Notes: The string is in a locked resource handle.  The caller
*    should call FreeString when the string is no longer needed.
*    Failure to do so is not catastrophic; the memory will be
*    deallocated when the program is shut down.
***************************************************************/

extern char *GetString (int resourceID);

/***************************************************************
* FreeString - Free a resource string
* Parameters:
*    resourceID - resource ID of the rCString to free
***************************************************************/

extern void FreeString (int resourceID);

/***************************************************************
* FlagError - Flag an error
* Parameters:
*    error - error message number
*    tError - toolbox error code; 0 if none
***************************************************************/

extern void FlagError (int error, int tError);
