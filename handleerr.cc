/***************************************************************
* HandleError.cc - routines to dosplay error messages
***************************************************************/

#if __ORCAC__
#pragma lint -1
#pragma noroot
#endif

#if DEBUG
#pragma debug 25
#endif

#include <types.h>
#include <Resources.h>
#include <Window.h>
#include <Memory.h>

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

char *GetString (int resourceID)
{
Handle hndl;                            /* resource handle */

hndl = LoadResource(rCString, resourceID);
if (toolerror() == 0) {
	HLock(hndl);
	return (char *) (*hndl);
	}
return NULL;
}

/***************************************************************
* FreeString - Free a resource string
* Parameters:
*    resourceID - resource ID of the rCString to free
***************************************************************/

void FreeString (int resourceID)
{
ReleaseResource(-3, rCString, resourceID);
}

/***************************************************************
* FlagError - Flag an error
* Parameters:
*    error - error message number
*    tError - toolbox error code; 0 if none
***************************************************************/

void FlagError (int error, int tError)
{
#define errorAlert 2000                 /* alert resource ID */
#define errorBase 2000                  /* base resource ID for fortunes */

char *substArray;                       /* substitution "array" */
char *errorString;                      /* pointer to the error string */

                                        /* form the error string */
errorString = GetString(errorBase + error);
substArray = NULL;
if (errorString != NULL) {
	substArray = (char *)malloc(strlen(substArray)+9);
	if (substArray != NULL)
		strcpy(substArray, errorString);
	FreeString(errorBase + error);
	}
if (substArray != NULL) {
	if (tError != 0)                     /* add the tool error number */
		sprintf(&substArray[strlen(substArray)], " ($%04X)", tError);
                                        /* show the alert */
	AlertWindow(awCString+awResource, (Pointer) &substArray, errorAlert);
	free(substArray);
	}

#undef errorAlert
#undef errorBase
}
