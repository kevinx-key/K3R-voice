/*
  afxres.h shim -- K3R-voice build compatibility header.

  Upstream Weasel's .rc files include "afxres.h", which ships with the MFC
  component of Visual Studio.  This fork builds against the ATL-only /
  Desktop-C++ workload, where MFC is not installed, so we provide the small
  set of resource symbols that the .rc files actually need.

  This file is intentionally identical in effect to <winres.h> from the
  Windows SDK, which is the documented non-MFC equivalent.
*/
#ifndef _AFXRES_H
#define _AFXRES_H

#include <winres.h>

#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

#endif /* _AFXRES_H */
