:: $Id$
:: $HeadURL$

:: Purpose: Setup directory structure just like unzipping the 
::          contents of windows_tutorial.zip would do

@echo off

set SVN_REPO=http://projects.whyaskwhy.org/svn/fgw_library/trunk

echo Creating chapter directories ...

for %%f in (
chapter_1
chapter_10
chapter_11
chapter_12
chapter_13
chapter_14
chapter_15
chapter_16
chapter_17
chapter_18
chapter_2
chapter_3
chapter_4
chapter_5
chapter_6
chapter_7
chapter_8
chapter_9
) do mkdir "%%f"

echo Checking out the unofficial public repository contents ...

svn co --force %SVN_REPO%/docs .
svn co --force %SVN_REPO%/fgw_headers .
