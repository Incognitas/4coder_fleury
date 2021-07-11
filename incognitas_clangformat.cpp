/*
4coder_experiments.cpp - Supplies extension bindings to the
defaults with experimental new features.
*/

// TOP

#include <sys/stat.h>
#include <string.h>
#include <thread>
#include <cstdio>
#include <array>

#define internal static

enum
{
    ClangFormat_NULL = 0,
    ClangFormat_Cancel = 1,
    ClangFormat_DiscardChanges = 2,
    ClangFormat_SaveBefore = 3,
};

internal void
clang_format_buffer(Application_Links* app, Buffer_ID buffer, Arena* arena)
{
    
    String_Const_u8 hot_directory = push_hot_directory(app, arena);
    String_Const_u8 buffer_filename = push_buffer_file_name(app, arena, buffer);
    
    String_Const_u8 cmd = push_u8_stringf(arena, "cmd /c clang-format -i \"%s\"", buffer_filename.str);
    
    std::array<const char*, 3> args={R"(C:\Program Files\LLVM\bin\clang-format.exe)", "-i", reinterpret_cast<const char*>(buffer_filename.str)};
    
    const intptr_t retval = _spawnl(_P_WAIT, args[0], args[0], args[1], args[2], nullptr);
    // check stats
    /*struct stat filestats;
    memset(&filestats, 0L, sizeof(filestats));
    
    stat(reinterpret_cast<const char*>(buffer_filename.str), &filestats);
    const auto curtime = filestats.st_mtime;
    
    if (!exec_system_command(app, 0, buffer_identifier(0), hot_directory, cmd, 0))
    {
        print_message(app, push_u8_stringf(arena, "Something went wrong !"));
    }
    
    size_t cpt{ 0 };
    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        stat(reinterpret_cast<const char*>(buffer_filename.str), &filestats);
        ++cpt;
    } while (filestats.st_mtime == curtime && cpt < 10);
    */
    
    if( retval != 0)
    {
        print_message(app, push_u8_stringf(arena, "Something went wrong !\n"));
    }
    else
    {
        // reopen the updated buffer
        buffer_reopen(app, buffer, 0);
    }
}

internal void
do_gui_sure_to_format_unsaved_file(Application_Links* app, Buffer_ID buffer, Arena* arena)
{
    
    Lister_Choice_List list = {};
    lister_choice(arena, &list, "(C)ancel", "", KeyCode_C, ClangFormat_Cancel);
    lister_choice(arena, &list, "(N)o [discard changes]", "", KeyCode_N, ClangFormat_DiscardChanges);
    lister_choice(arena, &list, "(S)ave and format", "", KeyCode_S, ClangFormat_SaveBefore);
    
    Lister_Choice* choice = get_choice_from_user(app, "There are unsaved changes, save before ?", list);
    
    if (choice != nullptr)
    {
        switch (choice->user_data)
        {
            case ClangFormat_SaveBefore:
            {
                
                // save buffer if the user accepts it before applying clang format
                buffer_save(app, buffer, push_buffer_file_name(app, arena, buffer), BufferSave_IgnoreDirtyFlag);
            }
            //[[fallthrough]]
            case ClangFormat_DiscardChanges:
            {
                clang_format_buffer(app, buffer, arena);
            }
            break;
            
            case ClangFormat_Cancel:
            default:
            {
                // just do nothing
            }
            break;
            
            break;
        }
    }
}

CUSTOM_COMMAND_SIG(incognitas_clang_format)
CUSTOM_DOC("Applies clang-format on the active buffer and reload the file after transformation.")
{
    
    const uint32_t access = Access_ReadWriteVisible;
    const View_ID view = get_active_view(app, access);
    Buffer_ID buffer = view_get_buffer(app, view, access);
    // retrieve active buffer to check if it has unsaved
    // changes or not
    
    b32 has_unsaved_changes = false;
    const Dirty_State dirty = buffer_get_dirty_state(app, buffer);
    
    if (dirty == DirtyState_UnsavedChanges)
    {
        has_unsaved_changes = true;
    }
    
    Scratch_Block scratch(app);
    
    if (has_unsaved_changes)
    {
        do_gui_sure_to_format_unsaved_file(app, buffer, scratch);
    }
    else
    {
        clang_format_buffer(app, buffer, scratch);
    }
}

// BOTTOM