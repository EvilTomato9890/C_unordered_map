
#ifndef ASSERTS_H_INCLUDED
#define ASSERTS_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include "colors.h"


/**
 * @brief Превращает часть кода в строку.
 * @param [in] a Кусок кода, который будет преобразован в строку.
 * @return Строчное отображение кода
 * 
 * Макрос берет код, который ему передан и превращает его в строку.
 * 
 * @note В случае попытки перевода функций, может протребоваться дополнительное преобразование
*/
#define stringify(a) #a


/**
 * @brief Аналог assert, завершающий программу.
 * @param [in] test Условие, которое проверяется assert-ом.
 * @param [in] message Сообщение, которое будет выведено.
 * 
 * Макрос проверяет заданное условие и, если оно ложно, выдает сообщение об ошибке, 
 * содержащее имя функции и номер строки кода, где произошел вызол и аргумент, который вызвал этот assert. 
 * После завершает выполнение программы.
 * 
*/
#ifdef NDEBUG
    #define HARD_ASSERT(...) (void)0
#else
    #define HARD_ASSERT(test, message) \
        do {            \
            if(!(test)) { \
                fprintf(stderr, RED_CONSOLE "%s\nERROR WAS OCCURED IN %s:%i LINE FROM %s BY %s\n" RESET_CONSOLE,  message,__FILE__, __LINE__, __PRETTY_FUNCTION__, stringify(test)); \
                abort(); \
            }   \
        } while(0)
#endif


/**
 * @brief Аналог assert, НЕ завершающий программу.
 * @param [in] test Условие, которое проверяется assert-ом.
 * @param [in] message Сообщение, которое будет выведено.
 * 
 * Макрос проверяет заданное условие и, если оно ложно, выдает сообщение об ошибке, 
 * содержащее имя функции и номер строки кода, где произошел вызол и аргумент, который вызвал этот assert. 
 * После выполнения НЕ завершает выполнение программы.
 * 
*/
#ifdef NDEBUG
    #define SOFT_ASSERT(...) (void)0
#else
    #define SOFT_ASSERT(test, message) \
        do {        \
            if(!(test)) { \
                fprintf(stderr, RED_CONSOLE "%s\nERROR WAS OCCURED IN %s IN %i LINE FROM %s BY %s" RESET_CONSOLE "\n", \
                    message,__FILE__, __LINE__, __PRETTY_FUNCTION__, stringify(test)); \
            }  \
        } while(0)
#endif


/**
 * @brief Аналог assert, НЕ завершающий программу и выполняющий некоторый код.
 * @param [in] test Условие, которое проверяется assert-ом.
 * @param [in] message Сообщение, которое будет выведено.
 * @param [in] command Фрагмент кода, который необходимо выполнить.
 * 
 * Макрос проверяет заданное условие и, если оно ложно, выдает сообщение об ошибке, 
 * содержащее имя функции и номер строки кода, где произошел вызол и аргумент, который вызвал этот assert. 
 * Также, если условие ложно, выполняет переданный фрагмент кода.
 * После выполнения НЕ завершает выполнение программы.
 * 
*/
#ifdef NDEBUG
    #define SOFT_ASSERT_FUCTIONAL(...) (void)0
#else 
    #define SOFT_ASSERT_FUCTIONAL(test, message, command) \
        do { \
            if(!(test)) { \
                printf(RED "%s\nERROR WAS OCCURED IN %s IN %i LINE FROM %s BY %s" RESET "\n", \
                    message,__FILE__, __LINE__, __PRETTY_FUNCTION__, stringify(test)); \
                command; \
            }  \
        } while(0)
#endif

#endif