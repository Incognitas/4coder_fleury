#include <cstdlib>

static Doyle_Project_Description* gs_all_doyle_projects = nullptr;
static i32 gs_currentlySelectedProject = -1;



static String_Const_u8 internal_file_dump(Arena *arena, char *name) {
    String_Const_u8 text = {};
    FILE *file = fopen(name, "rb");
    if (file != 0) {
        fseek(file, 0, SEEK_END);
        text.size = ftell(file);
        fseek(file, 0, SEEK_SET);
        text.str = push_array(arena, u8, text.size + 1);
        if (text.str == 0) {
            fprintf(stdout,
                    "fatal error: not enough memory in partition for file dumping");
            exit(1);
        }
        fread(text.str, 1, (size_t)text.size, file);
        text.str[text.size] = 0;
        fclose(file);
    }
    return (text);
}



static Doyle_Project_Command_Lister_Result
choose_doyle_project(Application_Links *app, Doyle_Project_Description* projects,  String_Const_u8 query){
    Doyle_Project_Command_Lister_Result result = {};
    if (projects != nullptr){
        Scratch_Block scratch(app);
        Lister_Block lister(app, scratch);
        lister_set_query(lister, query);
        lister_set_default_handlers(lister);
        
        i32 current=0;
        
        while(projects != nullptr && projects->initialized)
        {
            lister_add_item(lister, projects->name, projects->path, IntAsPtr(current), 0);
            ++current;
            projects++;
        }
        
        Lister_Result l_result = run_lister(app, lister);
        if (!l_result.canceled){
            result.success = true;
            result.index = (i32)PtrAsInt(l_result.user_data);
        }
    }
    
    return(result);
}


static void do_full_doyle_projects_loading(Async_Context *actx, String_Const_u8 data) {
    Application_Links *app = actx->app;
    Arena *arena = &global_incognitas_arena;
    Scratch_Block scratch(app);
    
    
    ProfileScope(app, "async doyle projects loading");
    
    String_Const_u8 doyle_workspaces = def_get_config_string(scratch, vars_save_string_lit("doyle_root_workspaces_dir"));
    
    // fetch all directories provided in the configuration value
    // incognitas_global_config.doyle_root_workspaces_dirs
    List_String_Const_u8 allentries = string_split(
                                                   arena, doyle_workspaces,
                                                   (u8*)";",
                                                   1);
    
    // NOTE(Aurelien): step 1 : fetch all directories recursively for yaml files
    List_String_Const_u8 allfilesfound{};
    
    for (Node_String_Const_u8 *node = allentries.first;
         node != nullptr; node = node->next) {
        find_files_pattern_match__recursive_directories_only(app, node->string,
                                                             string_u8_litexpr("project.yaml"),
                                                             &allfilesfound, true);
    }
    
    // bettrer check a little bit for consistency
    if( allfilesfound.node_count > 0)
    {
        // allocate enough space in the area (and deallocate beforehand if necessary)
        if( gs_all_doyle_projects )
        {
            pop_array(arena, Doyle_Project_Description, allfilesfound.node_count + 1);
            gs_all_doyle_projects = nullptr;
        }
        
        // NOTE(Aurelien): allocate memory for entries
        Doyle_Project_Description* projectslist = push_array_zero(arena, Doyle_Project_Description, allfilesfound.node_count + 1);
        
        Doyle_Project_Description* currentDoyleProject = projectslist;
        
        // NOTE(Aurelien): step 2 : add associated project name to the list of projects
        for (Node_String_Const_u8 *node = allfilesfound.first;
             node != nullptr; node = node->next) {
            
            String_Const_u8 full_path = push_u8_stringf(scratch, "%.*sproject.yaml",
                                                        string_expand(node->string));
            
            String_Const_u8 content =  internal_file_dump(scratch,
                                                          reinterpret_cast<char *>(full_path.str));
            
            // parse project.yaml and extract the project name from it
            const String_Const_u8 pattern = string_u8_litexpr("name:");
            i64 foundpos = string_find_first(content, pattern);
            // make sure we skip also the size of the pattern we are searching
            foundpos += pattern.size;
            
            // go to the given pos
            content = string_skip(content, foundpos);
            
            content = string_skip_whitespace(content);
            // NOTE(Aurelien): We have skipped all previous chars to get there, so reset foundpos to 0
            foundpos = 0;
            // NOTE(Aurelien): at this point we shall be located right before the project name
            const i64 endpos = string_find_first(content, foundpos, u8('\n'));
            
            
            String_Const_u8 projectname = string_substring(content, Range_i64{foundpos, endpos + 1});
            // add null character at the end (and ignore last spaces/carriage return)
            while(character_is_whitespace(projectname.str[projectname.size]))
            {
                --projectname.size;
            }
            projectname.str[++projectname.size] = 0;
            
            
            // NOTE(Aurelien): project name if extracted from file content which is in the scratchpad
            // so we have to make a copy of it into the arena
            currentDoyleProject->name = push_string_copy(arena, projectname);
            currentDoyleProject->path = node->string;
            currentDoyleProject->initialized = true;
            // go to next occurrence
            ++currentDoyleProject;
        }
        
        // NOTE(Aurelien): At the end, assign the newly created list to the global variable
        gs_all_doyle_projects = projectslist;
    }
}

static void switch_to_doyle_project_dir(Application_Links* app, Doyle_Project_Description* curProj)
{
    set_hot_directory(app, curProj->path);
}

static void open_doyle_project_files(Application_Links* app, Doyle_Project_Description* curproj)
{
    Scratch_Block scratch(app);
    
    Prj_Pattern_List blacklist = prj_get_standard_blacklist(scratch);
    // NOTE(Aurelien): handle blacklist first
    String8 treat_as_code = def_get_config_string(scratch, vars_save_string_lit("treat_as_code"));
    // NOTE(Aurelien): then whitelist (based on global configuration)
    String8Array extensions = parse_extension_line_to_extension_list(app, scratch, treat_as_code);
    Prj_Pattern_List whitelist = prj_pattern_list_from_extension_array(scratch, extensions);
    
    // manage the app dir if any
    {
        
        String_Const_u8 appdir = push_u8_stringf(scratch, "%.*sapp/", string_expand(curproj->path));
        prj_open_files_pattern_filter__rec(app, appdir, whitelist, blacklist, PrjOpenFileFlag_Recursive);
    }
    // manage the modules dir if any
    {
        String_Const_u8 modulesdir = push_u8_stringf(scratch, "%.*smodules/", string_expand(curproj->path));
        prj_open_files_pattern_filter__rec(app, modulesdir, whitelist, blacklist, PrjOpenFileFlag_Recursive);
    }
    
    // load project.yaml also
    {
        View_ID curview =  get_active_view(app, Access_Always);
        
        String_Const_u8 full_path = push_u8_stringf(scratch, "%.*sproject.yaml",
                                                    string_expand(curproj->path));
        
        Buffer_ID createdBuffer =  create_buffer(app, full_path, 0);
        
        // and make it the active buffer
        view_set_buffer(app, curview, createdBuffer, 0);
    }
    
}

enum
{
    DoyleBuildConfguration_NULL = 0,
    DoyleBuildConfguration_Debug = 1,
    DoyleBuildConfguration_Release = 2,
};

internal void
do_gui_choose_configuration(Application_Links* app, View_ID view)
{
    if( gs_currentlySelectedProject != -1)
    {
        String_Const_u8 hot_directory = gs_all_doyle_projects[gs_currentlySelectedProject].path;
        
        Scratch_Block scratch(app);
        Lister_Choice_List list = {};
        
        Buffer_ID buffer = get_comp_buffer(app);
        lister_choice(scratch, &list, "(R)elease", "", KeyCode_Y, DoyleBuildConfguration_Release);
        lister_choice(scratch, &list, "(D)ebug", "", KeyCode_N, DoyleBuildConfguration_Debug);
        
        
        Lister_Choice* choice = get_choice_from_user(app, "Choose build configuration", list);
        String_Const_u8 finalCmd{};
        
        if (choice != nullptr)
        {
            switch (choice->user_data)
            {
                case DoyleBuildConfguration_Debug:
                {
                    finalCmd = string_u8_litexpr("doyle build setup --debug --update");
                }
                break;
                
                case DoyleBuildConfguration_Release:
                {
                    finalCmd = string_u8_litexpr("doyle build setup --release --update");
                }
                break;
                
                default:
                break;
            }
        }
        
        if( finalCmd.size > 0)
        {
            (void)get_or_open_build_panel(app);
            set_fancy_compilation_buffer_font(app);
            
            exec_system_command(app, view, buffer_identifier(buffer), hot_directory, finalCmd, CLI_OverlapWithConflict|CLI_CursorAtEnd|CLI_SendEndSignal);
            
            lock_jump_buffer(app, buffer);
            
        }
    }
    else
    {
        print_message(app, string_u8_litexpr( "No Doyle project selected. Selection unavailable\n"));
    }
}


CUSTOM_COMMAND_SIG(incognitas_doyle_open_project)
CUSTOM_DOC("Searches for all project in provided search paths and displays them ") 
{
    Doyle_Project_Description* currentProject = gs_all_doyle_projects;
    
    // fetch all entries in gs_all_doyle_projects and list all projects
    if(currentProject != nullptr)
    {
        Doyle_Project_Command_Lister_Result result = choose_doyle_project(app, currentProject,string_u8_litexpr("Project:"));
        
        if( result.success)
        {
            gs_currentlySelectedProject = result.index;
            // selected project is at provided index
            currentProject = &gs_all_doyle_projects[gs_currentlySelectedProject];
            
            // change directory to the directory of the given project.yaml
            switch_to_doyle_project_dir(app, currentProject);
            
            // fetch all files with .h or .cxx extension in the  app/modules folder and load them
            open_doyle_project_files(app, currentProject);
            
            // start hacking from here on out
            // TODO(Aurelien): Configure build commands so that
            // Add custom command to switch between doyle default config being debug or release
            // - using F1 will call doyle build setup -u [--debug if debug by default]
            // - using F2 will try to build the project using CMake + ninja + VS2013 on window, CMake + gcc on Linux 
            // NOTE(Aurelien): Note that the result of thos operation shall be displayed on the **Build** panel
        }
    }
    else
    {
        print_message(app, string_u8_litexpr("Doyle database still updating. Be patient ...\n"));
    }
}



CUSTOM_COMMAND_SIG(incognitas_doyle_configure_project)
CUSTOM_DOC("Configures project using appropriate doyle command") 
{
    const View_ID view = get_active_view(app, Access_ReadWriteVisible);
    
    do_gui_choose_configuration(app, view);
}