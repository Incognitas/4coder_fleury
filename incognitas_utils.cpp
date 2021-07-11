function b32
match_pattern(String_Const_u8 string, String_Const_u8 pattern){
    
    // NOTE(Aurelien): old version used wildcard matching, but it is not needed as it is a simple
    // string comparison
    
    //List_String_Const_u8 t = string_split_wildcards(&global_incognitas_arena, pattern);
    //return static_cast<b32>(string_wildcard_match(t, string));
    
    return string_compare(string, pattern) == 0;
}


function void
find_files_pattern_match__recursive(Application_Links *app, String_Const_u8 path,
                                    String_Const_u8 pattern,
                                    List_String_Const_u8 *list,
                                    b32 stopAtDepthOfFirstEncounter){
    Scratch_Block scratch(app);
    
    ProfileScopeNamed(app, "get file list with pattern", profile_get_file_list);
    File_List contentlist = system_get_file_list(scratch, path);
    ProfileCloseNow(profile_get_file_list);
    
    File_Info **info = contentlist.infos;
    for (u32 i = 0; i < contentlist.count; ++i, ++info){
        String_Const_u8 file_name = (**info).file_name;
        
        if (HasFlag((**info).attributes.flags, FileAttribute_IsDirectory)){
            
            String_Const_u8 new_path = push_u8_stringf(scratch, "%.*s/%.*s/",
                                                       string_expand(path),
                                                       string_expand(file_name));
            find_files_pattern_match__recursive(app, new_path, pattern, list, stopAtDepthOfFirstEncounter);
        }
        else{ 
            if (!match_pattern(file_name, pattern)){
                continue;
            }
            
            
            String_Const_u8 full_path = push_u8_stringf(&global_incognitas_arena, "%.*s%.*s",
                                                        string_expand(path),
                                                        string_expand(file_name));
            
            string_list_push(&global_incognitas_arena, list, full_path);
        }
    }
}

function void
find_files_pattern_match__recursive_directories_only(Application_Links *app, String_Const_u8 path,
                                                     String_Const_u8 pattern,
                                                     List_String_Const_u8 *list,
                                                     b32 stopAtDepthOfFirstEncounter)
{
    Scratch_Block scratch(app);
    
    ProfileScopeNamed(app, "get dirs list with pattern", profile_get_file_list);
    File_List contentlist = system_get_file_list(scratch, path);
    ProfileCloseNow(profile_get_file_list);
    
    File_Info **info = contentlist.infos;
    for (u32 i = 0; i < contentlist.count; ++i, ++info)
    {
        String_Const_u8 file_name = (**info).file_name;
        
        if (HasFlag((**info).attributes.flags, FileAttribute_IsDirectory))
        {
            
            String_Const_u8 new_path;
            
            if( !character_is_slash(path.str[path.size - 1]) )
            {
                new_path = push_u8_stringf(scratch, "%.*s/%.*s/",
                                           string_expand(path),
                                           string_expand(file_name));
            }
            else
            {
                new_path = push_u8_stringf(scratch, "%.*s%.*s/",
                                           string_expand(path),
                                           string_expand(file_name));
            }
            
            find_files_pattern_match__recursive_directories_only(app, new_path, pattern, list, stopAtDepthOfFirstEncounter);
        }
        else
        { 
            if (!match_pattern(file_name, pattern)){
                continue;
            }
            
            
            String_Const_u8 full_path = push_u8_stringf(&global_incognitas_arena, "%.*s",
                                                        string_expand(path));
            
            string_list_push(&global_incognitas_arena, list, full_path);
        }
    }
}
