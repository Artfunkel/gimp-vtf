#include "file-vtf.h"

#ifdef _WIN32
#include "windows.h"
#include "resources.h"

void vtf_help(const gchar* help_id, gpointer help_data)
{
	ShellExecute(0,"open","http://developer.valvesoftware.com/wiki/Valve_Texture_Format",0,0,0);
}

void install_locale(gboolean saving)
{
	HRSRC en_mo;
	HANDLE file = INVALID_HANDLE_VALUE;
	char en_mo_out_filepath[MAX_PATH];
	char cd[MAX_PATH];
		
	en_mo = FindResource(NULL, MAKEINTRESOURCE(MO_FILE_EN), MAKEINTRESOURCE(IDR_LOCALISATION) );
	if (en_mo)
	{
		HGLOBAL en_mo_data = LoadResource(NULL,en_mo);
		if (en_mo_data)
		{
			int i;
			char* path_components[3] = { "locale", "en", "LC_MESSAGES" };

			strcpy_s(cd,MAX_PATH,plugin_dir);

			for(i=0; i < 3; i++)
			{
				snprintf(cd,MAX_PATH,"%s\\%s",cd,path_components[i]);
				if ( !CreateDirectory(cd,NULL) && GetLastError() != ERROR_ALREADY_EXISTS )
					break;
			}
			if (i == 3)
			{
				DWORD	file_attr;

				snprintf(en_mo_out_filepath,MAX_PATH,"%s\\file-vtf.mo",cd);

				file_attr = GetFileAttributes(en_mo_out_filepath);

				if ( !(file_attr & FILE_ATTRIBUTE_READONLY) || file_attr == INVALID_FILE_ATTRIBUTES )
				{
					file = CreateFile(en_mo_out_filepath,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);				
					if (file != INVALID_HANDLE_VALUE )
					{
						DWORD size = SizeofResource(NULL,en_mo);
						DWORD written = 0;
						WriteFile(file,LockResource(en_mo_data),size,&written,0);
						CloseHandle(file);
					}
				}
			}
			else
				snprintf(en_mo_out_filepath,MAX_PATH,"%s",cd); // where we failed during directory creation

			if (file == INVALID_HANDLE_VALUE && saving)
			{
				char error_buf[128];
				FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,NULL,GetLastError(),0,error_buf,128,0);
				g_message("Could not extract English language database!\n\nPath: %s\n\nError: %s\n(If you are trying to prevent the file from being overwritten, set the \"read only\" flag.)",en_mo_out_filepath,error_buf);
			}
		}
	}
}

void register_filetype()
{
	HKEY key=0, subkey=0;
		
	LPCSTR type_name = "gimpvtf-settings";
	LPCSTR type_desc = "Gimp VTF plug-in settings";
	LPCSTR icon_path = "shell32.dll,-63011";

	RegCreateKeyEx(HKEY_CURRENT_USER,
					"Software\\Classes\\.vtf-settings", 0,NULL,
					REG_OPTION_NON_VOLATILE,
					KEY_SET_VALUE, NULL,
					&key, NULL);
	RegSetValueEx(key,0,0,REG_SZ,(BYTE*)type_name,(DWORD)strlen(type_name)+1);
	RegCloseKey(key);

	RegCreateKeyEx(HKEY_CURRENT_USER,
					"Software\\Classes\\gimpvtf-settings", 0,NULL,
					REG_OPTION_NON_VOLATILE,
					KEY_SET_VALUE|KEY_CREATE_SUB_KEY, NULL,
					&key, NULL);
	RegSetValueEx(key,0,0,REG_SZ,(BYTE*)type_desc,(DWORD)strlen(type_desc)+1);

	RegCreateKeyEx(key,
					"DefaultIcon", 0,NULL,
					REG_OPTION_NON_VOLATILE,
					KEY_SET_VALUE, NULL,
					&subkey, NULL);
	RegSetValueEx(key,0,0,REG_SZ,(BYTE*)icon_path,(DWORD)strlen(icon_path)+1);
	RegCloseKey(key);
	RegCloseKey(subkey);
}

#endif // _WIN32