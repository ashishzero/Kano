#!/bin/bash

which g++ &> /dev/null
if [ $? == 0 ]; then
    COMPILER=g++
else
    which clang++ &> /dev/null
    if [ $? == 0 ]; then
        COMPILER=clang++
    else
        echo Error: Both GCC and CLANG compiler not detected.
        exit 
    fi
fi

mkdir -p bin

${COMPILER} -g -std=c++17 -DKANO_SERVER -DASSERTION_HANDLED Main.cpp Server.cpp Lexer.cpp Parser.cpp Resolver.cpp Printer.cpp StringBuilder.cpp Interp.cpp ./Kr/KrCommon.cpp ./Kr/KrBasic.cpp -o bin/Kano -lpthread
${COMPILER} -g -std=c++17 -DASSERTION_HANDLED Compiler.cpp Lexer.cpp Parser.cpp Resolver.cpp Printer.cpp StringBuilder.cpp Interp.cpp ./Kr/KrCommon.cpp ./Kr/KrBasic.cpp -o bin/kanoc -lpthread
