/*
 * Copyright (c) 2007 Brian Dessent
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Brian Dessent <brian@dessent.net>
 *
 */

#include "win32.h"
#include <memory>
#include <malloc.h>
#include "LogFile.h"
#include "resource.h"
#include "state.h"
#include "ini.h"
#include <sys/stat.h>

NTSecurity nt_sec;

#define ALL_INHERIT_ACE	(CONTAINER_INHERIT_ACE \
			 | OBJECT_INHERIT_ACE \
			 | INHERIT_ONLY_ACE)
#define NO_INHERIT_ACE  (0)

PSECURITY_DESCRIPTOR
NTSecurity::GetPosixPerms (const char *fname, PSID owner_sid, PSID group_sid,
			   mode_t mode, SECURITY_DESCRIPTOR &out_sd, acl_t &acl)
{
  DWORD u_attribute, g_attribute, o_attribute;
  DWORD offset = 0;

  /* Initialize out SD */
  if (!InitializeSecurityDescriptor (&out_sd, SECURITY_DESCRIPTOR_REVISION))
    Log (LOG_TIMESTAMP) << "InitializeSecurityDescriptor(" << fname
    			<< ") failed: " << GetLastError () << endLog;
  out_sd.Control |= SE_DACL_PROTECTED;

  /* Initialize ACL and fill with almost POSIX-like permissions.
     Note that the current user always requires write permissions, otherwise
     creating files in directories with restricted permissions fails. */
  if (!InitializeAcl (&acl.acl , sizeof acl, ACL_REVISION))
    Log (LOG_TIMESTAMP) << "InitializeAcl(" << fname << ") failed: "
    			<< GetLastError () << endLog;
  /* USER */
  /* Default user to current user. */
  if (!owner_sid)
    owner_sid = ownerSID.user.User.Sid;
  u_attribute = STANDARD_RIGHTS_ALL | FILE_GENERIC_READ | FILE_GENERIC_WRITE;
  if (mode & 0100) // S_IXUSR
    u_attribute |= FILE_GENERIC_EXECUTE;
  if ((mode & 0300) == 0300) // S_IWUSR | S_IXUSR
    u_attribute |= FILE_DELETE_CHILD;
  if (!AddAccessAllowedAceEx (&acl.acl, ACL_REVISION, NO_INHERIT_ACE,
			      u_attribute, owner_sid))
    Log (LOG_TIMESTAMP) << "AddAccessAllowedAceEx(" << fname
    			<< ", owner) failed: " << GetLastError () << endLog;
  else
    offset++;
  /* GROUP */
  /* Default group to current primary group. */
  if (!group_sid)
    group_sid = groupSID;
  g_attribute = STANDARD_RIGHTS_READ | FILE_READ_ATTRIBUTES;
  if (mode & 0040) // S_IRGRP
    g_attribute |= FILE_GENERIC_READ;
  if (mode & 0020) // S_IWGRP
    g_attribute |= FILE_GENERIC_WRITE;
  if (mode & 0010) // S_IXGRP
    g_attribute |= FILE_GENERIC_EXECUTE;
  if ((mode & 01030) == 00030) // S_IWGRP | S_IXGRP, !S_ISVTX
    g_attribute |= FILE_DELETE_CHILD;
  if (!AddAccessAllowedAceEx (&acl.acl, ACL_REVISION, NO_INHERIT_ACE,
			      g_attribute, group_sid))
    Log (LOG_TIMESTAMP) << "AddAccessAllowedAceEx(" << fname
    			<< ", group) failed: " << GetLastError () << endLog;
  else
    offset++;
  /* OTHER */
  o_attribute = STANDARD_RIGHTS_READ | FILE_READ_ATTRIBUTES;
  if (mode & 0004) // S_IROTH
    o_attribute |= FILE_GENERIC_READ;
  if (mode & 0002) // S_IWOTH
    o_attribute |= FILE_GENERIC_WRITE;
  if (mode & 0001) // S_IXOTH
    o_attribute |= FILE_GENERIC_EXECUTE;
  if ((mode & 01003) == 00003) // S_IWOTH | S_IXOTH, !S_ISVTX
    o_attribute |= FILE_DELETE_CHILD;
  if (!AddAccessAllowedAceEx (&acl.acl, ACL_REVISION, NO_INHERIT_ACE,
			      o_attribute, everyOneSID.theSID ()))
    Log (LOG_TIMESTAMP) << "AddAccessAllowedAceEx(" << fname
    			<< ", everyone) failed: " << GetLastError () << endLog;
  else
    offset++;
  if (mode & 07000) /* At least one of S_ISUID, S_ISGID, S_ISVTX */
    {
      DWORD attribute = 0;
      if (mode & 04000) // S_ISUID
      	attribute |= FILE_APPEND_DATA;
      if (mode & 02000) // S_ISGID
      	attribute |= FILE_WRITE_DATA;
      if (mode & 01000) // S_ISVTX
      	attribute |= FILE_READ_DATA;
      if (!AddAccessAllowedAceEx (&acl.acl, ACL_REVISION, NO_INHERIT_ACE,
				  attribute, nullSID.theSID ()))
	Log (LOG_TIMESTAMP) << "AddAccessAllowedAceEx(" << fname
			    << ", null) failed: " << GetLastError () << endLog;
      else
	offset++;
    }
  /* For directories, we also add inherit-only ACEs for CREATOR OWNER,
     CREATOR GROUP, and EVERYONE (aka OTHER). */
  if (mode & S_IFDIR)
    {
      if (mode & 01000) // S_ISVTX
	{
	  /* Don't allow default write permissions for group and other
	     in a S_ISVTX dir. */
	  /* GROUP */
	  g_attribute = STANDARD_RIGHTS_READ | FILE_READ_ATTRIBUTES;
	  if (mode & 0040) // S_IRGRP
	    g_attribute |= FILE_GENERIC_READ;
	  if (mode & 0010) // S_IXGRP
	    g_attribute |= FILE_GENERIC_EXECUTE;
	  /* OTHER */
	  o_attribute = STANDARD_RIGHTS_READ | FILE_READ_ATTRIBUTES;
	  if (mode & 0004) // S_IROTH
	    o_attribute |= FILE_GENERIC_READ;
	  if (mode & 0001) // S_IXOTH
	    o_attribute |= FILE_GENERIC_EXECUTE;
	}
      if (!AddAccessAllowedAceEx (&acl.acl, ACL_REVISION, ALL_INHERIT_ACE,
				  u_attribute, cr_ownerSID.theSID ()))
	Log (LOG_TIMESTAMP) << "AddAccessAllowedAceEx(" << fname
			    << ", creator owner) failed: "
			    << GetLastError () << endLog;
      else
	offset++;
      if (!AddAccessAllowedAceEx (&acl.acl, ACL_REVISION, ALL_INHERIT_ACE,
				  g_attribute, cr_groupSID.theSID ()))
	Log (LOG_TIMESTAMP) << "AddAccessAllowedAceEx(" << fname
			    << ", creator group) failed: "
			    << GetLastError () << endLog;
      else
	offset++;
      if (!AddAccessAllowedAceEx (&acl.acl, ACL_REVISION, ALL_INHERIT_ACE,
				  o_attribute, everyOneSID.theSID ()))
	Log (LOG_TIMESTAMP) << "AddAccessAllowedAceEx(" << fname
			    << ", everyone inherit) failed: "
			    << GetLastError () << endLog;
      else
	offset++;
    }

  /* Set SD's DACL to just created ACL. */
  if (!SetSecurityDescriptorDacl (&out_sd, TRUE, &acl.acl, FALSE))
    Log (LOG_TIMESTAMP) << "SetSecurityDescriptorDacl(" << fname
    			<< ") failed: " << GetLastError () << endLog;
  return &out_sd;
}

void
NTSecurity::NoteFailedAPI (const std::string &api)
{
  Log (LOG_TIMESTAMP) << api << "() failed: " << GetLastError () << endLog;
}

void
NTSecurity::initialiseWellKnownSIDs ()
{
  SID_IDENTIFIER_AUTHORITY n_sid_auth = { SECURITY_NULL_SID_AUTHORITY };
  /* Get the SID for "NULL" S-1-0-0 */
  if (!AllocateAndInitializeSid (&n_sid_auth, 1, SECURITY_NULL_RID,
				 0, 0, 0, 0, 0, 0, 0, &nullSID.theSID ()))
      return;
  SID_IDENTIFIER_AUTHORITY e_sid_auth = { SECURITY_WORLD_SID_AUTHORITY };
  /* Get the SID for "Everyone" S-1-1-0 */
  if (!AllocateAndInitializeSid (&e_sid_auth, 1, SECURITY_WORLD_RID,
				 0, 0, 0, 0, 0, 0, 0, &everyOneSID.theSID ()))
      return;
  SID_IDENTIFIER_AUTHORITY nt_sid_auth = { SECURITY_NT_AUTHORITY };
  /* Get the SID for "Administrators" S-1-5-32-544 */
  if (!AllocateAndInitializeSid (&nt_sid_auth, 2, SECURITY_BUILTIN_DOMAIN_RID,
				 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
				 &administratorsSID.theSID ()))
      return;
  /* Get the SID for "Users" S-1-5-32-545 */
  if (!AllocateAndInitializeSid (&nt_sid_auth, 2, SECURITY_BUILTIN_DOMAIN_RID,
				 DOMAIN_ALIAS_RID_USERS, 0, 0, 0, 0, 0, 0,
				 &usersSID.theSID ()))
      return;
  SID_IDENTIFIER_AUTHORITY c_sid_auth = { SECURITY_CREATOR_SID_AUTHORITY };
  /* Get the SID for "CREATOR OWNER" S-1-3-0 */
  if (!AllocateAndInitializeSid (&c_sid_auth, 1, SECURITY_CREATOR_OWNER_RID,
				 0, 0, 0, 0, 0, 0, 0, &cr_ownerSID.theSID ()))
      return;
  /* Get the SID for "CREATOR GROUP" S-1-3-1 */
  if (!AllocateAndInitializeSid (&c_sid_auth, 1, SECURITY_CREATOR_GROUP_RID,
				 0, 0, 0, 0, 0, 0, 0, &cr_groupSID.theSID ()))
      return;
  wellKnownSIDsinitialized (true);
}

void
NTSecurity::setDefaultDACL ()
{
  /* To assure that the created files have a useful ACL, the
     default DACL in the process token is set to full access to
     everyone. This applies to files and subdirectories created
     in directories which don't propagate permissions to child
     objects.
     To assure that the files group is meaningful, a token primary
     group of None is changed to Users or Administrators.
     This is the fallback if real POSIX permissions don't
     work for some reason. */

  /* Create a buffer which has enough room to contain the TOKEN_DEFAULT_DACL
     structure plus an ACL with one ACE.  */
  size_t bufferSize = sizeof (ACL) + sizeof (ACCESS_ALLOWED_ACE)
                      + GetLengthSid (everyOneSID.theSID ()) - sizeof (DWORD);

  std::unique_ptr<char[]> buf (new char[bufferSize]);

  /* First initialize the TOKEN_DEFAULT_DACL structure.  */
  PACL dacl = (PACL) buf.get ();

  /* Initialize the ACL for containing one ACE.  */
  if (!InitializeAcl (dacl, bufferSize, ACL_REVISION))
    {
      NoteFailedAPI ("InitializeAcl");
      return;
    }

  /* Create the ACE which grants full access to "Everyone" and store it
     in dacl.  */
  if (!AddAccessAllowedAceEx (dacl, ACL_REVISION, NO_INHERIT_ACE,
			      GENERIC_ALL, everyOneSID.theSID ()))
    {
      NoteFailedAPI ("AddAccessAllowedAceEx");
      return;
    }

  /* Set the default DACL to the above computed ACL. */
  if (!SetTokenInformation (token.theHANDLE(), TokenDefaultDacl, &dacl,
                            bufferSize))
    NoteFailedAPI ("SetTokenInformation");
}

void
NTSecurity::setBackupPrivileges ()
{
  LUID backup, restore;
  if (!LookupPrivilegeValue (NULL, SE_BACKUP_NAME, &backup))
      NoteFailedAPI ("LookupPrivilegeValue");
  else if (!LookupPrivilegeValue (NULL, SE_RESTORE_NAME, &restore))
      NoteFailedAPI ("LookupPrivilegeValue");
  else
    {
      PTOKEN_PRIVILEGES new_privs;

      new_privs = (PTOKEN_PRIVILEGES) alloca (sizeof (TOKEN_PRIVILEGES)
					      + sizeof (LUID_AND_ATTRIBUTES));
      new_privs->PrivilegeCount = 2;
      new_privs->Privileges[0].Luid = backup;
      new_privs->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
      new_privs->Privileges[1].Luid = restore;
      new_privs->Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;
      if (!AdjustTokenPrivileges (token.theHANDLE (), FALSE, new_privs,
				  0, NULL, NULL))
	NoteFailedAPI ("AdjustTokenPrivileges");
      else if (GetLastError () == ERROR_NOT_ALL_ASSIGNED)
	Log (LOG_TIMESTAMP) << "User has NO backup/restore rights" << endLog;
      else
	Log (LOG_TIMESTAMP) << "User has backup/restore rights" << endLog;
    }
}

void
NTSecurity::resetPrimaryGroup ()
{
  if (primaryGroupSID.pgrp.PrimaryGroup)
    {
      Log (LOG_TIMESTAMP) << "Changing gid back to original" << endLog;
      if (!SetTokenInformation (token.theHANDLE (), TokenPrimaryGroup,
				&primaryGroupSID, sizeof primaryGroupSID))
	NoteFailedAPI ("SetTokenInformation");
    }
}

void
NTSecurity::setAdminGroup ()
{
  TOKEN_PRIMARY_GROUP tpg;

  tpg.PrimaryGroup = administratorsSID.theSID ();
  Log (LOG_TIMESTAMP) << "Changing gid to Administrators" << endLog;
  if (!SetTokenInformation (token.theHANDLE (), TokenPrimaryGroup,
			    &tpg, sizeof tpg))
    NoteFailedAPI ("SetTokenInformation");
  else
    groupSID = administratorsSID.theSID ();
}

void
NTSecurity::setDefaultSecurity ()
{
  /* Get the processes access token. */
  if (!OpenProcessToken (GetCurrentProcess (),
			 TOKEN_READ | TOKEN_ADJUST_DEFAULT
			 | TOKEN_ADJUST_PRIVILEGES, &token.theHANDLE ()))
    {
      NoteFailedAPI ("OpenProcessToken");
      return;
    }

  /* Set backup and restore privileges if available. */
  setBackupPrivileges ();

  /* If initializing the well-known SIDs didn't work, we're finished here. */
  if (!wellKnownSIDsinitialized ())
    return;

  /* Set the default DACL to all permissions for everyone as a fallback. */
  setDefaultDACL ();

  /* Get the user */
  if (!GetTokenInformation (token.theHANDLE (), TokenUser, &ownerSID,
			    sizeof ownerSID, &size))
    {
      NoteFailedAPI ("GetTokenInformation(user)");
      return;
    }
  /* Make it the owner */
  TOKEN_OWNER owner = { ownerSID.user.User.Sid };
  if (!SetTokenInformation (token.theHANDLE (), TokenOwner, &owner,
			    sizeof owner))
    {
      NoteFailedAPI ("SetTokenInformation(owner)");
      return;
    }
  /* Get original primary group.  The token's primary group will be reset
     to the original group right before we call the postinstall scripts.
     This is necessary, otherwise, if the installing user is a domain user,
     the group information created by the postinstall calls to `mkpasswd -c,
     mkgroup -c' will be plain wrong. */
  if (!GetTokenInformation (token.theHANDLE (), TokenPrimaryGroup,
			    &primaryGroupSID, sizeof primaryGroupSID, &size))
    {
      NoteFailedAPI("GetTokenInformation(pgrp)");
      primaryGroupSID.pgrp.PrimaryGroup = (PSID) NULL;
    }
  groupSID = primaryGroupSID.pgrp.PrimaryGroup;
  /* Try to set the primary group to the Administrators group, but only if
     "Install for all users" has been chosen.  If it doesn't work, we're
     no admin and that's all there's to say about it. */
  if (root_scope == IDC_ROOT_SYSTEM)
    setAdminGroup ();
}

bool
NTSecurity::isRunAsAdmin ()
{
  BOOL is_run_as_admin = FALSE;
  if (!CheckTokenMembership(NULL, administratorsSID.theSID (), &is_run_as_admin))
    NoteFailedAPI("CheckTokenMembership(administratorsSID)");
  return (is_run_as_admin == TRUE);
}


VersionInfo::VersionInfo ()
{
  v.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
  if (GetVersionEx (&v) == 0)
    {
      Log (LOG_PLAIN) << "GetVersionEx () failed: " << GetLastError ()
                      << endLog;

      /* If GetVersionEx fails we really should bail with an error of some kind,
         but for now just assume we're on NT and continue.  */
      v.dwPlatformId = VER_PLATFORM_WIN32_NT;
    }
}

/* This is the Construct on First Use idiom to avoid static initialization
   order problems.  */
VersionInfo& GetVer ()
{
  static VersionInfo *vi = new VersionInfo ();
  return *vi;
}
