/* date = November 21st 2020 0:46 am */

#ifndef INCOGNITAS_TYPES_H
#define INCOGNITAS_TYPES_H

static Arena global_incognitas_arena;

struct Doyle_Project_Command_Lister_Result
{
    i32 index;
    b32 success;
};

struct Doyle_Project_Description
{
    b32 initialized;
    String_Const_u8 name;
    String_Const_u8 path;
};



#endif //INCOGNITAS_TYPES_H
