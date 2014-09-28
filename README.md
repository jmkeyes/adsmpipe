Tivoli ADSMPipe
===============
Pipe data directly into Tivoli as backup or archive data instead of storing it
to the filesystem; useful for backing up MySQL/PostgreSQL databases to Tivoli
as they are not supported by the Tivoli Data Protection suite.

Dependencies
------------
 - The Tivoli API package (TIVsm-API / TIVsm-API64)
 - GNU Make and a working GCC compiler.
 - (Optional) The `rpmbuild` tool for building RPM packages.
 - (Optional) The `debbuild` tool for building DEB packages.

Instructions
------------
Clone the repository into a fresh directory and run `make`.

    [user@host ~]$ git clone git://github.com/jmkeyes/adsmpipe.git
    Cloning into 'adsmpipe'...
    remote: Counting objects: 26, done.
    remote: Compressing objects: 100% (24/24), done.
    remote: Total 26 (delta 5), reused 0 (delta 0)
    Receiving objects: 100% (26/26), 12.34 KiB, done.
    Resolving deltas: 100% (5/5), done.
    [user@host ~]$ cd adsmpipe
    [user@host adsmpipe]$ make

    Please select a single target platform.
      linux

      make PLATFORM=<platform> all

    [user@host adsmpipe]$ make PLATFORM=linux all
        CC source/adsmblib.o
        CC source/adsmpipe.o
        LD adsmpipe
    [user@host adsmpipe]$ make PLATFORM=linux DESTDIR=/usr/local install
        INSTALL adsmpipe
    [user@host adsmpipe]$ which adsmpipe
    /usr/local/bin/adsmpipe
    [user@host adsmpipe]$ /usr/local/bin/adsmpipe
    Error: you must supply a filename or pattern using -f
    adsmpipe [-[tcxd] -f <filename> [-l<size>] [-v]] [-p <oldpw/newpw/newpw>]

    Creates, extracts or lists files in the ADSM pipe backup area
            -t      Lists files in pipe backup area matching the pattern
            -c      Creates a file in pipe backup area.
                     Data comes from standard input.
            -x      Extracts a file in pipe backup.
                     Data goes to standard output.
            -d      Deletes the file from active store.

            -B      Store data in the backup space of the ADSM server
            -A      Store data in the archive space of the ADSM server
            -f <file>
                    Provides filename for create, and extract operations.
                     For list operations, the filename can be a pattern
            -l <size>
                    Estimated size of data in bytes.
                     This is only needed for create
            -p <oldpw/newpw/newpw>
                    <oldpw/newpw/newpw> Changes password
                     <passwd> Uses passwd for signon
            -v      Verbose
            -m      Overwrite management class (API Version 2.1.2 and higher)
            -n <count>
                    File number to retrieve if multiple versions
            -s <filespace>
                    Specify file space (default "/adsmpipe")
    [user@host adsmpipe]$

Building Distribution-specific Packages
-------------------------
Clone the repository as usual. Run `make` with `PLATFORM` defined, but supply the
`tar`, `rpm`, or `deb` target in place of `all`. The build will place the resulting
package in the project root directory (where the Makefile is) and the packages should
be ready to install. Ensure you have the Tivoli API packages installed or the program
will not function.
