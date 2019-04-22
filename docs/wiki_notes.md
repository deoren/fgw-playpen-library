These are my notes for **You Can Program in C++: A Programmer's
Introduction** [^1].

About the book
--------------

I'll attempt to give proper remarks for this book later, but suffice to
say this book is *well* worth your time if you're learning C++. I like
how Francis pays particular attention to the pitfalls of the language.
He teaches you to always check for failing function calls (something a
lot of “beginner books” fail to do) and teaches you how to use strings
and vectors from the beginning in place of arrays.

What if you don't have the cd?
------------------------------

Before purchasing a copy of the book & CD, I checked out a copy of the
book from a library and did not have the CD. This was ok because at the
time the author made the contents available from his website. [^2]
Unfortunately [the author's website hasn't been operational for some
time](http://www.whyaskwhy.org/blog/1176/), however on the same note the
publisher *has* newer content available for download. [^3]

At the time the author's website had a broken link to Quincy 2002, [^4]
but I was able to find Quincy 2005 [^5] which is a newer version of it.
Thankfully it is still available for download for those that want to use
it. Since Francis Glassborow made the source for the **FGW Library**
(aka, “Playpen”) available, we can compile it using newer versions of
[MinGW](wikipedia:Mingw "wikilink") or different compilers altogether.
That's great because I like to use
[Netbeans](wikipedia:Netbeans "wikilink") and the current MinGW and plan
on trying out Visual Studio as I'm going through the book.

### Preparing to build the library

#### (Optional) Install Quincy

If you plan to use Quincy 2005, download it [^6] then proceed with the
rest of these instructions.

#### Arranging the directory structure

1.  Download the `windows_tutorial.zip` file from the publisher's
    website for the book [^7] and extract to a temporary location.
2.  Move all folders inside of `windows_tutorial` to `C:\tutorial`.
3.  Move all files from the `C:\tutorial\source` directory to the
    `C:\tutorial\fgw_headers` directory, choosing to overwrite existing
    files.
4.  Remove the `C:\tutorial\source` directory (should have nothing
    inside of it).
5.  Remove the `C:\tutorial\libraries` directory as we're going to build
    a newer library file. Aside from the volume serial number, you
    should see the following:

<!-- -->

    C:\tutorial>tree /F
    Folder PATH listing
    Volume serial number is 38B9-1B1A
    C:.
    ├───chapter_1
    ├───chapter_10
    ├───chapter_11
    ├───chapter_12
    ├───chapter_13
    ├───chapter_14
    ├───chapter_15
    ├───chapter_16
    ├───chapter_17
    ├───chapter_18
    ├───chapter_2
    ├───chapter_3
    ├───chapter_4
    ├───chapter_5
    ├───chapter_6
    ├───chapter_7
    ├───chapter_8
    ├───chapter_9
    └───fgw_headers
            adler32.c
            colournames.h
            deflate.c
            deflate.h
            fgw_text.h
            flood_fill.cpp
            flood_fill.h
            infblock.c
            infblock.h
            infcodes.c
            infcodes.h
            inffast.c
            inffast.h
            inffixed.h
            inflate.c
            inftrees.c
            inftrees.h
            infutil.c
            infutil.h
            keyboard.h
            libfgw.a
            line_drawing.cpp
            line_drawing.h
            makefile
            minipng.cpp
            minipng.h
            mouse.h
            playpen.cpp
            playpen.h
            point2d.cpp
            point2d.h
            point2dx.cpp
            point2dx.h
            shape.cpp
            shape.h
            trees.c
            trees.h
            winplaypen.gpj
            zconf.h
            zlib.h
            zutil.c
            zutil.h

##### (Optional) Checkout a copy of the source code from public Subversion respository

As an alternative to downloading the zip file and arranging the
directories as mentioned in the preceding instructions, you can check
out a copy of the source code from an unofficial public Subversion
repository. Provided you have Subversion installed, you should end up
with the same content in `C:\tutorial` as you would downloading the zip
file from the publisher's site and arranging the directory content.

1.  Open a command prompt
2.  `mkdir C:\tutorial`
3.  `cd C:\tutorial`
4.  `svn co `[`http://projects.whyaskwhy.org/svn/fgw_library/trunk/`](http://projects.whyaskwhy.org/svn/fgw_library/trunk/)` .`
5.  `prep.bat`

#### Updating your System Path variable

You should only follow the directions for one of these, not both.

##### Quincy 2005

If you're using Quincy 2005 [^8], modify your PATH by following these
directions.

1.  Open a command prompt (Start-&gt;Run-&gt;cmd-&gt;Enter)
2.  `set path=%path%;C:\Program Files\quincy\MinGW\bin`

##### Current MinGW & MSYS release

1.  Open a command prompt (Start-&gt;Run-&gt;cmd-&gt;Enter)
2.  `set PATH=%PATH%;C:\MinGW\bin;C:\MinGW\msys\1.0\bin`

#### Copy libgdi32.a to fgw\_headers directory

##### Quincy 2005

Copy `C:\Program Files\quincy\MinGW\lib\libgdi32.a` to
`C:\tutorial\fgw_headers\libgdi32.a` and replace any existing copy if it
exists.

##### Current MinGW & MSYS release

Copy `C:\MinGW\lib\libgdi32.a` to `C:\tutorial\fgw_headers\libgdi32.a`
and replace any existing copy if it exists.

### Building the library

1.  Open a command prompt
2.  `cd C:\tutorial\fgw_headers`
3.  `rmdir /s /q Release`
4.  `mkdir Release`
5.  `bash`
6.  `make clean`
7.  `make`
8.  `cp Release/libfgw.a .`

### Using the new library

Every time the book tells you to add `fgwlib.a` to your project,
reference `C:\tutorial\fgw_headers\libfgw.a` instead.

References
----------

<references />
<Category:Programming> <Category:Software> <Category:Books>

[^1]: ISBN 0470449586

[^2]: [Official website for this
    book](http://www.spellen.org/youcandoit/resources.htm)

[^3]: [Publisher's book
    page](http://www.wiley.com/legacy/wileychi/glassborowc++/material.html)

[^4]: [Quincy 2002 homepage](http://alstevens.com/quincy.html)

[^5]: [Quincy 2005 homepage](http://www.codecutter.net/tools/quincy/)

[^6]:

[^7]:

[^8]:
