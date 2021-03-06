/* unxUtil.cpp: 

   Copyright (C) 1996-2017 Christian Schenk

   This file is part of the MiKTeX Core Library.

   The MiKTeX Core Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2, or
   (at your option) any later version.

   The MiKTeX Core Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the MiKTeX Core Library; if not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA. */

#include "StdAfx.h"

#include "internal.h"

#include "miktex/Core/Directory.h"
#include "miktex/Core/PathName.h"

#include "Session/SessionImpl.h"

using namespace MiKTeX::Core;
using namespace std;

string Utils::GetOSVersionString()
{
  string version;
#if defined(HAVE_UNAME_SYSCALL)
  struct utsname buf;
  if (uname(&buf) < 0)
  {
    MIKTEX_FATAL_CRT_ERROR("uname");
  }
  version = buf.sysname;
  version += ' ';
  version += buf.release;
  version += ' ';
  version += buf.version;
  version += ' ';
  version += buf.machine;
#else
#warning Unimplemented : Utils::GetOSVersionString
  version = "UnkOS 0.1";
#endif
  return version;
}

void Utils::SetEnvironmentString(const string& valueName, const string& value)
{
  string oldValue;
  if (::GetEnvironmentString(valueName, oldValue) && oldValue == value)
  {
    return;
  }
  SessionImpl::GetSession()->trace_config->WriteFormattedLine("core", T_("setting env %s=%s"), valueName.c_str(), value.c_str());
  if (setenv(valueName.c_str(), value.c_str(), 1) != 0)
  {
    MIKTEX_FATAL_CRT_ERROR_2("setenv", "name", valueName);
  }
}

void Utils::CheckHeap()
{
}

void Utils::CanonicalizePathName(PathName& path)
{
  char resolved[PATH_MAX + 1];
  if (realpath(path.GetData(), resolved) == nullptr)
  {
    if (errno == ENOENT)
    {
      return;
    }
    MIKTEX_FATAL_CRT_ERROR_2("realpath", "path", path.ToString());
  }
  path = resolved;
}

void Utils::ShowWebPage(const string& url)
{
  UNIMPLEMENTED();
}

bool Utils::SupportsHardLinks(const PathName& path)
{
  return true;
}

bool Utils::CheckPath(bool repair)
{
#if 1
  // TODO
  if (repair)
  {
    UNIMPLEMENTED();
  }
#endif
  
  shared_ptr<Session> session = Session::Get();

  string envPath;
  if (!Utils::GetEnvironmentString("PATH", envPath))
  {
    return false;
  }
  
  PathName localBinDir = session->GetSpecialPath(SpecialPath::LocalBinDirectory);

  string repairedPath;
  bool pathCompetition;
  
  bool pathOkay = !Directory::Exists(localBinDir) || !FixProgramSearchPath(envPath, localBinDir, true, repairedPath, pathCompetition);

  bool repaired = false;

  if (!pathOkay && !repair)
  {
    SessionImpl::GetSession()->trace_error->WriteLine("core", T_("Something is wrong with the PATH:"));
    SessionImpl::GetSession()->trace_error->WriteLine("core", envPath.c_str());
  }
  else if (!pathOkay && repair)
  {
    SessionImpl::GetSession()->trace_error->WriteLine("core", T_("Setting new PATH:"));
    SessionImpl::GetSession()->trace_error->WriteLine("core", repairedPath.c_str());
    envPath = repairedPath;
    if (session->IsAdminMode())
    {
      // TODO: edit system configuration
    }
    else
    {
      // TODO: edit user configuration
    }
    pathOkay = true;
    repaired = true;
  }
  return repaired || pathOkay;
}
