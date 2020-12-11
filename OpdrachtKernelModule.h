#ifndef QUERY_IOCTL_H
#define QUERY_IOCTL_H
#include <linux/ioctl.h>
 
typedef struct
{
    int edges, toggleSpeed;
} query_arg_t;
 
#define GET_EDGES _IOR('x', 1, query_arg_t *)
#define CLEAR_EDGES _IO('x', 2)
#define SET_TOGGLE_SPEED _IOW('x', 3, query_arg_t *)
 
#endif
