@echo off

set ft_root=..\code\core_layer\renderer\ext\freetype\

set disabled_warnings=-wd4201 -wd4100 -wd4152 -wd4189 -wd4101 -wd4456 -W4 -WX 
set common_defines=-D_CRT_SECURE_NO_WARNINGS -DDEBUG
set common_compiler_flags=%common_defines% -Zi -FC -Od -nologo -DEBUG:FULL
set include_dirs=-I%ft_root%include\ -I%ft_root%

set ft_files=
set ft_files=%ft_files% %ft_root%\src\base\ftsystem.c
set ft_files=%ft_files% %ft_root%\src\base\ftinit.c
set ft_files=%ft_files% %ft_root%\src\base\ftdebug.c
set ft_files=%ft_files% %ft_root%\src\base\ftbase.c
set ft_files=%ft_files% %ft_root%\src\base\ftbitmap.c
set ft_files=%ft_files% %ft_root%\src\base\ftbbox.c
set ft_files=%ft_files% %ft_root%\src\base\ftglyph.c

set ft_files=%ft_files% %ft_root%\src\truetype\truetype.c
set ft_files=%ft_files% %ft_root%\src\sfnt\sfnt.c
set ft_files=%ft_files% %ft_root%\src\raster\raster.c
set ft_files=%ft_files% %ft_root%\src\psnames\psnames.c
set ft_files=%ft_files% %ft_root%\src\gzip\ftgzip.c
set ft_files=%ft_files% %ft_root%\src\smooth\smooth.c

if not exist ..\build mkdir ..\build
pushd ..\build

set common_compiler_flags=%common_compiler_flags% %disabled_warnings% %include_dirs%

cl ..\code\main.c %common_compiler_flags% -DEBUG:FULL -link -incremental:no -subsystem:console gdi32.lib user32.lib kernel32.lib winmm.lib ws2_32.lib

del *.obj

popd