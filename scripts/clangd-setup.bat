@ECHO OFF

set CurrDirName=%cd%

(
    echo CompileFlags:
    echo   Add:
    echo     - -I%CurrDirName%\include
) > .clangd