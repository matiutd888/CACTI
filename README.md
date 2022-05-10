# CACTI

Implementation of [Actor Model](https://en.wikipedia.org/wiki/Actor_model) in C using POSIX Threads. 

Involved implementing thread pool in C (the thread pool code is not written in seperate file because it is deeply connected to the Actor System). 

Project includes two sample programs. [macierz.c](mn418323/macierz.c) performs sumation of given matrix elements using the Actor Model, [silnia.c](mn418323/silnia.c) calculates n factorial using the Actor System.

The project was written as MIM UW Concurrent Programming course assignment.
