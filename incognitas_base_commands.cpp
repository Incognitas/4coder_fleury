
  static b32 is_doyle_project(Application_Links*app, String_Const_u8 file_name, Arena* arena)
{
    b32 status{false};
    Temp_Memory restore_point = begin_temp(arena);
    String_Const_u8 project_path = push_hot_directory(app, arena);
    
    if (project_path.size == 0){
        print_message(app, string_u8_litexpr("The hot directory is empty, cannot search for a project.\n"));
    }
    else
    {
        String_Const_u8 start_path = string_remove_last_folder(file_name);
        
        String_Const_u8 full_path = push_file_search_up_path(app, arena, start_path, string_u8_litexpr("project.yaml"));
        
        
        status = full_path.size > 0;
    }
    end_temp(restore_point);
    // fetch all parent directories of this file 
    return status;
}


static void save_and_clangformat(Application_Links* app, Buffer_ID buffer, String_Const_u8 file_name, Arena* arena)
{
    buffer_save(app, buffer, file_name, 0);
    
    // NOTE(Aurelien): At this point, we want to apply clang-format on the file if we are in a doyle project
    // this shall be the default behavior
    if(is_doyle_project(app, file_name, arena))
    {
        print_message(app, push_u8_stringf(arena, 
                                           "Auto formatting file '%.*s' using clang-format (because it belongs to a doyle project\n", string_expand(file_name)));
        clang_format_buffer(app, buffer, arena);
    }
}

CUSTOM_COMMAND_SIG(incognitas_save)
CUSTOM_DOC("Saves the current buffer.")
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    Scratch_Block scratch(app);
    String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer);
    
    save_and_clangformat(app, buffer, file_name, scratch);
}

function void
save_and_clangformat_all_dirty_buffers_with_postfix(Application_Links *app, String_Const_u8 postfix){
    ProfileScope(app, "save and maybe clang-format all dirty buffers");
    Scratch_Block scratch(app);
    for (Buffer_ID buffer = get_buffer_next(app, 0, Access_ReadWriteVisible);
         buffer != 0;
         buffer = get_buffer_next(app, buffer, Access_ReadWriteVisible)){
        Dirty_State dirty = buffer_get_dirty_state(app, buffer);
        if (dirty == DirtyState_UnsavedChanges)
        {
            Temp_Memory temp = begin_temp(scratch);
            String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer);
            if (string_match(string_postfix(file_name, postfix.size), postfix)){
                save_and_clangformat(app, buffer, file_name, scratch);
            }
            end_temp(temp);
        }
    }
}

CUSTOM_COMMAND_SIG(incognitas_save_all_dirty_buffers)
CUSTOM_DOC("Saves all buffers marked dirty (showing the '*' indicator).")
{
    String_Const_u8 empty = {};
    save_and_clangformat_all_dirty_buffers_with_postfix(app, empty);
}
