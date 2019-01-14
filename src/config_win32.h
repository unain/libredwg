#ifndef CONFIG_WIN32_H
#define CONFIG_WIN32_H
#define PACKAGE_STRING "libredwg"
#define PACKAGE_NAME "libredwg"
#define PACKAGE_VERSION 0.7

//#define restrict __restrict
//#define S_ISREG S_ISREG
#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG) 
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG) 
#endif 

#endif