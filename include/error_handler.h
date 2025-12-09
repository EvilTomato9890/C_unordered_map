#ifndef ERROR_HANDLER_H_INCLUDED
#define ERROR_HANDLER_H_INCLUDED

#include "logger.h"

enum error_t {
    HM_ERR_OK,
    HM_ERR_MEM_ALLOC,         
    HM_ERR_FULL,        
    HM_ERR_BAD_ARG,     
    HM_ERR_NOT_FOUND,  
    HM_ERR_INTERNAL 
};
 
#define RETURN_IF_ERROR(error, ...)                        \
    do {                                                   \
        if((int)(error) != 0) {                                 \
        LOGGER_ERROR("ERROR: %d happend", error);          \
        __VA_ARGS__;                                       \
        return error;                                      \
        }                                                  \
    } while (0)

#endif