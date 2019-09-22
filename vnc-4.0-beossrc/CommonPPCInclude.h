/* Special file included before all headers in PowerPC builds,
to compensate for the lack of a std:: namespace in that old
Metrowerks compiler for BeOS.  AGMS20130423 */

#if defined(__POWERPC__) && defined(__cplusplus)
  namespace std {
    extern int ___foobar___; // Dummy to make the namespace nonempty.
  }
  #define std
#endif
