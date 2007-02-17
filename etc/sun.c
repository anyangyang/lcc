/* SPARCs running SunOS 4.1.3 at CS Dept., Princeton University */

#include <string.h>

static char rcsid[] = "$Id: sun.c,v 1.9 1997/05/26 22:34:11 drh Exp $";

#ifndef LCCDIR
#define LCCDIR "/usr/local/lib/lcc/"
#endif

char *suffixes[] = { ".c", ".i", ".s", ".o", ".out", 0 };
char inputs[256] = "";
char *cpp[] = { LCCDIR "cpp",
	"-D__STDC__=1", "-Dsparc", "-Dsun", "-Dunix", "$1", "$2", "$3", 0 };
char *include[] = { "-I" LCCDIR "include",
	 "-I/usr/local/include", "-I/usr/include", 0 };
char *com[] = { LCCDIR "rcc", "-target=sparc-sun",
	"$1", "$2", "$3", "", 0 };
char *as[] = { "/bin/as", "-o", "$3", "$1", "$2", 0 };
char *ld[] = { "/bin/ld", "-o", "$3", "-dc", "-dp", "-e", "start", "-X",
	"$1", "/usr/lib/crt0.o", "", "", "$2", "", "",
	"-L" LCCDIR, "-llcc", "-lm", "", "-lc", 0 };
static char *libprefix = "/cmnusr/local/lib/ldb/";

extern char *concat(char *, char *);
extern int access(const char *, int);

int option(char *arg) {
	if (strncmp(arg, "-lccdir=", 8) == 0) {
		cpp[0] = concat(&arg[8], "/cpp");
		include[0] = concat("-I", concat(&arg[8], "/include"));
		com[0] = concat(&arg[8], "/rcc");
		ld[15] = concat("-L", &arg[8]);
	} else if (strcmp(arg, "-g4") == 0
	&& access("/u/drh/lib/sparc/rcc", 4) == 0
	&& access("/u/drh/book/cdb/sparc/sun/cdbld", 4) == 0) {
		com[0] = "/u/drh/lib/sparc/rcc";
		com[5] = "-g4";
		ld[0] = "/u/drh/book/cdb/sparc/sun/cdbld";
		ld[1] = "-o";
		ld[2] = "$3";
		ld[3] = "$1";
		ld[4] = "$2";
		ld[5] = 0;
	} else if (strcmp(arg, "-g") == 0)
		ld[16] = "-lg";
	else if (strcmp(arg, "-p") == 0
	&& strcmp(ld[9], "/usr/lib/crt0.o") == 0) {
		ld[9]  = "/usr/lib/mcrt0.o";
		ld[19] = "-lc_p";
	} else if (strcmp(arg, "-pg") == 0
	&& strcmp(ld[9], "/usr/lib/crt0.o") == 0) {
		ld[9]  = "/usr/lib/gcrt0.o";
		ld[19] = "-lc_p";
	} else if (strcmp(arg, "-b") == 0)
		;
	else if (strncmp(arg, "-B", 2) == 0)
		libprefix = arg + 2;
	else if (strcmp(arg, "-G") == 0
	&& strcmp(ld[9], "/usr/lib/crt0.o") == 0) {
	        com[0] = concat(libprefix, "sparc/rcc");
		com[1] = "";
		ld[0]  = "/cmnusr/local/lib/ldb/ldb-ld";
   	        ld[9]  = concat(libprefix, "sparc/crt0.o");
		ld[14] = concat(libprefix, "sparc/Cnub.o");
	} else if (strncmp(arg, "ld=", 3) == 0)
	        ld[0]  = &arg[3];
	else
		return 0;
	return 1;
}