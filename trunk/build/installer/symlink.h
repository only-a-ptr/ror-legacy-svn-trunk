#if PLATFORM == PLATFORM_WINDOWS

// from http://msdn.microsoft.com/en-us/library/bb776891(VS.85).aspx
#ifndef SYMLINK_H__
#define SYMLINK_H__

/* Windows headers come first */
#ifndef STRICT
#define STRICT
#endif
#include <windows.h>

/* C run time library headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wx/string.h>

/* COM headers (requires shell32.lib, ole32.lib, uuid.lib) */
#include <objbase.h>
#include <shlobj.h>

// CreateLink - uses the Shell's IShellLink and IPersistFile interfaces 
//              to create and store a shortcut to the specified object. 
//
// Returns the result of calling the member functions of the interfaces. 
//
// Parameters:
// lpszPathObj  - address of a buffer containing the path of the object. 
// lpszPathLink - address of a buffer containing the path where the 
//                Shell link is to be stored. 
// lpszDesc     - address of a buffer containing the description of the 
//                Shell link. 
HRESULT createLink2(LPCWSTR lpszPathObj, LPCWSTR lpszPathLink, LPCWSTR lpszDesc);

// small wxwidgets wrapper
int createLink(std::string target_str, std::string filename_str, std::string description_str);

#endif //MAKELINK_H__

#endif //PLATFORM 
