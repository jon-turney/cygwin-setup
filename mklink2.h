
/* This part of the code must be in C because the C++ interface to COM
doesn't work. */

#ifdef __cplusplus
extern "C"
{
#endif
  void make_link_2 (char *exepath, char *args, char *icon, char *lname);

  int mkcygsymlink (const char *from, const char *to);

#ifdef __cplusplus
};
#endif
